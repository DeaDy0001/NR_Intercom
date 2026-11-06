idf_component_register(
    SRCS "main.c" "settings.c" "wm8960.c" "audio.c" "net.c" "rtp.c"
         "nettest.c" "buttons.c" "led.c" "console_cmds.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_i2c esp_driver_i2s esp_driver_gpio esp_driver_rmt
                  esp_wifi esp_netif esp_event nvs_flash console esp_timer lwip
)
