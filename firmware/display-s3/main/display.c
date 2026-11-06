// RM67162 ueber QSPI mit esp_lcd (Befehl einspurig, Pixeldaten vierspurig).
// Init-Sequenz wie im LilyGo-AMOLED-Treiber (MIT-Lizenz), der im NR_Com-Repo bereits lief.

#include <string.h>
#include "display.h"
#include "sdkconfig.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "display";

#define LCD_HOST        SPI2_HOST
#define OP_WRITE_CMD    0x02
#define OP_WRITE_COLOR  0x32
#define STRIP_LINES     16
#define STRIP_BYTES     (DISP_W * STRIP_LINES * 2)

static esp_lcd_panel_io_handle_t s_io;
static SemaphoreHandle_t s_done;
static uint16_t *s_dma;            // Zwischenpuffer im internen RAM (DMA-faehig)
static uint32_t s_last_us;

static bool on_color_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *ev, void *ctx)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_done, &woken);
    return woken == pdTRUE;
}

static void cmd(uint8_t c, const uint8_t *data, size_t len)
{
    esp_lcd_panel_io_tx_param(s_io, (OP_WRITE_CMD << 24) | (c << 8), data, len);
}

static void cmd1(uint8_t c, uint8_t v)
{
    cmd(c, &v, 1);
}

void display_set_brightness(uint8_t level)
{
    cmd1(0x51, level);
}

void display_set_flip(bool flip)
{
    // MADCTL: quer (MV) + Spiegelung. 0xA0 = MV|MY, 0x60 = MV|MX
    cmd1(0x36, flip ? 0x60 : 0xA0);
}

esp_err_t display_init(void)
{
    s_done = xSemaphoreCreateBinary();
    s_dma = heap_caps_malloc(STRIP_BYTES, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!s_dma) return ESP_ERR_NO_MEM;

    gpio_config_t rst = { .pin_bit_mask = 1ULL << CONFIG_NRD_LCD_RST, .mode = GPIO_MODE_OUTPUT };
    gpio_config(&rst);
    gpio_set_level(CONFIG_NRD_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(CONFIG_NRD_LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(CONFIG_NRD_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(150));

    spi_bus_config_t bus = {
        .data0_io_num = CONFIG_NRD_LCD_D0,
        .data1_io_num = CONFIG_NRD_LCD_D1,
        .sclk_io_num = CONFIG_NRD_LCD_SCK,
        .data2_io_num = CONFIG_NRD_LCD_D2,
        .data3_io_num = CONFIG_NRD_LCD_D3,
        .max_transfer_sz = STRIP_BYTES + 16,
        .flags = SPICOMMON_BUSFLAG_QUAD,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = CONFIG_NRD_LCD_CS,
        .dc_gpio_num = -1,
        .spi_mode = 0,
        .pclk_hz = CONFIG_NRD_LCD_PCLK_MHZ * 1000 * 1000,
        .trans_queue_depth = 4,
        .on_color_trans_done = on_color_done,
        .lcd_cmd_bits = 32,
        .lcd_param_bits = 8,
        .flags.quad_mode = true,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &s_io));

    for (int i = 0; i < 2; i++) {            // zweimal, wie im Referenztreiber
        cmd1(0xFE, 0x00);                    // Seite 0
        cmd(0x11, NULL, 0);                  // Sleep Out
        vTaskDelay(pdMS_TO_TICKS(120));
        cmd1(0xFE, 0x05);
        cmd1(0x05, 0x05);                    // OVSS
        cmd1(0xFE, 0x01);
        cmd1(0x73, 0x25);
        cmd1(0xFE, 0x00);
        cmd1(0x3A, 0x55);                    // RGB565
        cmd1(0x51, 0x00);                    // Helligkeit 0 waehrend Init
        cmd(0x29, NULL, 0);                  // Display an
        vTaskDelay(pdMS_TO_TICKS(120));
    }
    display_set_flip(false);
    ESP_LOGI(TAG, "RM67162 bereit (%d x %d, QSPI %d MHz)", DISP_W, DISP_H, CONFIG_NRD_LCD_PCLK_MHZ);
    return ESP_OK;
}

void display_push(const uint16_t *fb, int x0, int y0, int x1, int y1)
{
    // RM67162 braucht gerade Startadressen und ungerade Endadressen
    x0 &= ~1; y0 &= ~1;
    x1 |= 1;  y1 |= 1;
    if (x1 >= DISP_W) x1 = DISP_W - 1;
    if (y1 >= DISP_H) y1 = DISP_H - 1;
    if (x0 > x1 || y0 > y1) return;
    int64_t t0 = esp_timer_get_time();
    const int w = x1 - x0 + 1;
    const int lines = STRIP_LINES * DISP_W / w;   // so viele Zeilen passen in den Puffer

    uint8_t caset[4] = { x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF };
    cmd(0x2A, caset, 4);
    for (int y = y0; y <= y1; y += lines) {
        int n = (y + lines - 1 <= y1) ? lines : (y1 - y + 1);
        int ye = y + n - 1;
        uint8_t raset[4] = { y >> 8, y & 0xFF, ye >> 8, ye & 0xFF };
        cmd(0x2B, raset, 4);
        uint16_t *dst = s_dma;
        for (int r = 0; r < n; r++) {
            memcpy(dst, fb + (y + r) * DISP_W + x0, w * 2);
            dst += w;
        }
        esp_lcd_panel_io_tx_color(s_io, (OP_WRITE_COLOR << 24) | (0x2C << 8), s_dma, w * n * 2);
        xSemaphoreTake(s_done, portMAX_DELAY);
    }
    s_last_us = (uint32_t)(esp_timer_get_time() - t0);
}

uint32_t display_last_push_us(void)
{
    return s_last_us;
}
