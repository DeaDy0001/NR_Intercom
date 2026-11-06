// NR_Intercom Bodypack - ESP32-S3 AMOLED (Display + Bedienung), Test-Firmware Phase 1
//
// - RM67162-AMOLED 536 x 240 ueber QSPI, Bildspeicher im PSRAM
// - 2 Drehgeber KY-040 ueber PCNT
// - Akkuspannung ueber GPIO1
// - Demo-Oberflaeche (4 Kanaele), Menue, Testbild, FPS-Test, Info
// Die Verbindung zum ESP32-C5 (Funk/Audio) folgt in Phase 2.

#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "display.h"
#include "gfx.h"
#include "input.h"
#include "battery.h"
#include "ui.h"

static const char *TAG = "main";

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(display_init());
    ESP_ERROR_CHECK(gfx_init());
    ESP_ERROR_CHECK(input_init());
    if (battery_init() != ESP_OK) ESP_LOGW(TAG, "Akku-Messung nicht verfuegbar");
    ESP_ERROR_CHECK(ui_start());

    ESP_LOGI(TAG, "bereit. RAM intern frei %u KB, PSRAM frei %u KB",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
}
