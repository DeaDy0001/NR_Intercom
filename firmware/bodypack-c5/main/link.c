#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "link.h"
#include "settings.h"
#include "audio.h"
#include "wm8960.h"
#include "net.h"
#include "rtp.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "link";
#define LINK_UART   UART_NUM_1
#define LINK_BAUD   115200

static int64_t s_save_at;       // 0 = nichts zu speichern
static bool    s_send_settings = true;

static void send_line(const char *s)
{
    uart_write_bytes(LINK_UART, s, strlen(s));
}

static void send_settings(void)
{
    char b[200];
    snprintf(b, sizeof(b),
             "S mode=%d mic=%d pga=%d boost=%d vol=%d side=%d gate=%d band=%d ant=%d codec=%d kbps=%d "
             "frame=%d jb=%d latch=%d\n",
             g_set.mode, g_set.mic, g_set.pga_db, g_set.boost, g_set.hp_vol, g_set.sidetone, g_set.gate_db,
             g_set.band, g_set.ant_ext, g_set.codec, g_set.opus_kbps, g_set.frame_ms, g_set.jb_ms,
             g_set.ptt_latch);
    send_line(b);
}

static void send_live(void)
{
    audio_stats_t as;
    net_info_t ni;
    audio_get_stats(&as);
    net_get_info(&ni);
    char b[260];
    snprintf(b, sizeof(b),
             "L jack=%.1f board=%.1f peak=%.1f out=%.1f gateg=%.0f talk=%d codecok=%d wifi=%d rssi=%d ch=%d "
             "g5=%d clip=%lu xrun=%lu ip=%s ssid=%s\n",
             as.rms_jack_db, as.rms_board_db, as.mic_peak_db, as.out_peak_db, as.gate_gain_db, g_talk ? 1 : 0,
             wm8960_present() ? 1 : 0, ni.connected ? 1 : 0, ni.rssi, ni.channel, ni.band_5g ? 1 : 0,
             (unsigned long)as.clips, (unsigned long)(as.rx_overflows + as.tx_underruns),
             ni.connected ? ni.ip : "-", g_set.ssid[0] ? g_set.ssid : "-");
    // Leerzeichen in der SSID wuerden das Format stoeren
    for (char *p = strstr(b, "ssid="); p && *p && *p != '\n'; p++) if (*p == ' ') *p = '_';
    send_line(b);
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

// Wendet eine Einstellung an wie der passende Konsolenbefehl
static bool apply(const char *k, int v)
{
    if (!strcmp(k, "mode")) {
        g_set.mode = clampi(v, 0, MODE_COUNT - 1);
        audio_set_mode(g_set.mode);
    } else if (!strcmp(k, "mic") || !strcmp(k, "pga") || !strcmp(k, "boost")) {
        if (k[0] == 'm') g_set.mic = v ? MIC_JACK : MIC_ONBOARD;
        else if (k[0] == 'p') g_set.pga_db = clampi(v, -17, 30);
        else g_set.boost = clampi(v, 0, 3);
        wm8960_set_mic(g_set.mic, g_set.pga_db, g_set.boost);
    } else if (!strcmp(k, "vol")) {
        g_set.hp_vol = clampi(v, 0, 100);
        wm8960_set_hp_volume(g_set.hp_vol);
    } else if (!strcmp(k, "side")) {
        g_set.sidetone = clampi(v, 0, 100);
    } else if (!strcmp(k, "gate")) {
        g_set.gate_db = v >= 0 ? 0 : clampi(v, -80, -20);
    } else if (!strcmp(k, "band")) {
        g_set.band = clampi(v, 0, 2);
        net_connect();
    } else if (!strcmp(k, "ant")) {
        g_set.ant_ext = v ? 1 : 0;
        net_set_antenna(g_set.ant_ext);
    } else if (!strcmp(k, "codec")) {
        g_set.codec = v ? CODEC_L16 : CODEC_OPUS;
        rtp_codec_changed();
    } else if (!strcmp(k, "kbps")) {
        g_set.opus_kbps = clampi(v, 8, 128);
        rtp_codec_changed();
    } else if (!strcmp(k, "frame")) {
        g_set.frame_ms = (v == 5 || v == 20) ? v : 10;
        rtp_codec_changed();
    } else if (!strcmp(k, "jb")) {
        g_set.jb_ms = clampi(v, 5, 300);
    } else if (!strcmp(k, "latch")) {
        g_set.ptt_latch = v ? 1 : 0;
    } else {
        return false;
    }
    return true;
}

static void handle_line(char *line)
{
    char *cmd = strtok(line, " \r\n");
    if (!cmd) return;
    if (!strcmp(cmd, "GET")) {
        s_send_settings = true;
    } else if (!strcmp(cmd, "TALK")) {
        char *a = strtok(NULL, " \r\n");
        g_talk = a && atoi(a);
    } else if (!strcmp(cmd, "SET")) {
        char *k = strtok(NULL, " \r\n");
        char *v = strtok(NULL, " \r\n");
        if (k && v && apply(k, atoi(v))) {
            s_save_at = esp_timer_get_time() + 2000000;   // gesammelt speichern (Flash schonen)
            s_send_settings = true;
        } else {
            ESP_LOGW(TAG, "unbekannte Einstellung: %s", k ? k : "?");
        }
    }
}

static void link_task(void *arg)
{
    char line[96];
    int n = 0;
    int64_t last_live = 0, last_set = 0;
    uint8_t buf[64];
    while (1) {
        int r = uart_read_bytes(LINK_UART, buf, sizeof(buf), pdMS_TO_TICKS(20));
        for (int i = 0; i < r; i++) {
            char c = (char)buf[i];
            if (c == '\n') {
                line[n] = 0;
                handle_line(line);
                n = 0;
            } else if (n < (int)sizeof(line) - 1) {
                line[n++] = c;
            }
        }
        int64_t now = esp_timer_get_time();
        if (s_send_settings || now - last_set > 5000000) {
            s_send_settings = false;
            last_set = now;
            send_settings();
        }
        if (now - last_live > 100000) {
            last_live = now;
            send_live();
        }
        if (s_save_at && now > s_save_at) {
            s_save_at = 0;
            settings_save();
        }
    }
}

esp_err_t link_init(void)
{
    uart_config_t cfg = {
        .baud_rate = LINK_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err = uart_driver_install(LINK_UART, 512, 1024, 0, NULL, 0);
    if (err != ESP_OK) return err;
    uart_param_config(LINK_UART, &cfg);
    uart_set_pin(LINK_UART, CONFIG_NRB_PIN_LINK_TX, CONFIG_NRB_PIN_LINK_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // RX mit Pull-up, damit eine offene Leitung keinen Zeichenmuell erzeugt
    gpio_pullup_en(CONFIG_NRB_PIN_LINK_RX);
    xTaskCreate(link_task, "link", 3072, NULL, 4, NULL);
    ESP_LOGI(TAG, "Display-Verbindung: UART1 TX=GP%d RX=GP%d, %d Baud", CONFIG_NRB_PIN_LINK_TX,
             CONFIG_NRB_PIN_LINK_RX, LINK_BAUD);
    return ESP_OK;
}
