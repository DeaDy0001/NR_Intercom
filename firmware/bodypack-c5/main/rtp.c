#include <stdio.h>
#include <math.h>
#include <string.h>
#include "rtp.h"
#include "audio.h"
#include "settings.h"
#include "net.h"
#include "opus.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"

static const char *TAG = "rtp";

#define JB_SLOTS        32
#define MAX_PAYLOAD     500                 // L16 5 ms = 480 Byte, Opus deutlich weniger
#define MAX_FRAME_SAMP  1920                // 40 ms bei 48 kHz (Decoder-Ausgabe)
#define MAX_TX_SAMP     960                 // 20 ms
#define DSCP_EF_TOS     0xB8                // DSCP 46 (Expedited Forwarding) -> WMM Voice

typedef struct {
    bool     used;
    uint8_t  pt;
    uint16_t seq;
    uint16_t nsamp;
    uint16_t len;
    uint8_t  data[MAX_PAYLOAD];
} jb_slot_t;

static int s_sock = -1;
static struct sockaddr_in s_dst;
static volatile bool s_dst_valid;
static volatile bool s_codec_dirty = true;
static SemaphoreHandle_t s_lock;
static jb_slot_t s_jb[JB_SLOTS];
static bool     s_playing;
static uint16_t s_next_seq;
static bool     s_have_seq;
static uint16_t s_last_n = 480;
static uint8_t  s_last_pt = RTP_PT_OPUS;
static int16_t  s_l16_last[MAX_FRAME_SAMP];
static rtp_stats_t s_st;
static float    s_jitter;            // in Samples (RFC 3550)
static int64_t  s_prev_transit;
static bool     s_have_transit;
static uint64_t s_enc_sum, s_dec_sum;
static uint32_t s_enc_n, s_dec_n;
static TaskHandle_t s_tx_task;
static OpusDecoder *s_dec;
static uint16_t s_tx_seq;
static uint32_t s_tx_ts, s_tx_ssrc;
static uint32_t s_rx_ssrc;
static bool     s_rx_ssrc_valid;
static int64_t  s_rx_last_us;
static float    s_rx_peak;
static bool     s_dec_idle;                  // letztes Paket war Stille (nicht dekodiert)
static uint32_t s_cand_ssrc;                 // moeglicher neuer Strom
static int      s_cand_n;

void rtp_update_target(void)
{
    s_dst_valid = false;
    if (!g_set.host[0]) return;
    memset(&s_dst, 0, sizeof(s_dst));
    s_dst.sin_family = AF_INET;
    s_dst.sin_port = htons(g_set.port);
    if (inet_pton(AF_INET, g_set.host, &s_dst.sin_addr) != 1) {
        ESP_LOGW(TAG, "ungueltige Ziel-IP '%s'", g_set.host);
        return;
    }
    s_dst_valid = true;
    ESP_LOGI(TAG, "RTP-Ziel: %s:%u", g_set.host, g_set.port);
}

void rtp_set_target(const char *ip, uint16_t port)
{
    s_dst_valid = false;
    memset(&s_dst, 0, sizeof(s_dst));
    s_dst.sin_family = AF_INET;
    s_dst.sin_port = htons(port);
    if (inet_pton(AF_INET, ip, &s_dst.sin_addr) != 1) return;
    s_dst_valid = true;
    ESP_LOGI(TAG, "RTP-Ziel (Intercom): %s:%u", ip, port);
}

void rtp_target_str(char *out, int max)
{
    if (!s_dst_valid) { snprintf(out, max, "-"); return; }
    char ip[16];
    inet_ntop(AF_INET, &s_dst.sin_addr, ip, sizeof(ip));
    snprintf(out, max, "%s:%u", ip, ntohs(s_dst.sin_port));
}

uint32_t rtp_get_ssrc(void)
{
    return s_tx_ssrc;
}

void rtp_codec_changed(void)
{
    s_codec_dirty = true;
}

void rtp_get_stats(rtp_stats_t *st)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *st = s_st;
    int depth = 0;
    for (int i = 0; i < JB_SLOTS; i++) {
        if (s_jb[i].used) depth += s_jb[i].nsamp;
    }
    st->jb_depth_ms = depth * 1000 / AUDIO_RATE;
    st->rx_peak_db = s_rx_peak > 1e-6f ? 20.0f * log10f(s_rx_peak) : -120.0f;
    st->jitter_ms = s_jitter * 1000.0f / AUDIO_RATE;
    st->enc_us_avg = s_enc_n ? (uint32_t)(s_enc_sum / s_enc_n) : 0;
    st->dec_us_avg = s_dec_n ? (uint32_t)(s_dec_sum / s_dec_n) : 0;
    xSemaphoreGive(s_lock);
    st->codec_stack_free = s_tx_task ? uxTaskGetStackHighWaterMark(s_tx_task) * sizeof(StackType_t) : 0;
}

void rtp_reset_stats(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    memset(&s_st, 0, sizeof(s_st));
    s_enc_sum = s_dec_sum = 0;
    s_enc_n = s_dec_n = 0;
    xSemaphoreGive(s_lock);
}

static void jb_reset_locked(void)
{
    for (int i = 0; i < JB_SLOTS; i++) s_jb[i].used = false;
    s_playing = false;
    s_have_seq = false;
}

static int tx_frame_samples(void)
{
    int f = g_set.frame_ms;
    if (g_set.codec == CODEC_L16) {
        f = 5;                                 // L16 nur 5 ms (RAM: Jitterbuffer-Slots sind 500 Byte)
    } else if (f != 5 && f != 10 && f != 20) {
        f = 10;
    }
    return f * AUDIO_RATE / 1000;
}

// ---------------------------------------------------------------- Senden
static OpusEncoder *create_encoder(void)
{
    int err;
    int app = g_set.opus_voip ? OPUS_APPLICATION_VOIP : OPUS_APPLICATION_RESTRICTED_LOWDELAY;
    OpusEncoder *enc = opus_encoder_create(AUDIO_RATE, 1, app, &err);
    if (!enc) {
        ESP_LOGE(TAG, "Opus-Encoder: %s", opus_strerror(err));
        return NULL;
    }
    opus_encoder_ctl(enc, OPUS_SET_BITRATE(g_set.opus_kbps * 1000));
    opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(g_set.opus_cplx > OPUS_CPLX_MAX ? OPUS_CPLX_MAX : g_set.opus_cplx));
    opus_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl(enc, OPUS_SET_VBR(1));
    opus_encoder_ctl(enc, OPUS_SET_VBR_CONSTRAINT(1));
    opus_encoder_ctl(enc, OPUS_SET_DTX(0));
    opus_encoder_ctl(enc, OPUS_SET_INBAND_FEC(g_set.opus_voip ? 1 : 0));
    opus_encoder_ctl(enc, OPUS_SET_PACKET_LOSS_PERC(g_set.opus_voip ? 5 : 0));
    ESP_LOGI(TAG, "Opus-Encoder: %d kbit/s, Komplexitaet %d, %s, %d ms",
             g_set.opus_kbps, g_set.opus_cplx, g_set.opus_voip ? "voip+FEC" : "lowdelay",
             tx_frame_samples() * 1000 / AUDIO_RATE);
    return enc;
}

// Ein Codec-Task fuer Encoder UND Decoder: spart einen zweiten grossen Stack (kein PSRAM).
static OpusEncoder *s_enc;

static void encode_and_send(int16_t *pcm, int frame)
{
    static uint8_t pkt[12 + MAX_TX_SAMP * 2];
    int plen;
    uint8_t pt;
    if (g_set.codec == CODEC_OPUS && s_enc) {
        int64_t t0 = esp_timer_get_time();
        plen = opus_encode(s_enc, pcm, frame, pkt + 12, sizeof(pkt) - 12);
        uint32_t dt = (uint32_t)(esp_timer_get_time() - t0);
        s_enc_sum += dt; s_enc_n++;
        if (dt > s_st.enc_us_max) s_st.enc_us_max = dt;
        if (plen < 0) { ESP_LOGW(TAG, "opus_encode: %s", opus_strerror(plen)); return; }
        pt = RTP_PT_OPUS;
    } else {
        for (int i = 0; i < frame; i++) {          // L16 = Big Endian
            pkt[12 + 2 * i] = (uint16_t)pcm[i] >> 8;
            pkt[13 + 2 * i] = (uint16_t)pcm[i] & 0xFF;
        }
        plen = frame * 2;
        pt = RTP_PT_L16;
    }
    pkt[0] = 0x80;                                 // V=2
    pkt[1] = pt;
    pkt[2] = s_tx_seq >> 8; pkt[3] = s_tx_seq & 0xFF;
    pkt[4] = s_tx_ts >> 24; pkt[5] = s_tx_ts >> 16; pkt[6] = s_tx_ts >> 8; pkt[7] = s_tx_ts;
    pkt[8] = s_tx_ssrc >> 24; pkt[9] = s_tx_ssrc >> 16; pkt[10] = s_tx_ssrc >> 8; pkt[11] = s_tx_ssrc;
    s_tx_seq++;
    s_tx_ts += frame;                              // Opus-RTP-Takt ist immer 48 kHz

    if (!s_dst_valid || s_sock < 0) return;
    if (!net_is_connected() && s_dst.sin_addr.s_addr != htonl(INADDR_LOOPBACK)) return;
    int r = sendto(s_sock, pkt, 12 + plen, 0, (struct sockaddr *)&s_dst, sizeof(s_dst));
    if (r > 0) { s_st.tx_packets++; s_st.tx_bytes += r; }
}

// ---------------------------------------------------------------- Empfangen
static void rx_task(void *arg)
{
    static uint8_t buf[1500];
    while (1) {
        int len = recv(s_sock, buf, sizeof(buf), 0);
        if (len < 12 || (buf[0] >> 6) != 2) continue;
        int cc = buf[0] & 0x0F;
        int hdr = 12 + 4 * cc;
        if (buf[0] & 0x10) {                       // Header-Extension
            if (len < hdr + 4) continue;
            hdr += 4 + 4 * ((buf[hdr + 2] << 8) | buf[hdr + 3]);
        }
        int plen = len - hdr;
        if (buf[0] & 0x20) plen -= buf[len - 1];   // Padding
        if (plen <= 0) continue;
        if (plen > MAX_PAYLOAD) { s_st.rx_bad++; continue; }   // z. B. L16 mit mehr als 5 ms
        const uint8_t pt = buf[1] & 0x7F;
        if (pt >= 64 && pt <= 95) { s_st.rx_rtcp++; continue; }   // RTCP (SR/RR/SDES/BYE) ueber rtcpMux
        // Nur EINEN Strom abspielen: Kommen gleichzeitig zwei Stroeme (andere SSRC) an, wuerde der
        // Jitterbuffer bei jedem Paket neu synchronisieren und nichts mehr ausgeben.
        const uint32_t ssrc = ((uint32_t)buf[8] << 24) | (buf[9] << 16) | (buf[10] << 8) | buf[11];
        const int64_t now_us = esp_timer_get_time();
        // Wechsel auf einen neuen Strom (z. B. nach mixerRebuilt): sofort, wenn der alte 300 ms still ist,
        // oder schon nach 3 Paketen des neuen in Folge ohne ein Paket des alten dazwischen (~30 ms).
        // Kommen zwei Stroeme verschraenkt an, setzt jedes Paket des alten den Zaehler zurueck.
        if (ssrc != s_rx_ssrc) {
            if (ssrc == s_cand_ssrc) s_cand_n++;
            else { s_cand_ssrc = ssrc; s_cand_n = 1; }
            bool old_silent = !s_rx_ssrc_valid || now_us - s_rx_last_us >= 300000;
            if (!old_silent && s_cand_n < 3) { s_st.rx_foreign++; continue; }
            s_rx_ssrc = ssrc;
            if (s_rx_ssrc_valid) s_st.rx_ssrc_changes++;
            s_rx_ssrc_valid = true;
            s_st.rx_ssrc = ssrc;
            s_cand_n = 0;
        } else {
            s_cand_n = 0;                          // alter Strom lebt noch -> Kandidat verwerfen
        }
        s_rx_last_us = now_us;
        const uint8_t *payload = buf + hdr;
        int nsamp;
        if (pt == RTP_PT_L16) {
            nsamp = plen / 2;
            if (nsamp > MAX_FRAME_SAMP) nsamp = MAX_FRAME_SAMP;
        } else {
            // alles andere als Opus behandeln (mediasoup vergibt PT dynamisch, z. B. 100)
            nsamp = opus_packet_get_nb_samples(payload, plen, AUDIO_RATE);
            if (nsamp <= 0 || nsamp > MAX_FRAME_SAMP) { s_st.rx_bad++; continue; }
        }
        uint16_t seq = (buf[2] << 8) | buf[3];
        uint32_t ts = ((uint32_t)buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];

        // Jitter nach RFC 3550 (in Samples)
        int64_t arrival = esp_timer_get_time() * AUDIO_RATE / 1000000;
        int64_t transit = arrival - (int64_t)ts;
        if (s_have_transit) {
            int64_t d = transit - s_prev_transit;
            if (d < 0) d = -d;
            if (d < AUDIO_RATE) s_jitter += ((float)d - s_jitter) / 16.0f;
        }
        s_prev_transit = transit;
        s_have_transit = true;

        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_st.rx_packets++;
        s_st.rx_payload_bytes += plen;
        if (!s_have_seq) {
            s_next_seq = seq;
            s_have_seq = true;
        }
        int16_t ahead = (int16_t)(seq - s_next_seq);
        if (ahead < -JB_SLOTS || ahead >= 4 * JB_SLOTS) {
            // grosser Sprung (Sender neu gestartet / mixerRebuilt): neu synchronisieren
            jb_reset_locked();
            s_next_seq = seq;
            s_have_seq = true;
            ahead = 0;
            s_st.rx_resyncs++;
        }
        if (ahead < 0) {
            s_st.rx_late++;
        } else if (ahead < JB_SLOTS) {
            jb_slot_t *sl = &s_jb[seq % JB_SLOTS];
            sl->used = true;
            sl->pt = pt;
            sl->seq = seq;
            sl->nsamp = nsamp;
            sl->len = plen;
            memcpy(sl->data, payload, plen);
        }
        xSemaphoreGive(s_lock);
    }
}

// ---------------------------------------------------------------- Wiedergabe
static int decode_slot(const jb_slot_t *sl, int16_t *out)
{
    if (sl->pt == RTP_PT_L16) {
        for (int i = 0; i < sl->nsamp; i++) {
            out[i] = (int16_t)((sl->data[2 * i] << 8) | sl->data[2 * i + 1]);
        }
        memcpy(s_l16_last, out, sl->nsamp * sizeof(int16_t));
        return sl->nsamp;
    }
    // Stille-Pakete (TOC + hoechstens 2 Byte, z. B. 3 Byte vom NR_Com-Mixer) nicht dekodieren:
    // spart ~1,5 ms CPU pro 10 ms. Beim naechsten echten Paket startet der Decoder sauber neu.
    if (sl->len <= 3) {
        memset(out, 0, sl->nsamp * sizeof(int16_t));
        s_dec_idle = true;
        s_st.rx_silence++;
        return sl->nsamp;
    }
    if (s_dec_idle) {
        opus_decoder_ctl(s_dec, OPUS_RESET_STATE);
        s_dec_idle = false;
    }
    int64_t t0 = esp_timer_get_time();
    int n = opus_decode(s_dec, sl->data, sl->len, out, MAX_FRAME_SAMP, 0);
    uint32_t dt = (uint32_t)(esp_timer_get_time() - t0);
    s_dec_sum += dt; s_dec_n++;
    if (dt > s_st.dec_us_max) s_st.dec_us_max = dt;
    if (n < 0) { s_st.rx_bad++; return 0; }
    return n;
}

static int conceal(const jb_slot_t *next, int16_t *out)
{
    int n = s_last_n;
    if (s_last_pt == RTP_PT_L16) {
        for (int i = 0; i < n; i++) {
            s_l16_last[i] = (int16_t)(s_l16_last[i] / 2);   // leiser werdend wiederholen
            out[i] = s_l16_last[i];
        }
        return n;
    }
    if (next) {
        // Opus-FEC: das naechste Paket enthaelt eine grobe Kopie des fehlenden
        int r = opus_decode(s_dec, next->data, next->len, out, n, 1);
        if (r > 0) { s_st.rx_fec++; return r; }
    }
    int r = opus_decode(s_dec, NULL, 0, out, n, 0);         // Paketverlust-Verschleierung
    return r > 0 ? r : 0;
}

// Holt das naechste Paket aus dem Jitterbuffer und dekodiert es (oder verdeckt einen Verlust).
static int play_one(int16_t *out)
{
    int n = 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int depth = 0, frames = 0;
    for (int i = 0; i < JB_SLOTS; i++) {
        if (s_jb[i].used) { depth += s_jb[i].nsamp; frames++; }
    }
    const int target = g_set.jb_ms * AUDIO_RATE / 1000;
    if (!s_playing && s_have_seq && frames > 0 && depth >= target) s_playing = true;
    if (s_playing) {
        if (frames == 0) {
            s_playing = false;                     // Strom abgerissen -> neu vorpuffern
        } else {
            // Latenz abbauen, wenn der Puffer deutlich zu voll ist (Taktdrift, Burst)
            while (depth > target + 3 * s_last_n && frames > 1) {
                jb_slot_t *sl = &s_jb[s_next_seq % JB_SLOTS];
                if (sl->used && sl->seq == s_next_seq) {
                    sl->used = false;
                    depth -= sl->nsamp;
                    frames--;
                    s_st.rx_dropped++;
                }
                s_next_seq++;
            }
            jb_slot_t *sl = &s_jb[s_next_seq % JB_SLOTS];
            if (sl->used && sl->seq == s_next_seq) {
                n = decode_slot(sl, out);
                s_last_n = sl->nsamp;
                s_last_pt = sl->pt;
                sl->used = false;
            } else {
                jb_slot_t *nx = &s_jb[(uint16_t)(s_next_seq + 1) % JB_SLOTS];
                bool have_next = nx->used && nx->seq == (uint16_t)(s_next_seq + 1) && nx->pt != RTP_PT_L16;
                n = conceal(have_next ? nx : NULL, out);
                s_st.rx_lost++;
            }
            s_next_seq++;
        }
    }
    xSemaphoreGive(s_lock);
    return n;
}

static void codec_task(void *arg)
{
    static int16_t out[MAX_FRAME_SAMP];
    static int16_t pcm[MAX_TX_SAMP];
    while (1) {
        bool busy = false;
        if (s_codec_dirty) {
            s_codec_dirty = false;
            if (s_enc) { opus_encoder_destroy(s_enc); s_enc = NULL; }
            if (g_set.codec == CODEC_OPUS) s_enc = create_encoder();
            xStreamBufferReset(g_tx_stream);
        }

        // 1) Wiedergabe: Puffer zum I2S nur knapp fuellen (ca. 10 ms), der Rest bleibt im Jitterbuffer
        size_t level = xStreamBufferBytesAvailable(g_play_stream) / sizeof(int16_t);
        if (level <= AUDIO_PLAY_FILL) {
            int n = play_one(out);
            if (n > 0) {
                int32_t pk = 0;                    // Pegel des Empfangs (Diagnose: kommt Ton oder Stille?)
                for (int i = 0; i < n; i++) { int32_t a = out[i] < 0 ? -out[i] : out[i]; if (a > pk) pk = a; }
                float p = pk / 32768.0f;
                s_rx_peak = p > s_rx_peak ? p : s_rx_peak * 0.9f;
                xStreamBufferSend(g_play_stream, out, n * sizeof(int16_t), 0);
                busy = true;
            }
        }

        // 2) Senden: sobald ein ganzes Paket Mikrofon-Samples bereitliegt
        const int frame = tx_frame_samples();
        const size_t need = frame * sizeof(int16_t);
        if (xStreamBufferBytesAvailable(g_tx_stream) >= need) {
            size_t got = xStreamBufferReceive(g_tx_stream, pcm, need, 0);
            if (got == need) encode_and_send(pcm, frame);
            busy = true;
        }

        if (!busy) vTaskDelay(1);
    }
}

esp_err_t rtp_init(void)
{
    s_tx_seq = (uint16_t)esp_random();
    s_tx_ts = esp_random();
    s_tx_ssrc = esp_random();
    s_lock = xSemaphoreCreateMutex();
    int err;
    s_dec = opus_decoder_create(AUDIO_RATE, 1, &err);
    if (!s_dec) {
        ESP_LOGE(TAG, "Opus-Decoder: %s", opus_strerror(err));
        return ESP_FAIL;
    }
    s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_sock < 0) return ESP_FAIL;
    int tos = DSCP_EF_TOS;
    setsockopt(s_sock, IPPROTO_IP, IP_TOS, &tos, sizeof(tos));
    struct sockaddr_in local = {
        .sin_family = AF_INET,
        .sin_port = htons(g_set.local_port),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(s_sock, (struct sockaddr *)&local, sizeof(local)) != 0) {
        ESP_LOGE(TAG, "bind auf Port %u fehlgeschlagen", g_set.local_port);
        return ESP_FAIL;
    }
    rtp_update_target();
    // Opus braucht viel Stack (Variable-Length-Arrays). Am PC gemessen: Encoder 21-34 KB, Decoder 12-15 KB.
    // Prioritaet unter dem Netzwerk-Stack (tiT = 18): bei CPU-Ueberlast stottert hoechstens das Audio,
    // Verbindung zum Server und zum Display bleiben bestehen (vorher: Watchdog, Sitzung abgelaufen)
    if (xTaskCreate(rx_task, "rtp_rx", 4096, NULL, 17, NULL) != pdPASS ||
        xTaskCreate(codec_task, "codec", CODEC_STACK, NULL, 16, &s_tx_task) != pdPASS) {
        ESP_LOGE(TAG, "zu wenig RAM fuer RTP-Tasks (frei: %u Byte)", (unsigned)esp_get_free_heap_size());
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "RTP auf Port %u, Codec %s (Opus PT %d, L16 PT %d)", g_set.local_port,
             g_set.codec == CODEC_OPUS ? "Opus" : "L16", RTP_PT_OPUS, RTP_PT_L16);
    return ESP_OK;
}
