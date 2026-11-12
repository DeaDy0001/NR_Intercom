#include <stdio.h>
#include <string.h>
#include "ota.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

static const char *TAG = "ota";
void link_send_line(const char *s);      // link.c

static volatile int s_pct = -1;
static char s_msg[64];
static char s_url[200], s_ssid[33], s_pass[65];
static EventGroupHandle_t s_ev;
#define EV_IP BIT0

int ota_progress(void) { return s_pct; }
const char *ota_message(void) { return s_msg; }
void ota_clear_error(void) { if (s_pct == -2) s_pct = -1; }

static void fail(const char *why)
{
    snprintf(s_msg, sizeof(s_msg), "%s", why);
    s_pct = -2;
    char b[96];
    snprintf(b, sizeof(b), "O err=%s\n", why);
    link_send_line(b);
    ESP_LOGE(TAG, "Update fehlgeschlagen: %s", why);
}

static void on_ip(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    xEventGroupSetBits(s_ev, EV_IP);
}

static bool wifi_up(void)
{
    static bool init;
    if (!init) {
        s_ev = xEventGroupCreate();
        esp_netif_init();
        esp_event_loop_create_default();
        esp_netif_create_default_wifi_sta();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        if (esp_wifi_init(&cfg) != ESP_OK) return false;
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_ip, NULL);
        esp_wifi_set_storage(WIFI_STORAGE_RAM);
        esp_wifi_set_mode(WIFI_MODE_STA);
        init = true;
    }
    wifi_config_t wc = { 0 };
    strlcpy((char *)wc.sta.ssid, s_ssid, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, s_pass, sizeof(wc.sta.password));
    wc.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    wc.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    esp_wifi_set_config(WIFI_IF_STA, &wc);
    esp_wifi_start();
    esp_wifi_set_ps(WIFI_PS_NONE);
    for (int i = 0; i < 3; i++) {                 // einige Versuche, der S3 hat nur 2,4 GHz
        esp_wifi_connect();
        if (xEventGroupWaitBits(s_ev, EV_IP, pdFALSE, pdTRUE, pdMS_TO_TICKS(8000)) & EV_IP) return true;
        esp_wifi_disconnect();
    }
    return false;
}

static void ota_task(void *arg)
{
    s_pct = 0;
    link_send_line("O p=0\n");
    if (!wifi_up()) { fail("WLAN nicht erreichbar (S3 kann nur 2,4 GHz)"); goto out; }
    esp_http_client_config_t http = { .url = s_url, .timeout_ms = 10000, .buffer_size = 4096 };
    esp_https_ota_config_t cfg = { .http_config = &http };
    esp_https_ota_handle_t h = NULL;
    if (esp_https_ota_begin(&cfg, &h) != ESP_OK) { fail("PC nicht erreichbar (Firewall? TCP 8070)"); goto out; }
    int last = 0;
    esp_err_t err;
    while ((err = esp_https_ota_perform(h)) == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
        int size = esp_https_ota_get_image_size(h), got = esp_https_ota_get_image_len_read(h);
        int pct = size > 0 ? got * 100 / size : 0;
        s_pct = pct;
        if (pct >= last + 5) {
            last = pct;
            char b[24];
            snprintf(b, sizeof(b), "O p=%d\n", pct);
            link_send_line(b);
        }
    }
    if (err != ESP_OK || !esp_https_ota_is_complete_data_received(h)) {
        esp_https_ota_abort(h);
        fail("Download abgebrochen");
        goto out;
    }
    if (esp_https_ota_finish(h) != ESP_OK) { fail("Programm ungueltig"); goto out; }
    s_pct = 100;
    link_send_line("O ok\n");
    ESP_LOGI(TAG, "Update fertig, Neustart");
    vTaskDelay(pdMS_TO_TICKS(800));
    esp_restart();
out:
    esp_wifi_stop();
    vTaskDelete(NULL);
}

void ota_start(const char *url, const char *ssid, const char *pass)
{
    if (s_pct >= 0) return;                       // laeuft schon
    snprintf(s_url, sizeof(s_url), "%s", url);
    snprintf(s_ssid, sizeof(s_ssid), "%s", ssid);
    snprintf(s_pass, sizeof(s_pass), "%s", pass);
    s_msg[0] = 0;
    ESP_LOGI(TAG, "Update ueber WLAN '%s' von %s", s_ssid, s_url);
    if (xTaskCreate(ota_task, "ota", 8192, NULL, 4, NULL) != pdPASS) fail("zu wenig RAM");
}

void ota_check_new_version(bool c5_online)
{
    static bool done;
    static int64_t t0;
    if (done) return;
    esp_ota_img_states_t st;
    const esp_partition_t *run = esp_ota_get_running_partition();
    if (!run || esp_ota_get_state_partition(run, &st) != ESP_OK || st != ESP_OTA_IMG_PENDING_VERIFY) {
        done = true;
        return;
    }
    if (!t0) t0 = esp_timer_get_time();
    if (c5_online) {                              // Display laeuft und spricht mit dem C5 -> gut
        esp_ota_mark_app_valid_cancel_rollback();
        ESP_LOGI(TAG, "neue Version bestaetigt");
        done = true;
    } else if (esp_timer_get_time() - t0 > 60000000) {
        ESP_LOGE(TAG, "neue Version ohne Verbindung zum C5 -> zurueck zur vorherigen");
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
}
