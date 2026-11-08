#include <string.h>
#include <stddef.h>
#include "settings.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "settings";
#define SETTINGS_NS      "nrb"
#define SETTINGS_KEY     "cfg"
#define SETTINGS_VERSION 3

settings_t g_set;

typedef struct {
    uint32_t   version;
    settings_t s;
} settings_blob_t;

void settings_reset(void)
{
    memset(&g_set, 0, sizeof(g_set));
    strlcpy(g_set.ssid, CONFIG_NRB_WIFI_SSID, sizeof(g_set.ssid));
    strlcpy(g_set.pass, CONFIG_NRB_WIFI_PASS, sizeof(g_set.pass));
    strlcpy(g_set.host, CONFIG_NRB_RTP_HOST, sizeof(g_set.host));
    g_set.port       = CONFIG_NRB_RTP_PORT;
    g_set.local_port = CONFIG_NRB_RTP_LOCAL_PORT;
    g_set.band       = BAND_5G;
    g_set.ant_ext    = 1;
    g_set.mode       = MODE_LOOP;
    g_set.mic        = MIC_JACK;
    g_set.pga_db     = 20;
    g_set.boost      = 2;
    g_set.hp_vol     = 60;
    g_set.sidetone   = 30;
    g_set.frame_ms   = 10;
    g_set.jb_ms      = 30;
    g_set.ptt_latch  = 0;
    g_set.codec      = CODEC_OPUS;
    g_set.opus_kbps  = 48;
    g_set.opus_cplx  = 5;
    g_set.opus_voip  = 0;
    g_set.gate_db    = -50;
}

void settings_load(void)
{
    settings_reset();
    nvs_handle_t h;
    if (nvs_open(SETTINGS_NS, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "keine gespeicherten Einstellungen, nutze Standardwerte");
        return;
    }
    settings_blob_t blob;
    size_t len = sizeof(blob);
    esp_err_t err = nvs_get_blob(h, SETTINGS_KEY, &blob, &len);
    nvs_close(h);
    const size_t hdr = offsetof(settings_blob_t, s);
    if (err == ESP_OK && len == sizeof(blob) && blob.version == SETTINGS_VERSION) {
        g_set = blob.s;
        ESP_LOGI(TAG, "Einstellungen geladen");
    } else if (err == ESP_OK && blob.version >= 2 && blob.version < SETTINGS_VERSION && len > hdr &&
               len <= sizeof(blob)) {
        // aelterer Stand: vorhandene Felder uebernehmen, neue Felder behalten ihre Standardwerte
        const settings_t def = g_set;   // Standardwerte (settings_reset ist schon gelaufen)
        memcpy(&g_set, &blob.s, len - hdr);
        g_set.gate_db = def.gate_db;    // lag im alten Stand evtl. im Fuellbyte
        ESP_LOGI(TAG, "Einstellungen von Version %lu uebernommen", (unsigned long)blob.version);
    } else {
        ESP_LOGW(TAG, "Einstellungen ungueltig/alt, nutze Standardwerte");
    }
}

esp_err_t settings_save(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(SETTINGS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    settings_blob_t blob = { .version = SETTINGS_VERSION, .s = g_set };
    err = nvs_set_blob(h, SETTINGS_KEY, &blob, sizeof(blob));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

const char *mode_name(int mode)
{
    switch (mode) {
    case MODE_OFF:  return "off";
    case MODE_LOOP: return "loop";
    case MODE_TONE: return "tone";
    case MODE_RTP:  return "rtp";
    default:        return "?";
    }
}
