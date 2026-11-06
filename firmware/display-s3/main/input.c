// Drehgeber KY-040 ueber die PCNT-Hardware (Quadratur x4, Entprellfilter), Taster entprellt.
#include <stdlib.h>
#include "input.h"
#include "sdkconfig.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"

static const char *TAG = "input";

QueueHandle_t g_input_q;

typedef struct {
    pcnt_unit_handle_t unit;
    int last;          // letzter Zaehlerstand
    int acc;           // noch nicht ausgegebene Teilschritte
    int raw_total;
    int sw_gpio;
    uint8_t hist;
    bool pressed;
    bool long_sent;
    int64_t t_down;
    input_ev_type_t ev_turn, ev_press, ev_long;
} encoder_t;

static encoder_t s_enc[2];

static esp_err_t enc_setup(encoder_t *e, int clk, int dt, int sw)
{
    pcnt_unit_config_t ucfg = { .high_limit = 10000, .low_limit = -10000 };
    ESP_ERROR_CHECK(pcnt_new_unit(&ucfg, &e->unit));
    pcnt_glitch_filter_config_t filt = { .max_glitch_ns = 1000 };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(e->unit, &filt));

    pcnt_chan_config_t ca_cfg = { .edge_gpio_num = clk, .level_gpio_num = dt };
    pcnt_chan_config_t cb_cfg = { .edge_gpio_num = dt, .level_gpio_num = clk };
    pcnt_channel_handle_t ca, cb;
    ESP_ERROR_CHECK(pcnt_new_channel(e->unit, &ca_cfg, &ca));
    ESP_ERROR_CHECK(pcnt_new_channel(e->unit, &cb_cfg, &cb));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(ca, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(ca, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(cb, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(cb, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_unit_enable(e->unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(e->unit));
    ESP_ERROR_CHECK(pcnt_unit_start(e->unit));
    // KY-040 hat eigene 10-k-Pull-ups, die internen schaden nicht
    gpio_pullup_en(clk);
    gpio_pullup_en(dt);

    gpio_config_t io = { .pin_bit_mask = 1ULL << sw, .mode = GPIO_MODE_INPUT, .pull_up_en = GPIO_PULLUP_ENABLE };
    ESP_ERROR_CHECK(gpio_config(&io));
    e->sw_gpio = sw;
    e->hist = 0xFF;
    return ESP_OK;
}

static void send(input_ev_type_t t, int v)
{
    input_ev_t ev = { .type = t, .value = v };
    xQueueSend(g_input_q, &ev, 0);
}

static void enc_poll(encoder_t *e)
{
    int cnt = 0;
    pcnt_unit_get_count(e->unit, &cnt);
    int d = cnt - e->last;
    e->last = cnt;
    if (abs(cnt) > 8000) {               // weit vor dem Ueberlauf zuruecksetzen
        pcnt_unit_clear_count(e->unit);
        e->last = 0;
    }
#if CONFIG_NRD_ENC_INVERT
    d = -d;
#endif
    e->raw_total += d;
    e->acc += d;
    int steps = e->acc / CONFIG_NRD_ENC_STEPS_PER_DETENT;
    if (steps) {
        e->acc -= steps * CONFIG_NRD_ENC_STEPS_PER_DETENT;
        send(e->ev_turn, steps);
    }

    // Taster: 4 gleiche Abtastungen (20 ms) = stabil
    e->hist = (e->hist << 1) | (gpio_get_level(e->sw_gpio) & 1);
    int64_t now = esp_timer_get_time();
    if (!e->pressed && (e->hist & 0x0F) == 0x00) {
        e->pressed = true;
        e->long_sent = false;
        e->t_down = now;
    } else if (e->pressed && (e->hist & 0x0F) == 0x0F) {
        e->pressed = false;
        if (!e->long_sent) send(e->ev_press, 0);
    } else if (e->pressed && !e->long_sent && now - e->t_down > 600000) {
        e->long_sent = true;
        send(e->ev_long, 0);
    }
}

static void input_task(void *arg)
{
    while (1) {
        enc_poll(&s_enc[0]);
        enc_poll(&s_enc[1]);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void input_raw_counts(int *enc1, int *enc2)
{
    *enc1 = s_enc[0].raw_total;
    *enc2 = s_enc[1].raw_total;
}

esp_err_t input_init(void)
{
    g_input_q = xQueueCreate(32, sizeof(input_ev_t));
    s_enc[0].ev_turn = EV_ENC1_TURN; s_enc[0].ev_press = EV_ENC1_PRESS; s_enc[0].ev_long = EV_ENC1_LONG;
    s_enc[1].ev_turn = EV_ENC2_TURN; s_enc[1].ev_press = EV_ENC2_PRESS; s_enc[1].ev_long = EV_ENC2_LONG;
    enc_setup(&s_enc[0], CONFIG_NRD_ENC1_CLK, CONFIG_NRD_ENC1_DT, CONFIG_NRD_ENC1_SW);
    enc_setup(&s_enc[1], CONFIG_NRD_ENC2_CLK, CONFIG_NRD_ENC2_DT, CONFIG_NRD_ENC2_SW);
    xTaskCreate(input_task, "input", 3072, NULL, 8, NULL);
    ESP_LOGI(TAG, "Drehgeber 1: CLK %d DT %d SW %d | Drehgeber 2: CLK %d DT %d SW %d",
             CONFIG_NRD_ENC1_CLK, CONFIG_NRD_ENC1_DT, CONFIG_NRD_ENC1_SW,
             CONFIG_NRD_ENC2_CLK, CONFIG_NRD_ENC2_DT, CONFIG_NRD_ENC2_SW);
    return ESP_OK;
}
