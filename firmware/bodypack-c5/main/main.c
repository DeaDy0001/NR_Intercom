// NR_Intercom Bodypack - ESP32-C5 (Funk + Audio), Test-Firmware Phase 1
//
// - WM8960 (Waveshare Audio Board) als I2S-Master, ESP32-C5 als Slave, 48 kHz
// - Modi: loop (Mic -> Kopfhoerer), tone (1 kHz), rtp (Opus oder L16 ueber WLAN)
// - WLAN 5 GHz, Stromsparen aus, Roaming 802.11k/v/r
// - Funk-Messung (nettest) gegen tools/udp_echo.py
// - Bedienung ueber USB-Konsole (help) und 4 Tasten

#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_system.h"
#include "driver/i2c_master.h"
#include "sdkconfig.h"
#include "settings.h"
#include "wm8960.h"
#include "audio.h"
#include "net.h"
#include "rtp.h"
#include "buttons.h"
#include "led.h"
#include "link.h"
#include "com.h"
#include "ota.h"

static const char *TAG = "main";

void console_cmds_register(void);
i2c_master_bus_handle_t g_i2c_bus;

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    settings_load();

    if (led_init(CONFIG_NRB_PIN_LED) == ESP_OK) led_set(10, 10, 10);

    // Der I2C-Treiber meldet jedes NACK dreizeilig; wm8960.c meldet selbst, was endgueltig scheitert
    esp_log_level_set("i2c.master", ESP_LOG_NONE);
    wm8960_i2c_recover(CONFIG_NRB_PIN_I2C_SDA, CONFIG_NRB_PIN_I2C_SCL);
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = CONFIG_NRB_PIN_I2C_SDA,
        .scl_io_num = CONFIG_NRB_PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));
    g_i2c_bus = bus;

    if (wm8960_init(bus) == ESP_OK) {
        wm8960_set_mic(g_set.mic, g_set.pga_db, g_set.boost);
        wm8960_set_hp_volume(g_set.hp_vol);
    } else {
        ESP_LOGE(TAG, "WM8960 fehlt: Verkabelung SDA=GPIO%d / SCL=GPIO%d / 3V3 / GND pruefen, dann 'codec' eingeben",
                 CONFIG_NRB_PIN_I2C_SDA, CONFIG_NRB_PIN_I2C_SCL);
    }

    ESP_ERROR_CHECK(audio_init());
    ESP_ERROR_CHECK(net_init());
    ESP_ERROR_CHECK(rtp_init());
    ESP_ERROR_CHECK(buttons_init());
    if (ota_init() != ESP_OK) ESP_LOGW(TAG, "Update ueber WLAN nicht verfuegbar");
    if (com_init() != ESP_OK) ESP_LOGW(TAG, "Intercom-Client nicht gestartet");
    if (link_init() != ESP_OK) ESP_LOGW(TAG, "Display-Verbindung (UART) nicht verfuegbar");

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_cfg = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_cfg.prompt = "nrb> ";
    esp_console_dev_usb_serial_jtag_config_t usj_cfg = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&usj_cfg, &repl_cfg, &repl));
    esp_console_register_help_command();
    console_cmds_register();
    ESP_ERROR_CHECK(esp_console_start_repl(repl));

    ESP_LOGI(TAG, "bereit. Freier RAM: %u Byte (minimal %u). 'help' fuer Befehle, 'status' fuer den Zustand.",
             (unsigned)esp_get_free_heap_size(), (unsigned)esp_get_minimum_free_heap_size());
}
