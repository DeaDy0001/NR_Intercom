// NR_Intercom Bodypack - ESP32-S3 AMOLED (Display + Bedienung), Test-Firmware Phase 1
//
// - RM67162-AMOLED 536 x 240 ueber QSPI, Bildspeicher im PSRAM
// - 2 Drehgeber KY-040 ueber PCNT
// - Akkuspannung ueber GPIO1
// - Demo-Oberflaeche (4 Kanaele), Menue, Testbild, FPS-Test, Info
// - UART-Verbindung zum ESP32-C5: Bodypack-Einstellungen und Status im Menue

#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "display.h"
#include "gfx.h"
#include "input.h"
#include "battery.h"
#include "ui.h"
#include "link.h"
#include "esp_console.h"
#include "esp_system.h"
#include "soc/rtc_cntl_reg.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "main";

// Flash-Modus ohne BOOT-Taste (fuer NR-Intercom.bat)
static int cmd_download(int argc, char **argv)
{
    printf("Starte in den Flash-Modus (Download) ...\n");
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(100));
    REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
    esp_restart();
    return 0;
}

static int cmd_status(int argc, char **argv)
{
    ui_print_status();
    return 0;
}

void app_main(void)
{
    // Flash-Modus-Anforderung ('download') ueberlebt einen Reset: sofort loeschen
    REG_CLR_BIT(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
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
    if (link_init() != ESP_OK) ESP_LOGW(TAG, "Verbindung zum C5 nicht verfuegbar");
    ESP_ERROR_CHECK(ui_start());

    // USB-Konsole: "status" zeigt alles (fuer NR-Intercom.bat: s3 status)
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t rc = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    rc.prompt = "nrd> ";
    esp_console_dev_usb_serial_jtag_config_t uc = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    if (esp_console_new_repl_usb_serial_jtag(&uc, &rc, &repl) == ESP_OK) {
        esp_console_register_help_command();
        const esp_console_cmd_t st = { .command = "status", .help = "Zustand von Display, C5-Verbindung und Intercom",
                                       .func = cmd_status };
        esp_console_cmd_register(&st);
        const esp_console_cmd_t dl = { .command = "download", .help = "Flash-Modus (Update ohne BOOT-Taste)",
                                       .func = cmd_download };
        esp_console_cmd_register(&dl);
        esp_console_start_repl(repl);
    }

    ESP_LOGI(TAG, "bereit. RAM intern frei %u KB, PSRAM frei %u KB",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
}
