#include <stdlib.h>
#include <string.h>
#include "net.h"
#include "settings.h"
#include "sdkconfig.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "net";

static esp_netif_t *s_netif;
static volatile bool s_connected;
static char s_ip[16];
static uint32_t s_disconnects, s_roams;
static uint8_t s_last_bssid[6];

void net_set_antenna(bool external)
{
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << CONFIG_NRB_PIN_ANT_SW,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io);
    int lvl = external ? CONFIG_NRB_ANT_EXT_LEVEL : !CONFIG_NRB_ANT_EXT_LEVEL;
    gpio_set_level(CONFIG_NRB_PIN_ANT_SW, lvl);
    ESP_LOGI(TAG, "Antenne: %s (GPIO%d = %d)", external ? "extern (IPEX)" : "intern (Keramik)",
             CONFIG_NRB_PIN_ANT_SW, lvl);
}

static volatile bool s_scanning;   // waehrend des Scans nicht automatisch neu verbinden

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (g_set.ssid[0]) esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_CONNECTED) {
        wifi_event_sta_connected_t *ev = data;
        if (memcmp(s_last_bssid, ev->bssid, 6) != 0 && s_last_bssid[0] | s_last_bssid[1] | s_last_bssid[2]) {
            s_roams++;
        }
        memcpy(s_last_bssid, ev->bssid, 6);
        ESP_LOGI(TAG, "verbunden mit %.*s, Kanal %d, AP %02x:%02x:%02x:%02x:%02x:%02x",
                 ev->ssid_len, (char *)ev->ssid, ev->channel,
                 ev->bssid[0], ev->bssid[1], ev->bssid[2], ev->bssid[3], ev->bssid[4], ev->bssid[5]);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *ev = data;
        if (s_connected) s_disconnects++;
        s_connected = false;
        ESP_LOGW(TAG, "getrennt (Grund %d), verbinde neu ...", ev->reason);
        if (g_set.ssid[0] && !s_scanning) esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = data;
        snprintf(s_ip, sizeof(s_ip), IPSTR, IP2STR(&ev->ip_info.ip));
        s_connected = true;
        ESP_LOGI(TAG, "IP-Adresse: %s", s_ip);
    }
}

esp_err_t net_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_netif = esp_netif_create_default_wifi_sta();
    esp_netif_set_hostname(s_netif, "nr-bodypack");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    net_set_antenna(g_set.ant_ext);
    return net_connect();
}

esp_err_t net_connect(void)
{
    wifi_config_t wc = { 0 };
    strlcpy((char *)wc.sta.ssid, g_set.ssid, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, g_set.pass, sizeof(wc.sta.password));
    wc.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;          // staerksten AP waehlen
    wc.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    wc.sta.threshold.authmode = g_set.pass[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    wc.sta.pmf_cfg.capable = true;
    wc.sta.rm_enabled = 1;    // 802.11k
    wc.sta.btm_enabled = 1;   // 802.11v
    wc.sta.ft_enabled = 1;    // 802.11r

    esp_wifi_stop();
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));

    wifi_band_mode_t bm = WIFI_BAND_MODE_AUTO;
    if (g_set.band == BAND_5G) bm = WIFI_BAND_MODE_5G_ONLY;
    if (g_set.band == BAND_2G) bm = WIFI_BAND_MODE_2G_ONLY;

    ESP_ERROR_CHECK(esp_wifi_start());
    esp_err_t err = esp_wifi_set_band_mode(bm);
    if (err != ESP_OK) ESP_LOGW(TAG, "Bandwahl fehlgeschlagen: %s", esp_err_to_name(err));
    // Kein Stromsparen: sonst bis zu 100 ms zusaetzliche Verzoegerung
    esp_wifi_set_ps(WIFI_PS_NONE);

    if (!g_set.ssid[0]) {
        ESP_LOGW(TAG, "keine SSID gesetzt -> Konsole: wifi <ssid> <passwort>");
    } else {
        ESP_LOGI(TAG, "verbinde mit '%s' (Band: %s) ...", g_set.ssid,
                 g_set.band == BAND_5G ? "5 GHz" : g_set.band == BAND_2G ? "2,4 GHz" : "auto");
    }
    return ESP_OK;
}

bool net_is_connected(void)
{
    return s_connected;
}

void net_get_info(net_info_t *info)
{
    memset(info, 0, sizeof(*info));
    info->connected = s_connected;
    info->disconnects = s_disconnects;
    info->roams = s_roams;
    strlcpy(info->ip, s_ip, sizeof(info->ip));
    wifi_ap_record_t ap;
    if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
        strlcpy(info->ssid, (char *)ap.ssid, sizeof(info->ssid));
        memcpy(info->bssid, ap.bssid, 6);
        info->rssi = ap.rssi;
        info->channel = ap.primary;
        info->band_5g = ap.primary > 14;
    }
}

int net_scan_list(net_ap_t *out, int max)
{
    s_scanning = true;
    if (!s_connected) {
        esp_wifi_disconnect();            // ein laufender Verbindungsversuch blockiert sonst den Scan
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    wifi_scan_config_t sc = { .show_hidden = false };
    esp_err_t err = esp_wifi_scan_start(&sc, true);
    int n_out = 0;
    if (err == ESP_OK) {
        uint16_t n = 24;
        wifi_ap_record_t *recs = calloc(n, sizeof(wifi_ap_record_t));
        if (recs) {
            esp_wifi_scan_get_ap_records(&n, recs);
            for (int i = 0; i < n; i++) {
                if (!recs[i].ssid[0]) continue;
                int k;
                for (k = 0; k < n_out; k++) if (!strcmp(out[k].ssid, (char *)recs[i].ssid)) break;
                if (k < n_out) {
                    if (recs[i].rssi > out[k].rssi) { out[k].rssi = recs[i].rssi; out[k].channel = recs[i].primary; }
                    continue;
                }
                if (n_out >= max) continue;
                strlcpy(out[n_out].ssid, (char *)recs[i].ssid, sizeof(out[n_out].ssid));
                out[n_out].rssi = recs[i].rssi;
                out[n_out].channel = recs[i].primary;
                out[n_out].secure = recs[i].authmode != WIFI_AUTH_OPEN;
                n_out++;
            }
            free(recs);
        }
        // nach Signalstaerke sortieren
        for (int i = 1; i < n_out; i++) {
            net_ap_t t = out[i];
            int j = i - 1;
            while (j >= 0 && out[j].rssi < t.rssi) { out[j + 1] = out[j]; j--; }
            out[j + 1] = t;
        }
    } else {
        ESP_LOGW(TAG, "Scan fehlgeschlagen: %s", esp_err_to_name(err));
    }
    s_scanning = false;
    if (!s_connected && g_set.ssid[0]) esp_wifi_connect();
    return n_out;
}

void net_get_mac_hex(char out[13])
{
    uint8_t m[6] = { 0 };
    esp_wifi_get_mac(WIFI_IF_STA, m);
    snprintf(out, 13, "%02x%02x%02x%02x%02x%02x", m[0], m[1], m[2], m[3], m[4], m[5]);
}

void net_scan(void)
{
    wifi_scan_config_t sc = { .show_hidden = false };
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) {
        printf("Scan fehlgeschlagen\n");
        return;
    }
    uint16_t n = 30;
    wifi_ap_record_t *recs = calloc(n, sizeof(wifi_ap_record_t));
    if (!recs) return;
    esp_wifi_scan_get_ap_records(&n, recs);
    printf("%-32s %-17s  Kanal  RSSI\n", "SSID", "BSSID");
    for (int i = 0; i < n; i++) {
        printf("%-32s %02x:%02x:%02x:%02x:%02x:%02x  %5d  %4d\n", (char *)recs[i].ssid,
               recs[i].bssid[0], recs[i].bssid[1], recs[i].bssid[2], recs[i].bssid[3],
               recs[i].bssid[4], recs[i].bssid[5], recs[i].primary, recs[i].rssi);
    }
    free(recs);
}
