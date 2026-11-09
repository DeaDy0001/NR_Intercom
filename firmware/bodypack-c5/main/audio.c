#include <math.h>
#include <string.h>
#include "audio.h"
#include "settings.h"
#include "sdkconfig.h"
#include "driver/i2s_std.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"

static const char *TAG = "audio";

StreamBufferHandle_t g_tx_stream;
StreamBufferHandle_t g_play_stream;
volatile bool g_talk;

static i2s_chan_handle_t s_tx, s_rx;
static volatile int s_mode = MODE_OFF;
static audio_stats_t s_st;
static float s_mic_peak, s_out_peak;   // linear 0..1, abklingend
static float s_ms_l, s_ms_r;           // mittlere Leistung (normiert), ~0,5 s
static float s_gate_gain = 1.0f;
static float s_env;                    // geglaettete Mikrofonleistung fuer das Gate
static volatile uint32_t s_rx_ovf, s_tx_udf;   // I2S-Aussetzer (aus der ISR gezaehlt)

static bool IRAM_ATTR on_rx_ovf(i2s_chan_handle_t h, i2s_event_data_t *e, void *arg)
{
    s_rx_ovf++;
    return false;
}

static bool IRAM_ATTR on_tx_udf(i2s_chan_handle_t h, i2s_event_data_t *e, void *arg)
{
    s_tx_udf++;
    return false;
}

#define GATE_FLOOR   0.0316f           // geschlossen: -30 dB (nicht ganz stumm, klingt natuerlicher)
#define GATE_HOLD    (200 / AUDIO_BLOCK_MS)   // 200 ms offen halten nach dem letzten Wort

static inline int16_t sat16(int32_t v)
{
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

static float lin_to_db(float v)
{
    return v > 1e-6f ? 20.0f * log10f(v) : -120.0f;
}

void audio_set_mode(int mode)
{
    if (mode < 0 || mode >= MODE_COUNT) return;
    s_mode = mode;
    xStreamBufferReset(g_tx_stream);
    xStreamBufferReset(g_play_stream);
}

void audio_get_stats(audio_stats_t *st)
{
    *st = s_st;
    st->mic_peak_db = lin_to_db(s_mic_peak);
    st->out_peak_db = lin_to_db(s_out_peak);
    st->rms_board_db = s_ms_l > 1e-12f ? 10.0f * log10f(s_ms_l) : -120.0f;
    st->rms_jack_db  = s_ms_r > 1e-12f ? 10.0f * log10f(s_ms_r) : -120.0f;
    st->gate_gain_db = lin_to_db(s_gate_gain);
    st->rx_overflows = s_rx_ovf;
    st->tx_underruns = s_tx_udf;
}

static void audio_task(void *arg)
{
    static int16_t in[AUDIO_BLOCK * 2];
    static int16_t out[AUDIO_BLOCK * 2];
    static int16_t mic[AUDIO_BLOCK];
    static int16_t play[AUDIO_BLOCK];
    float phase = 0.0f;
    const float tone_step = 2.0f * (float)M_PI * 1000.0f / AUDIO_RATE;
    const float decay = 1.0f - 0.006f * AUDIO_BLOCK_MS;   // Spitzenwert-Abfall pro Block
    const float ms_a  = 0.002f * AUDIO_BLOCK_MS;          // Mittelung Effektivpegel (~0,5 s)
    const float g_att = 0.007f;    // Gate oeffnet in ~3 ms
    const float g_rel = 0.00014f;  // Gate schliesst in ~150 ms
    const float env_a = 0.1f * AUDIO_BLOCK_MS;   // Huellkurve ~10 ms
    int gate_hold = 0;

    while (1) {
        size_t got = 0;
        esp_err_t err = i2s_channel_read(s_rx, in, sizeof(in), &got, pdMS_TO_TICKS(100));
        if (err != ESP_OK || got != sizeof(in)) {
            s_st.i2s_errors++;
            if (err != ESP_ERR_TIMEOUT) vTaskDelay(1);   // nie ohne Pause kreisen (Watchdog, WLAN)
            if (err == ESP_ERR_TIMEOUT) {
                static int64_t last_warn;
                int64_t now = esp_timer_get_time();
                if (now - last_warn > 5000000) {   // hoechstens alle 5 s melden
                    last_warn = now;
                    ESP_LOGW(TAG, "kein I2S-Takt vom WM8960 (Codec per I2C erreichbar? 'i2cscan' / CLK->GP%d, WS->GP%d pruefen)",
                             CONFIG_NRB_PIN_I2S_BCLK, CONFIG_NRB_PIN_I2S_WS);
                }
            }
            continue;
        }
        int64_t t0 = esp_timer_get_time();

        // Mikrofon-Kanal waehlen: LINPUT1 = linker I2S-Kanal, RINPUT1 = rechter
        const int ch = (g_set.mic == MIC_ONBOARD) ? 0 : 1;
        int32_t mpk = 0;
        int64_t sl = 0, sr = 0;
        for (int i = 0; i < AUDIO_BLOCK; i++) {
            int16_t s = in[2 * i + ch];
            mic[i] = s;
            int32_t a = s < 0 ? -s : s;
            if (a > mpk) mpk = a;
            sl += (int32_t)in[2 * i] * in[2 * i];
            sr += (int32_t)in[2 * i + 1] * in[2 * i + 1];
        }
        const float norm = 1.0f / (32768.0f * 32768.0f * AUDIO_BLOCK);
        float pl = (float)sl * norm, pr = (float)sr * norm;
        s_ms_l += (pl - s_ms_l) * ms_a;
        s_ms_r += (pr - s_ms_r) * ms_a;

        // Uebersteuerung zaehlen (Samples am Anschlag)
        for (int i = 0; i < AUDIO_BLOCK; i++) {
            if (mic[i] >= 32000 || mic[i] <= -32000) { s_st.clips++; break; }
        }

        // Mikrofon-Gate: unterdrueckt das Grundrauschen in Sprechpausen.
        // Entscheidung ueber eine geglaettete Huellkurve (~10 ms) mit 6 dB Hysterese, damit es bei
        // Rauschen nahe der Schwelle nicht flattert; weiches Oeffnen (~3 ms) gegen Knacksen.
        float pblk = ch ? pr : pl;
        s_env += (pblk - s_env) * env_a;
        if (g_set.gate_db < 0) {
            float thr_open = powf(10.0f, g_set.gate_db / 10.0f);
            float thr_close = thr_open * 0.25f;
            if (s_env > thr_open) gate_hold = GATE_HOLD;
            else if (s_env > thr_close && gate_hold > 0) gate_hold = GATE_HOLD;   // noch im Hysterese-Band
            else if (gate_hold > 0) gate_hold--;
            float target = gate_hold > 0 ? 1.0f : GATE_FLOOR;
            float g = s_gate_gain;
            const float c = target > g ? g_att : g_rel;
            for (int i = 0; i < AUDIO_BLOCK; i++) {
                g += (target - g) * c;
                mic[i] = (int16_t)(mic[i] * g);
            }
            s_gate_gain = g;
        } else {
            s_gate_gain = 1.0f;
        }

        const int mode = s_mode;
        const int32_t side_q15 = (int32_t)g_set.sidetone * g_set.sidetone * 32767 / 10000;

        switch (mode) {
        case MODE_LOOP:
            if (g_set.mic == MIC_STEREO) {       // Test: Eingaenge getrennt auf linkes/rechtes Ohr, ohne Gate
                memcpy(out, in, sizeof(out));
                break;
            }
            for (int i = 0; i < AUDIO_BLOCK; i++) {
                out[2 * i] = out[2 * i + 1] = mic[i];
            }
            break;

        case MODE_TONE:
            for (int i = 0; i < AUDIO_BLOCK; i++) {
                int16_t s = (int16_t)(sinf(phase) * 8192.0f);   // -12 dBFS
                phase += tone_step;
                if (phase > 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
                out[2 * i] = out[2 * i + 1] = s;
            }
            break;

        case MODE_RTP: {
            if (g_talk) {
                if (xStreamBufferSend(g_tx_stream, mic, sizeof(mic), 0) != sizeof(mic)) {
                    s_st.tx_overflows++;
                }
            }
            size_t n = xStreamBufferReceive(g_play_stream, play, sizeof(play), 0);
            if (n < sizeof(play)) {
                if (n > 0) s_st.play_underruns++;
                memset((uint8_t *)play + n, 0, sizeof(play) - n);
            }
            for (int i = 0; i < AUDIO_BLOCK; i++) {
                int32_t v = play[i];
                if (g_talk) v += (mic[i] * side_q15) >> 15;   // Mithoerton nur beim Sprechen
                out[2 * i] = out[2 * i + 1] = sat16(v);
            }
            break;
        }

        default:
            memset(out, 0, sizeof(out));
            break;
        }

        int32_t opk = 0;
        for (int i = 0; i < AUDIO_BLOCK * 2; i += 2) {
            int32_t a = out[i] < 0 ? -out[i] : out[i];
            if (a > opk) opk = a;
        }

        size_t written = 0;
        i2s_channel_write(s_tx, out, sizeof(out), &written, pdMS_TO_TICKS(100));

        float m = mpk / 32768.0f, o = opk / 32768.0f;
        s_mic_peak = m > s_mic_peak ? m : s_mic_peak * decay;
        s_out_peak = o > s_out_peak ? o : s_out_peak * decay;
        uint32_t dt = (uint32_t)(esp_timer_get_time() - t0);
        if (dt > s_st.proc_us_max) s_st.proc_us_max = dt;
        s_st.blocks++;
    }
}

esp_err_t audio_init(void)
{
    g_tx_stream   = xStreamBufferCreate(AUDIO_RATE * AUDIO_TX_RING_MS / 1000 * sizeof(int16_t), 1);
    g_play_stream = xStreamBufferCreate(AUDIO_RATE * AUDIO_PLAY_RING_MS / 1000 * sizeof(int16_t), 1);
    if (!g_tx_stream || !g_play_stream) return ESP_ERR_NO_MEM;

    // ESP32 = I2S-Slave, WM8960 liefert BCLK und WS
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_SLAVE);
    chan_cfg.dma_desc_num = AUDIO_DMA_DESC;
    chan_cfg.dma_frame_num = AUDIO_BLOCK;
    chan_cfg.auto_clear = true;
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &s_tx, &s_rx));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(AUDIO_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = CONFIG_NRB_PIN_I2S_BCLK,
            .ws   = CONFIG_NRB_PIN_I2S_WS,
            .dout = CONFIG_NRB_PIN_I2S_DOUT,
            .din  = CONFIG_NRB_PIN_I2S_DIN,
            .invert_flags = { .mclk_inv = false, .bclk_inv = false, .ws_inv = false },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_tx, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(s_rx, &std_cfg));
    i2s_event_callbacks_t rx_cbs = { .on_recv_q_ovf = on_rx_ovf };
    i2s_event_callbacks_t tx_cbs = { .on_send_q_ovf = on_tx_udf };
    i2s_channel_register_event_callback(s_rx, &rx_cbs, NULL);
    i2s_channel_register_event_callback(s_tx, &tx_cbs, NULL);
    ESP_ERROR_CHECK(i2s_channel_enable(s_tx));
    ESP_ERROR_CHECK(i2s_channel_enable(s_rx));

    s_mode = g_set.mode;
    // hoechste Prioritaet: bei nur 4 ms Puffer darf WLAN den Audio-Task nicht aufhalten
    xTaskCreate(audio_task, "audio", 4096, NULL, configMAX_PRIORITIES - 1, NULL);
    ESP_LOGI(TAG, "I2S Slave aktiv: BCLK=%d WS=%d DIN=%d DOUT=%d, Modus %s",
             CONFIG_NRB_PIN_I2S_BCLK, CONFIG_NRB_PIN_I2S_WS, CONFIG_NRB_PIN_I2S_DIN,
             CONFIG_NRB_PIN_I2S_DOUT, mode_name(s_mode));
    return ESP_OK;
}
