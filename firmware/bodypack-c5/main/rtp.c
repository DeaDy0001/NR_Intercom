#include <string.h>
#include <math.h>
#include "rtp.h"
#include "audio.h"
#include "settings.h"
#include "net.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"

static const char *TAG = "rtp";

#define JB_SLOTS        32
#define MAX_FRAME_SAMP  480                 // 10 ms bei 48 kHz (L16 passt so in ein UDP-Paket)
#define DSCP_EF_TOS     0xB8                // DSCP 46 (Expedited Forwarding) -> WMM Voice

typedef struct {
    bool     used;
    uint16_t seq;
    uint16_t nsamp;
    int16_t  pcm[MAX_FRAME_SAMP];
} jb_slot_t;

static int s_sock = -1;
static struct sockaddr_in s_dst;
static volatile bool s_dst_valid;
static SemaphoreHandle_t s_lock;
static jb_slot_t s_jb[JB_SLOTS];
static bool     s_playing;
static uint16_t s_next_seq;
static int32_t  s_max_seq_ahead;     // hoechste empfangene Sequenz relativ zu s_next_seq
static bool     s_have_seq;
static int16_t  s_last_pcm[MAX_FRAME_SAMP];
static uint16_t s_last_n = MAX_FRAME_SAMP;
static rtp_stats_t s_st;
static float    s_jitter;            // in Samples (RFC 3550)
static int64_t  s_prev_transit;
static bool     s_have_transit;

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

void rtp_get_stats(rtp_stats_t *st)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *st = s_st;
    int depth = 0;
    for (int i = 0; i < JB_SLOTS; i++) {
        if (s_jb[i].used) depth += s_jb[i].nsamp;
    }
    st->jb_depth_ms = depth * 1000 / AUDIO_RATE;
    st->jitter_ms = s_jitter * 1000.0f / AUDIO_RATE;
    xSemaphoreGive(s_lock);
}

void rtp_reset_stats(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    memset(&s_st, 0, sizeof(s_st));
    xSemaphoreGive(s_lock);
}

static void jb_reset_locked(void)
{
    for (int i = 0; i < JB_SLOTS; i++) s_jb[i].used = false;
    s_playing = false;
    s_have_seq = false;
    s_max_seq_ahead = 0;
}

// ---------------------------------------------------------------- Senden
static void tx_task(void *arg)
{
    uint8_t pkt[12 + MAX_FRAME_SAMP * 2];
    int16_t pcm[MAX_FRAME_SAMP];
    uint16_t seq = (uint16_t)esp_random();
    uint32_t ts = esp_random();
    const uint32_t ssrc = esp_random();

    while (1) {
        const int frame = g_set.frame_ms * AUDIO_RATE / 1000;   // Samples pro Paket
        size_t need = frame * sizeof(int16_t), have = 0;
        while (have < need) {
            have += xStreamBufferReceive(g_tx_stream, (uint8_t *)pcm + have, need - have, portMAX_DELAY);
        }
        if (!s_dst_valid || !net_is_connected() || s_sock < 0) continue;

        pkt[0] = 0x80;                         // V=2
        pkt[1] = RTP_PT_L16;
        pkt[2] = seq >> 8; pkt[3] = seq & 0xFF;
        pkt[4] = ts >> 24; pkt[5] = ts >> 16; pkt[6] = ts >> 8; pkt[7] = ts;
        pkt[8] = ssrc >> 24; pkt[9] = ssrc >> 16; pkt[10] = ssrc >> 8; pkt[11] = ssrc;
        for (int i = 0; i < frame; i++) {      // L16 = Big Endian
            pkt[12 + 2 * i] = (uint16_t)pcm[i] >> 8;
            pkt[13 + 2 * i] = (uint16_t)pcm[i] & 0xFF;
        }
        int r = sendto(s_sock, pkt, 12 + frame * 2, 0, (struct sockaddr *)&s_dst, sizeof(s_dst));
        if (r > 0) s_st.tx_packets++;
        seq++;
        ts += frame;
    }
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
        if (buf[0] & 0x10) {                   // Header-Extension
            if (len < hdr + 4) continue;
            hdr += 4 + 4 * ((buf[hdr + 2] << 8) | buf[hdr + 3]);
        }
        int plen = len - hdr;
        if (buf[0] & 0x20) plen -= buf[len - 1];   // Padding
        if (plen <= 0) continue;
        uint16_t seq = (buf[2] << 8) | buf[3];
        uint32_t ts = ((uint32_t)buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
        int nsamp = plen / 2;
        if (nsamp > MAX_FRAME_SAMP) nsamp = MAX_FRAME_SAMP;

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
        if (!s_have_seq) {
            s_next_seq = seq;
            s_have_seq = true;
        }
        int16_t ahead = (int16_t)(seq - s_next_seq);
        if (ahead < -JB_SLOTS || ahead > 4 * JB_SLOTS) {
            // grosser Sprung (z. B. Sender neu gestartet / mixerRebuilt): neu synchronisieren
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
            sl->seq = seq;
            sl->nsamp = nsamp;
            const uint8_t *p = buf + hdr;
            for (int i = 0; i < nsamp; i++) sl->pcm[i] = (int16_t)((p[2 * i] << 8) | p[2 * i + 1]);
            if (ahead > s_max_seq_ahead) s_max_seq_ahead = ahead;
        }
        xSemaphoreGive(s_lock);
    }
}

// ---------------------------------------------------------------- Wiedergabe
static void play_task(void *arg)
{
    int16_t out[MAX_FRAME_SAMP];
    while (1) {
        // Wiedergabepuffer nur knapp gefuellt halten (ca. 2 Bloecke), der Rest ist Jitterbuffer
        size_t level = xStreamBufferBytesAvailable(g_play_stream) / sizeof(int16_t);
        if (level > 2 * AUDIO_BLOCK) {
            vTaskDelay(1);
            continue;
        }
        int n = 0;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        int depth = 0, frames = 0;
        for (int i = 0; i < JB_SLOTS; i++) {
            if (s_jb[i].used) { depth += s_jb[i].nsamp; frames++; }
        }
        const int target = g_set.jb_ms * AUDIO_RATE / 1000;
        if (!s_playing) {
            if (s_have_seq && depth >= target && frames > 0) s_playing = true;
        }
        if (s_playing) {
            if (frames == 0) {
                s_playing = false;            // Strom abgerissen -> neu vorpuffern
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
                    n = sl->nsamp;
                    memcpy(out, sl->pcm, n * sizeof(int16_t));
                    memcpy(s_last_pcm, sl->pcm, n * sizeof(int16_t));
                    s_last_n = n;
                    sl->used = false;
                } else {
                    // Paket fehlt: letztes Paket leiser wiederholen (einfache Verschleierung)
                    n = s_last_n;
                    for (int i = 0; i < n; i++) {
                        s_last_pcm[i] = (int16_t)(s_last_pcm[i] / 2);
                        out[i] = s_last_pcm[i];
                    }
                    s_st.rx_lost++;
                    s_st.rx_concealed++;
                }
                s_next_seq++;
            }
        }
        xSemaphoreGive(s_lock);

        if (n > 0) {
            xStreamBufferSend(g_play_stream, out, n * sizeof(int16_t), pdMS_TO_TICKS(20));
        } else {
            vTaskDelay(1);
        }
    }
}

esp_err_t rtp_init(void)
{
    s_lock = xSemaphoreCreateMutex();
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
    xTaskCreate(rx_task, "rtp_rx", 4096, NULL, configMAX_PRIORITIES - 3, NULL);
    xTaskCreate(play_task, "rtp_play", 4096, NULL, configMAX_PRIORITIES - 3, NULL);
    xTaskCreate(tx_task, "rtp_tx", 4096, NULL, configMAX_PRIORITIES - 4, NULL);
    ESP_LOGI(TAG, "RTP (L16, 48 kHz mono, PT %d) auf Port %u", RTP_PT_L16, g_set.local_port);
    return ESP_OK;
}
