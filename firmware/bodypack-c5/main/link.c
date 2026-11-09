#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "link.h"
#include "settings.h"
#include "audio.h"
#include "wm8960.h"
#include "net.h"
#include "rtp.h"
#include "com.h"
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

volatile int g_cpu_load = -1;   // CPU-Last in % (aus der Laufzeit des Idle-Tasks), -1 = unbekannt
static int64_t s_save_at;       // 0 = nichts zu speichern
static bool    s_send_settings = true;
static bool    s_send_channels = true;
static bool    s_do_scan;
static bool    s_do_disc;

// Freitext (SSID, Passwort, Kanalnamen, PIN) wird als Hex uebertragen: keine Probleme mit Leerzeichen/Umlauten
static void hex_enc(const char *s, char *out, size_t max)
{
    static const char *h = "0123456789abcdef";
    size_t o = 0;
    for (; *s && o + 2 < max; s++) {
        out[o++] = h[(uint8_t)*s >> 4];
        out[o++] = h[(uint8_t)*s & 15];
    }
    out[o] = 0;
    if (o == 0 && max > 1) { out[0] = '-'; out[1] = 0; }     // leer = "-"
}

static int hexv(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

static void hex_dec(const char *s, char *out, size_t max)
{
    size_t o = 0;
    if (s && strcmp(s, "-")) {
        for (; s[0] && s[1] && o + 1 < max; s += 2) {
            int a = hexv(s[0]), b = hexv(s[1]);
            if (a < 0 || b < 0) break;
            out[o++] = (char)(a * 16 + b);
        }
    }
    out[o] = 0;
}

static void send_line(const char *s)
{
    uart_write_bytes(LINK_UART, s, strlen(s));
}

static void send_settings(void)
{
    char b[420], host[100], pin[34], ssid[68];
    hex_enc(g_set.com_host, host, sizeof(host));
    hex_enc(g_set.com_pin, pin, sizeof(pin));
    hex_enc(g_set.ssid, ssid, sizeof(ssid));
    snprintf(b, sizeof(b),
             "S mode=%d mic=%d pga=%d boost=%d vol=%d side=%d gate=%d band=%d ant=%d codec=%d kbps=%d "
             "frame=%d jb=%d latch=%d comon=%d comport=%u comhost=%s compin=%s wssid=%s\n",
             g_set.mode, g_set.mic, g_set.pga_db, g_set.boost, g_set.hp_vol, g_set.sidetone, g_set.gate_db,
             g_set.band, g_set.ant_ext, g_set.codec, g_set.opus_kbps, g_set.frame_ms, g_set.jb_ms,
             g_set.ptt_latch, g_set.com_on, g_set.com_port, host, pin, ssid);
    send_line(b);
}

// Intercom-Status (1 Hz) und Kanalliste (bei Aenderung)
static void send_com(void)
{
    com_status_t st;
    com_get_status(&st);
    char b[320], user[68], msg[100];
    hex_enc(st.user, user, sizeof(user));
    hex_enc(st.msg, msg, sizeof(msg));
    snprintf(b, sizeof(b), "N st=%d nch=%d rev=%lu ip=%s user=%s msg=%s\n", st.state, st.nch,
             (unsigned long)st.rev, st.server_ip[0] ? st.server_ip : "-", user, msg);
    send_line(b);
}

static void send_channels(void)
{
    com_channel_t ch[COM_MAX_CH];
    com_status_t st;
    com_get_status(&st);
    int n = com_get_channels(ch, COM_MAX_CH);
    char b[120], name[2 * COM_NAME_LEN + 2];
    for (int i = 0; i < n; i++) {
        hex_enc(ch[i].name, name, sizeof(name));
        snprintf(b, sizeof(b), "C i=%d j=%d t=%d v=%d a=%d n=%s\n", i, ch[i].joined, ch[i].talk, ch[i].volume,
                 ch[i].talkers, name);
        send_line(b);
    }
    snprintf(b, sizeof(b), "CE n=%d rev=%lu\n", n, (unsigned long)st.rev);
    send_line(b);
}

static void do_disc(void)
{
    static com_server_t sv[6];
    send_line("DS\n");                                  // Suche laeuft
    int n = net_is_connected() ? com_discover(sv, 6, 1600) : 0;
    char b[200], nm[50], pr[66];
    for (int i = 0; i < n; i++) {
        hex_enc(sv[i].name, nm, sizeof(nm));
        hex_enc(sv[i].project, pr, sizeof(pr));
        snprintf(b, sizeof(b), "d ip=%.15s p=%u n=%.49s j=%.65s\n", sv[i].ip, sv[i].port, nm, pr);
        send_line(b);
    }
    snprintf(b, sizeof(b), "DE n=%d\n", n);
    send_line(b);
}

static void do_scan(void)
{
    static net_ap_t aps[16];
    send_line("WS\n");                                  // Scan laeuft
    int n = net_scan_list(aps, 16);
    char b[120], ssid[68];
    for (int i = 0; i < n; i++) {
        hex_enc(aps[i].ssid, ssid, sizeof(ssid));
        snprintf(b, sizeof(b), "w r=%d c=%d k=%d s=%s\n", aps[i].rssi, aps[i].channel, aps[i].secure, ssid);
        send_line(b);
    }
    snprintf(b, sizeof(b), "WE n=%d\n", n);
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
             "g5=%d clip=%lu xrun=%lu cpu=%d ip=%s ssid=%s\n",
             as.rms_jack_db, as.rms_board_db, as.mic_peak_db, as.out_peak_db, as.gate_gain_db, g_talk ? 1 : 0,
             wm8960_present() ? 1 : 0, ni.connected ? 1 : 0, ni.rssi, ni.channel, ni.band_5g ? 1 : 0,
             (unsigned long)as.clips, (unsigned long)(as.rx_overflows + as.tx_underruns), g_cpu_load,
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
        if (k[0] == 'm') g_set.mic = clampi(v, 0, 2);
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
        bool on = a && atoi(a);
        if (com_online()) com_talk_all(on);              // Intercom: Talk in allen betretenen Kanaelen
        else g_talk = on;
    } else if (!strcmp(cmd, "CH")) {                     // CH <index> join|leave|talk|vol <wert>
        char *i = strtok(NULL, " \r\n"), *c = strtok(NULL, " \r\n"), *v = strtok(NULL, " \r\n");
        if (i && c) {
            int idx = atoi(i), val = v ? atoi(v) : 0;
            if (!strcmp(c, "join")) com_join(idx, true);
            else if (!strcmp(c, "leave")) com_join(idx, false);
            else if (!strcmp(c, "talk")) com_talk(idx, val);
            else if (!strcmp(c, "vol")) com_volume(idx, val);
        }
    } else if (!strcmp(cmd, "CHGET")) {
        s_send_channels = true;
    } else if (!strcmp(cmd, "DISC")) {
        s_do_disc = true;
    } else if (!strcmp(cmd, "SCAN")) {
        s_do_scan = true;
    } else if (!strcmp(cmd, "WIFI")) {                   // WIFI <ssid hex> <passwort hex>
        char *s = strtok(NULL, " \r\n"), *p = strtok(NULL, " \r\n");
        if (s) {
            hex_dec(s, g_set.ssid, sizeof(g_set.ssid));
            hex_dec(p, g_set.pass, sizeof(g_set.pass));
            settings_save();
            ESP_LOGI(TAG, "WLAN vom Display: '%s'", g_set.ssid);
            net_connect();
            s_send_settings = true;
        }
    } else if (!strcmp(cmd, "COM")) {                    // COM <ein 0|1> <port> <host hex> <pin hex>
        char *on = strtok(NULL, " \r\n"), *port = strtok(NULL, " \r\n");
        char *host = strtok(NULL, " \r\n"), *pin = strtok(NULL, " \r\n");
        if (on && port && host && pin) {
            g_set.com_on = atoi(on) ? 1 : 0;
            int pt = atoi(port);
            g_set.com_port = pt > 0 && pt < 65536 ? pt : 3002;
            hex_dec(host, g_set.com_host, sizeof(g_set.com_host));
            hex_dec(pin, g_set.com_pin, sizeof(g_set.com_pin));
            settings_save();
            ESP_LOGI(TAG, "Intercom vom Display: %s, %s:%u", g_set.com_on ? "ein" : "aus", g_set.com_host,
                     g_set.com_port);
            com_restart();
            s_send_settings = true;
        }
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
    static char line[320];
    int n = 0;
    int64_t last_live = 0, last_set = 0, last_com = 0, last_ch = 0;
    uint32_t sent_rev = 0;
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
        static int64_t last_cpu;
        static uint32_t idle0;
        if (now - last_cpu >= 1000000) {               // CPU-Last einmal pro Sekunde
            uint32_t idle = ulTaskGetIdleRunTimeCounter();
            if (last_cpu) {
                int64_t dt = now - last_cpu;
                int load = 100 - (int)((int64_t)(idle - idle0) * 100 / dt);
                g_cpu_load = load < 0 ? 0 : load > 100 ? 100 : load;
            }
            idle0 = idle;
            last_cpu = now;
        }
        if (s_do_disc) {
            s_do_disc = false;
            do_disc();
        }
        if (s_do_scan) {
            s_do_scan = false;
            do_scan();
        }
        com_status_t cs;
        com_get_status(&cs);
        if (cs.rev != sent_rev || s_send_channels || now - last_ch > 5000000) {
            sent_rev = cs.rev;
            s_send_channels = false;
            last_ch = now;
            send_com();
            send_channels();
            last_com = now;
        } else if (now - last_com > 1000000) {
            last_com = now;
            send_com();
        }
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
    esp_err_t err = uart_driver_install(LINK_UART, 512, 2048, 0, NULL, 0);
    if (err != ESP_OK) return err;
    uart_param_config(LINK_UART, &cfg);
    uart_set_pin(LINK_UART, CONFIG_NRB_PIN_LINK_TX, CONFIG_NRB_PIN_LINK_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    // RX mit Pull-up, damit eine offene Leitung keinen Zeichenmuell erzeugt
    gpio_pullup_en(CONFIG_NRB_PIN_LINK_RX);
    xTaskCreate(link_task, "link", 6144, NULL, 4, NULL);   // HTTP-Abfrage bei der Server-Suche
    ESP_LOGI(TAG, "Display-Verbindung: UART1 TX=GP%d RX=GP%d, %d Baud", CONFIG_NRB_PIN_LINK_TX,
             CONFIG_NRB_PIN_LINK_RX, LINK_BAUD);
    return ESP_OK;
}
