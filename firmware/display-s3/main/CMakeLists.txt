idf_component_register(
    SRCS "main.c" "display.c" "gfx.c" "fonts.c" "input.c" "battery.c" "ui.c" "link.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_lcd esp_driver_spi esp_driver_gpio esp_driver_pcnt esp_driver_uart esp_adc
                  nvs_flash esp_timer esp_psram
)
