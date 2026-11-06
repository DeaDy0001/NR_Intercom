#include "buttons.h"
#include "audio.h"
#include "settings.h"
#include "wm8960.h"
#include "net.h"
#include "led.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "buttons";

static const int s_pins[4] = {
    CONFIG_NRB_PIN_BTN1, CONFIG_NRB_PIN_BTN2, CONFIG_NRB_PIN_BTN3, CONFIG_NRB_PIN_BTN4,
};

void console_print_status(void);   // console_cmds.c

static void on_press(int idx)
{
    switch (idx) {
    case 0:
        if (g_set.ptt_latch) g_talk = !g_talk;
        else g_talk = true;
        break;
    case 1:
        g_set.mode = (g_set.mode + 1) % MODE_COUNT;
        audio_set_mode(g_set.mode);
        ESP_LOGI(TAG, "Modus: %s", mode_name(g_set.mode));
        break;
    case 2:
        g_set.mic = g_set.mic == MIC_ONBOARD ? MIC_JACK : MIC_ONBOARD;
        wm8960_set_mic(g_set.mic, g_set.pga_db, g_set.boost);
        ESP_LOGI(TAG, "Mikrofon: %s", g_set.mic == MIC_ONBOARD ? "Board" : "Buchse");
        break;
    case 3:
        console_print_status();
        break;
    }
}

static void on_release(int idx)
{
    if (idx == 0 && !g_set.ptt_latch) g_talk = false;
}

static void update_led(uint32_t tick)
{
    if (!net_is_connected() && g_set.mode == MODE_RTP) {
        bool on = (tick / 50) % 2;                 // gelb blinkend: kein WLAN
        led_set(on ? 20 : 0, on ? 12 : 0, 0);
    } else if (g_talk && g_set.mode == MODE_RTP) {
        led_set(40, 0, 0);                         // rot: Talk
    } else {
        switch (g_set.mode) {
        case MODE_LOOP: led_set(0, 15, 0); break;  // gruen
        case MODE_TONE: led_set(0, 0, 20); break;  // blau
        case MODE_RTP:  led_set(0, 12, 12); break; // tuerkis
        default:        led_set(3, 3, 3); break;   // schwach weiss
        }
    }
}

static void buttons_task(void *arg)
{
    uint8_t hist[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    bool pressed[4] = { false };
    uint32_t tick = 0;
    while (1) {
        for (int i = 0; i < 4; i++) {
            hist[i] = (hist[i] << 1) | (gpio_get_level(s_pins[i]) & 1);
            if (!pressed[i] && (hist[i] & 0x0F) == 0x00) {        // 4x hintereinander gedrueckt
                pressed[i] = true;
                on_press(i);
            } else if (pressed[i] && (hist[i] & 0x0F) == 0x0F) {  // 4x hintereinander losgelassen
                pressed[i] = false;
                on_release(i);
            }
        }
        if (tick % 4 == 0) update_led(tick);
        tick++;
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

esp_err_t buttons_init(void)
{
    uint64_t mask = 0;
    for (int i = 0; i < 4; i++) mask |= 1ULL << s_pins[i];
    gpio_config_t io = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    xTaskCreate(buttons_task, "buttons", 3072, NULL, 6, NULL);
    ESP_LOGI(TAG, "Tasten auf GPIO %d, %d, %d, %d", s_pins[0], s_pins[1], s_pins[2], s_pins[3]);
    return ESP_OK;
}
