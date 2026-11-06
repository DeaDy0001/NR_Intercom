// WS2812-RGB-LED ueber RMT (ohne externe Komponente)
#include "led.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"

#define RMT_RES_HZ 10000000   // 0,1 us pro Tick

static rmt_channel_handle_t s_chan;
static rmt_encoder_handle_t s_enc;

esp_err_t led_init(int gpio)
{
    rmt_tx_channel_config_t cfg = {
        .gpio_num = gpio,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_RES_HZ,
        .mem_block_symbols = 48,
        .trans_queue_depth = 2,
    };
    esp_err_t err = rmt_new_tx_channel(&cfg, &s_chan);
    if (err != ESP_OK) return err;
    rmt_bytes_encoder_config_t enc = {
        .bit0 = { .level0 = 1, .duration0 = 3, .level1 = 0, .duration1 = 9 },   // 0,3 us / 0,9 us
        .bit1 = { .level0 = 1, .duration0 = 9, .level1 = 0, .duration1 = 3 },   // 0,9 us / 0,3 us
        .flags.msb_first = 1,
    };
    err = rmt_new_bytes_encoder(&enc, &s_enc);
    if (err != ESP_OK) return err;
    return rmt_enable(s_chan);
}

void led_set(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_chan) return;
    static uint8_t grb[3];
    grb[0] = g; grb[1] = r; grb[2] = b;
    rmt_transmit_config_t tc = { .loop_count = 0 };
    if (rmt_transmit(s_chan, s_enc, grb, sizeof(grb), &tc) == ESP_OK) {
        rmt_tx_wait_all_done(s_chan, 10);
    }
}
