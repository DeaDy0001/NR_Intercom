// Updates ueber WLAN (OTA) fuer C5 und S3, siehe ota.h
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "ota.h"
#include "settings.h"
#include "net.h"
#include "com.h"
#include "link.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_app_desc.h"
#include "esp_http_client.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ota";

static int s_sock = -1;
static struct sockaddr_in s_peer;        // wer das Update angestossen hat (bekommt den Fortschritt)
static volatile bool s_busy;
static char s_url[200];
static char s_s3_ver[40];

const char *ota_s3_version(void)
{
    return s_s3_ver;
}

static void reply_to(const struct sockaddr_in *to, const char *fmt, ...)
{
    if (s_sock < 0 || !to) return;
    char b[320];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    if (n > 0) sendto(s_sock, b, n < (int)sizeof(b) ? n : (int)sizeof(b) - 1, 0, (const struct sockaddr *)to, sizeof(*to));
}

#define report(...) reply_to(&s_peer, __VA_ARGS__)

// ---------------------------------------------------------------- C5 selbst aktualisieren
static void c5_ota_task(void *arg)
{
    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    esp_http_client_config_t cfg = { .url = s_url, .timeout_ms = 10000, .buffer_size = 2048 };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    esp_ota_handle_t h = 0;
    esp_err_t err = ESP_FAIL;
    const char *why = "";
    static char buf[2048];
    if (!part || !c) { why = "kein OTA-Bereich"; goto out; }
    if (esp_http_client_open(c, 0) != ESP_OK) { why = "PC nicht erreichbar (Firewall? TCP 8070)"; goto out; }
    int total = esp_http_client_fetch_headers(c);
    int status = esp_http_client_get_status_code(c);
    if (status != 200) { why = "Datei nicht gefunden"; goto out; }
    if (total > (int)part->size) { why = "Programm zu gross"; goto out; }
    if ((err = esp_ota_begin(part, OTA_SIZE_UNKNOWN, &h)) != ESP_OK) { why = "esp_ota_begin"; goto out; }
    ESP_LOGI(TAG, "Update C5: %s -> %s (%d Byte)", s_url, part->label, total);
    report("{\"ota\":\"progress\",\"target\":\"c5\",\"pct\":0}");
    int got = 0, last_pct = 0;
    while (1) {
        int r = esp_http_client_read(c, buf, sizeof(buf));
        if (r < 0) { why = "Download abgebrochen"; err = ESP_FAIL; goto out; }
        if (r == 0) break;
        if ((err = esp_ota_write(h, buf, r)) != ESP_OK) { why = "Schreiben fehlgeschlagen"; goto out; }
        got += r;
        int pct = total > 0 ? got * 100 / total : 0;
        if (pct >= last_pct + 5) {
            last_pct = pct;
            report("{\"ota\":\"progress\",\"target\":\"c5\",\"pct\":%d}", pct);
        }
    }
    if (total > 0 && got != total) { why = "Download unvollstaendig"; err = ESP_FAIL; goto out; }
    if ((err = esp_ota_end(h)) != ESP_OK) { h = 0; why = "Programm ungueltig"; goto out; }
    h = 0;
    if ((err = esp_ota_set_boot_partition(part)) != ESP_OK) { why = "Umschalten fehlgeschlagen"; goto out; }
    ESP_LOGI(TAG, "Update C5 fertig (%d Byte), Neustart", got);
    report("{\"ota\":\"done\",\"target\":\"c5\",\"bytes\":%d}", got);
    esp_http_client_cleanup(c);
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();

out:
    if (h) esp_ota_abort(h);
    if (c) { esp_http_client_close(c); esp_http_client_cleanup(c); }
    ESP_LOGE(TAG, "Update C5 fehlgeschlagen: %s (%s)", why, esp_err_to_name(err));
    report("{\"ota\":\"error\",\"target\":\"c5\",\"msg\":\"%s\"}", why);
    s_busy = false;
    vTaskDelete(NULL);
}

// ---------------------------------------------------------------- S3: Fortschritt kommt ueber die UART
void ota_s3_line(const char *line)
{
    if (line[0] == 'V') {                                  // "V ver=<version>"
        const char *v = strstr(line, "ver=");
        if (v) {
            size_t n = strcspn(v + 4, " \r\n");
            if (n >= sizeof(s_s3_ver)) n = sizeof(s_s3_ver) - 1;
            memcpy(s_s3_ver, v + 4, n);
            s_s3_ver[n] = 0;
        }
        return;
    }
    // "O p=<pct>" | "O ok" | "O err=<text>"
    const char *p = strstr(line, "p=");
    if (p) { report("{\"ota\":\"progress\",\"target\":\"s3\",\"pct\":%d}", atoi(p + 2)); return; }
    if (strstr(line, " ok")) { report("{\"ota\":\"done\",\"target\":\"s3\"}"); s_busy = false; return; }
    const char *e = strstr(line, "err=");
    if (e) {
        char m[64];
        size_t n = strcspn(e + 4, "\r\n");
        if (n >= sizeof(m)) n = sizeof(m) - 1;
        memcpy(m, e + 4, n);
        m[n] = 0;
        for (char *q = m; *q; q++) if (*q == '"' || *q == '\\') *q = '\'';
        report("{\"ota\":\"error\",\"target\":\"s3\",\"msg\":\"%s\"}", m);
        s_busy = false;
    }
}

// ---------------------------------------------------------------- UDP-Befehle
static void udp_task(void *arg)
{
    char buf[300];
    while (1) {
        struct sockaddr_in from;
        socklen_t fl = sizeof(from);
        int len = recvfrom(s_sock, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&from, &fl);
        if (len <= 0) continue;
        buf[len] = 0;
        buf[strcspn(buf, "\r\n")] = 0;
        if (!strncmp(buf, "NRBP?", 5)) {
            char mac[13], ip[16] = "";
            net_info_t ni;
            net_get_info(&ni);
            net_get_mac_hex(mac);
            if (ni.connected) strlcpy(ip, ni.ip, sizeof(ip));
            com_status_t cs;
            com_get_status(&cs);
            for (char *q = cs.user; *q; q++) if (*q == '"' || *q == '\\') *q = '\'';
            reply_to(&from,
                     "{\"nrbp\":1,\"name\":\"Bodypack %s\",\"mac\":\"%s\",\"ip\":\"%s\",\"c5\":\"%s\",\"s3\":\"%s\","
                     "\"user\":\"%s\",\"busy\":%d}",
                     mac + 8, mac, ip, esp_app_get_description()->version, s_s3_ver, cs.user, s_busy ? 1 : 0);
        } else if (!strncmp(buf, "NRBP_OTA ", 9)) {
            char code[20] = "", target[4] = "", url[200] = "";
            if (sscanf(buf + 9, "%19s %3s %199s", code, target, url) != 3) {
                reply_to(&from, "{\"ota\":\"error\",\"msg\":\"Befehl unvollstaendig\"}");
                continue;
            }
            if (strcmp(code, g_set.ota_code)) {
                ESP_LOGW(TAG, "Update abgelehnt: falscher Update-Code von %s", inet_ntoa(from.sin_addr));
                reply_to(&from, "{\"ota\":\"error\",\"msg\":\"Update-Code falsch\"}");
                continue;
            }
            if (s_busy) { reply_to(&from, "{\"ota\":\"error\",\"msg\":\"Update laeuft schon\"}"); continue; }
            s_peer = from;
            strlcpy(s_url, url, sizeof(s_url));
            s_busy = true;
            if (!strcmp(target, "c5")) {
                reply_to(&from, "{\"ota\":\"start\",\"target\":\"c5\"}");
                if (xTaskCreate(c5_ota_task, "ota", 6144, NULL, 5, NULL) != pdPASS) {
                    s_busy = false;
                    reply_to(&from, "{\"ota\":\"error\",\"target\":\"c5\",\"msg\":\"zu wenig RAM\"}");
                }
            } else if (!strcmp(target, "s3")) {
                reply_to(&from, "{\"ota\":\"start\",\"target\":\"s3\"}");
                link_s3_ota(url);
            } else {
                s_busy = false;
                reply_to(&from, "{\"ota\":\"error\",\"msg\":\"Ziel unbekannt\"}");
            }
        }
    }
}

// Neue Version nach einem Update: erst nach erfolgreicher WLAN-Verbindung als gut markieren,
// sonst nach 90 s auf die alte zurueckspringen.
static void verify_task(void *arg)
{
    for (int i = 0; i < 90; i++) {
        if (net_is_connected()) {
            vTaskDelay(pdMS_TO_TICKS(3000));               // kurz stabil laufen lassen
            esp_ota_mark_app_valid_cancel_rollback();
            ESP_LOGI(TAG, "neue Version bestaetigt");
            vTaskDelete(NULL);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGE(TAG, "neue Version ohne WLAN -> zurueck zur vorherigen");
    esp_ota_mark_app_invalid_rollback_and_reboot();
    vTaskDelete(NULL);
}

esp_err_t ota_init(void)
{
    esp_ota_img_states_t st;
    const esp_partition_t *run = esp_ota_get_running_partition();
    if (run && esp_ota_get_state_partition(run, &st) == ESP_OK && st == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGI(TAG, "erste Start nach Update (%s), pruefe ...", run->label);
        xTaskCreate(verify_task, "ota_ok", 2560, NULL, 3, NULL);
    }
    s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_sock < 0) return ESP_FAIL;
    struct sockaddr_in a = { .sin_family = AF_INET, .sin_port = htons(OTA_UDP_PORT), .sin_addr.s_addr = htonl(INADDR_ANY) };
    if (bind(s_sock, (struct sockaddr *)&a, sizeof(a)) != 0) return ESP_FAIL;
    if (xTaskCreate(udp_task, "ota_udp", 3072, NULL, 4, NULL) != pdPASS) return ESP_ERR_NO_MEM;
    ESP_LOGI(TAG, "Update ueber WLAN bereit (UDP %d), laeuft aus %s", OTA_UDP_PORT, run ? run->label : "?");
    return ESP_OK;
}
