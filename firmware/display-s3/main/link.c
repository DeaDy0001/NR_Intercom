#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "link.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

static const char *TAG = "link";
#define LINK_UART UART_NUM_1
#define LINK_BAUD 115200

static c5_state_t s_st;
static link_channel_t s_ch[LINK_MAX_CH], s_ch_new[LINK_MAX_CH];
static int s_nch;
static uint32_t s_ch_gen;
static link_ap_t s_ap[LINK_MAX_AP];
static int s_nap, s_nap_new;
static bool s_scanning;
static link_server_t s_srv[LINK_MAX_SRV];
static int s_nsrv, s_nsrv_new;
static bool s_discovering;

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

static void hex_enc(const char *s, char *out, size_t max)
{
    static const char *h = "0123456789abcdef";
    size_t o = 0;
    for (; *s && o + 2 < max; s++) {
        out[o++] = h[(uint8_t)*s >> 4];
        out[o++] = h[(uint8_t)*s & 15];
    }
    out[o] = 0;
    if (o == 0 && max > 1) { out[0] = '-'; out[1] = 0; }
}

// Wert zu einem Schluessel in einer "k=v k=v"-Zeile (Zeile wird nicht veraendert)
static bool kv(const char *line, const char *key, char *out, size_t max)
{
    size_t kl = strlen(key);
    for (const char *p = line; (p = strstr(p, key)); p += kl) {
        if ((p == line || p[-1] == ' ') && p[kl] == '=') {
            p += kl + 1;
            size_t n = 0;
            while (p[n] && p[n] != ' ' && p[n] != '\r' && n + 1 < max) { out[n] = p[n]; n++; }
            out[n] = 0;
            return true;
        }
    }
    return false;
}

static int kvi(const char *line, const char *key, int def)
{
    char b[16];
    return kv(line, key, b, sizeof(b)) ? atoi(b) : def;
}

static void kvhex(const char *line, const char *key, char *out, size_t max)
{
    char b[140];
    if (kv(line, key, b, sizeof(b))) hex_dec(b, out, max);
}
static int64_t s_last_rx;
static SemaphoreHandle_t s_lock;

static void parse_kv(char *tok, bool settings)
{
    char *eq = strchr(tok, '=');
    if (!eq) return;
    *eq = 0;
    const char *k = tok, *v = eq + 1;
    int i = atoi(v);
    float f = strtof(v, NULL);
    c5_state_t *s = &s_st;
    if (settings) {
        if (!strcmp(k, "mode")) s->mode = i;
        else if (!strcmp(k, "mic")) s->mic = i;
        else if (!strcmp(k, "pga")) s->pga = i;
        else if (!strcmp(k, "boost")) s->boost = i;
        else if (!strcmp(k, "vol")) s->vol = i;
        else if (!strcmp(k, "side")) s->side = i;
        else if (!strcmp(k, "gate")) s->gate = i;
        else if (!strcmp(k, "band")) s->band = i;
        else if (!strcmp(k, "ant")) s->ant = i;
        else if (!strcmp(k, "codec")) s->codec = i;
        else if (!strcmp(k, "kbps")) s->kbps = i;
        else if (!strcmp(k, "frame")) s->frame = i;
        else if (!strcmp(k, "jb")) s->jb = i;
        else if (!strcmp(k, "latch")) s->latch = i;
        else if (!strcmp(k, "comon")) s->com_on = i;
        else if (!strcmp(k, "comport")) s->com_port = i;
        else if (!strcmp(k, "comhost")) hex_dec(v, s->com_host, sizeof(s->com_host));
        else if (!strcmp(k, "compin")) hex_dec(v, s->com_pin, sizeof(s->com_pin));
        else if (!strcmp(k, "wssid")) hex_dec(v, s->wifi_ssid, sizeof(s->wifi_ssid));
    } else {
        if (!strcmp(k, "jack")) s->jack_db = f;
        else if (!strcmp(k, "board")) s->board_db = f;
        else if (!strcmp(k, "peak")) s->peak_db = f;
        else if (!strcmp(k, "out")) s->out_db = f;
        else if (!strcmp(k, "gateg")) s->gate_gain_db = f;
        else if (!strcmp(k, "talk")) s->talk = i;
        else if (!strcmp(k, "codecok")) s->codec_ok = i;
        else if (!strcmp(k, "wifi")) s->wifi = i;
        else if (!strcmp(k, "rssi")) s->rssi = i;
        else if (!strcmp(k, "ch")) s->channel = i;
        else if (!strcmp(k, "g5")) s->band_5g = i;
        else if (!strcmp(k, "clip")) s->clips = strtoul(v, NULL, 10);
        else if (!strcmp(k, "xrun")) s->xruns = strtoul(v, NULL, 10);
        else if (!strcmp(k, "ip")) strlcpy(s->ip, v, sizeof(s->ip));
        else if (!strcmp(k, "ssid")) strlcpy(s->ssid, v, sizeof(s->ssid));
    }
}

static void handle_line(char *line)
{
    // Intercom-Status, Kanalliste, WLAN-Suche
    if (line[0] == 'N' && line[1] == ' ') {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        s_st.com_state = kvi(line, "st", 0);
        s_st.com_nch = kvi(line, "nch", 0);
        s_st.com_rev = (uint32_t)kvi(line, "rev", 0);
        if (!kv(line, "ip", s_st.com_ip, sizeof(s_st.com_ip)) || !strcmp(s_st.com_ip, "-")) s_st.com_ip[0] = 0;
        kvhex(line, "user", s_st.com_user, sizeof(s_st.com_user));
        kvhex(line, "msg", s_st.com_msg, sizeof(s_st.com_msg));
        s_last_rx = esp_timer_get_time();
        xSemaphoreGive(s_lock);
        return;
    }
    if (line[0] == 'C' && line[1] == ' ') {             // ein Kanal (Index i)
        int i = kvi(line, "i", -1);
        if (i < 0 || i >= LINK_MAX_CH) return;
        link_channel_t *c = &s_ch_new[i];
        c->joined = kvi(line, "j", 0);
        c->talk = kvi(line, "t", 0);
        c->volume = kvi(line, "v", 100);
        c->talkers = kvi(line, "a", 0);
        c->name[0] = 0;
        kvhex(line, "n", c->name, sizeof(c->name));
        return;
    }
    if (line[0] == 'C' && line[1] == 'E') {             // Liste vollstaendig -> uebernehmen
        int n = kvi(line, "n", 0);
        if (n > LINK_MAX_CH) n = LINK_MAX_CH;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        memcpy(s_ch, s_ch_new, sizeof(s_ch));
        s_nch = n;
        s_ch_gen++;
        xSemaphoreGive(s_lock);
        return;
    }
    if (line[0] == 'D' && line[1] == 'S') { s_discovering = true; s_nsrv_new = 0; return; }
    if (line[0] == 'd' && line[1] == ' ') {
        if (s_nsrv_new >= LINK_MAX_SRV) return;
        link_server_t *v = &s_srv[s_nsrv_new];
        if (!kv(line, "ip", v->ip, sizeof(v->ip))) return;
        v->port = kvi(line, "p", 3002);
        v->name[0] = v->project[0] = 0;
        kvhex(line, "n", v->name, sizeof(v->name));
        kvhex(line, "j", v->project, sizeof(v->project));
        s_nsrv_new++;
        return;
    }
    if (line[0] == 'D' && line[1] == 'E') { s_nsrv = s_nsrv_new; s_discovering = false; return; }
    if (line[0] == 'W' && line[1] == 'S') { s_scanning = true; s_nap_new = 0; return; }
    if (line[0] == 'w' && line[1] == ' ') {
        if (s_nap_new >= LINK_MAX_AP) return;
        link_ap_t *a = &s_ap[s_nap_new];                // waehrend der Suche wird die Liste neu gefuellt
        a->rssi = kvi(line, "r", -100);
        a->channel = kvi(line, "c", 0);
        a->secure = kvi(line, "k", 1);
        a->ssid[0] = 0;
        kvhex(line, "s", a->ssid, sizeof(a->ssid));
        s_nap_new++;
        return;
    }
    if (line[0] == 'W' && line[1] == 'E') { s_nap = s_nap_new; s_scanning = false; return; }

    if ((line[0] != 'S' && line[0] != 'L') || line[1] != ' ') return;
    bool settings = line[0] == 'S';
    xSemaphoreTake(s_lock, portMAX_DELAY);
    char *save;
    for (char *t = strtok_r(line + 2, " \r", &save); t; t = strtok_r(NULL, " \r", &save)) parse_kv(t, settings);
    if (settings) s_st.have_settings = true;
    s_st.lines++;
    s_last_rx = esp_timer_get_time();
    xSemaphoreGive(s_lock);
}

static void send(const char *s)
{
    uart_write_bytes(LINK_UART, s, strlen(s));
}

static void link_task(void *arg)
{
    static char line[512];
    int n = 0;
    uint8_t buf[128];
    int64_t last_get = 0;
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
            } else {
                n = 0;   // Zeile zu lang -> verwerfen
            }
        }
        // solange keine Einstellungen da sind, jede Sekunde danach fragen
        int64_t now = esp_timer_get_time();
        if (!s_st.have_settings && now - last_get > 1000000) {
            last_get = now;
            send("GET\n");
        }
    }
}

void link_get(c5_state_t *st)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *st = s_st;
    st->online = s_last_rx && (esp_timer_get_time() - s_last_rx) < 2500000;   // kurze Pausen des C5 (Suche, Last) tolerieren
    xSemaphoreGive(s_lock);
}

void link_set(const char *name, int value)
{
    char b[48];
    snprintf(b, sizeof(b), "SET %s %d\n", name, value);
    send(b);
}

void link_talk(bool on)
{
    send(on ? "TALK 1\n" : "TALK 0\n");
}

int link_get_channels(link_channel_t *out, int max, uint32_t *gen)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int n = s_nch < max ? s_nch : max;
    memcpy(out, s_ch, n * sizeof(link_channel_t));
    if (gen) *gen = s_ch_gen;
    xSemaphoreGive(s_lock);
    return n;
}

void link_channel(int idx, const char *cmd, int value)
{
    char b[48];
    snprintf(b, sizeof(b), "CH %d %s %d\n", idx, cmd, value);
    send(b);
    // sofort lokal anzeigen, der C5 bestaetigt mit der naechsten Liste
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (idx >= 0 && idx < s_nch) {
        link_channel_t *c = &s_ch[idx];
        if (!strcmp(cmd, "join")) c->joined = true;
        else if (!strcmp(cmd, "leave")) { c->joined = false; c->talk = false; }
        else if (!strcmp(cmd, "talk") && c->joined) c->talk = value;
        else if (!strcmp(cmd, "vol")) c->volume = value;
        s_ch_gen++;
    }
    xSemaphoreGive(s_lock);
}

void link_discover(void)
{
    s_discovering = true;
    s_nsrv_new = 0;
    send("DISC\n");
}

bool link_discover_busy(void)
{
    return s_discovering;
}

int link_get_servers(link_server_t *out, int max)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int n = s_discovering ? 0 : (s_nsrv < max ? s_nsrv : max);
    memcpy(out, s_srv, n * sizeof(link_server_t));
    xSemaphoreGive(s_lock);
    return n;
}

void link_scan(void)
{
    s_scanning = true;
    s_nap_new = 0;
    send("SCAN\n");
}

bool link_scan_busy(void)
{
    return s_scanning;
}

int link_get_scan(link_ap_t *out, int max)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int n = s_scanning ? 0 : (s_nap < max ? s_nap : max);
    memcpy(out, s_ap, n * sizeof(link_ap_t));
    xSemaphoreGive(s_lock);
    return n;
}

void link_wifi(const char *ssid, const char *pass)
{
    char b[220], hs[70], hp[132];
    hex_enc(ssid, hs, sizeof(hs));
    hex_enc(pass, hp, sizeof(hp));
    snprintf(b, sizeof(b), "WIFI %s %s\n", hs, hp);
    send(b);
}

void link_com(bool on, int port, const char *host, const char *pin)
{
    char b[200], hh[100], hp[34];
    hex_enc(host, hh, sizeof(hh));
    hex_enc(pin, hp, sizeof(hp));
    snprintf(b, sizeof(b), "COM %d %d %s %s\n", on ? 1 : 0, port, hh, hp);
    send(b);
}

esp_err_t link_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    uart_config_t cfg = {
        .baud_rate = LINK_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err = uart_driver_install(LINK_UART, 2048, 512, 0, NULL, 0);
    if (err != ESP_OK) return err;
    uart_param_config(LINK_UART, &cfg);
    uart_set_pin(LINK_UART, CONFIG_NRD_LINK_TX, CONFIG_NRD_LINK_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    gpio_pullup_en(CONFIG_NRD_LINK_RX);
    xTaskCreate(link_task, "link", 4096, NULL, 6, NULL);
    ESP_LOGI(TAG, "Verbindung zum C5: UART1 TX=GPIO%d RX=GPIO%d, %d Baud", CONFIG_NRD_LINK_TX, CONFIG_NRD_LINK_RX,
             LINK_BAUD);
    return ESP_OK;
}
