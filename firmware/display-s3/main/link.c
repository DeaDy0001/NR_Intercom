#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "link.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"

static const char *TAG = "link";
#define LINK_UART UART_NUM_1
#define LINK_BAUD 115200

static c5_state_t s_st;
static int64_t s_last_rx;
static SemaphoreHandle_t s_lock;

static void parse_kv(char *tok, bool settings)
{
    char *eq = strchr(tok, '=');
    if (!eq) return;
    *eq = 0;
    const char *k = tok, *v = eq + 1;
    int i = atoi(v);
    float f = strtof(v, NULL);
    c5_state_t *s = &s_st;
    if (settings) {
        if (!strcmp(k, "mode")) s->mode = i;
        else if (!strcmp(k, "mic")) s->mic = i;
        else if (!strcmp(k, "pga")) s->pga = i;
        else if (!strcmp(k, "boost")) s->boost = i;
        else if (!strcmp(k, "vol")) s->vol = i;
        else if (!strcmp(k, "side")) s->side = i;
        else if (!strcmp(k, "gate")) s->gate = i;
        else if (!strcmp(k, "band")) s->band = i;
        else if (!strcmp(k, "ant")) s->ant = i;
        else if (!strcmp(k, "codec")) s->codec = i;
        else if (!strcmp(k, "kbps")) s->kbps = i;
        else if (!strcmp(k, "frame")) s->frame = i;
        else if (!strcmp(k, "jb")) s->jb = i;
        else if (!strcmp(k, "latch")) s->latch = i;
    } else {
        if (!strcmp(k, "jack")) s->jack_db = f;
        else if (!strcmp(k, "board")) s->board_db = f;
        else if (!strcmp(k, "peak")) s->peak_db = f;
        else if (!strcmp(k, "out")) s->out_db = f;
        else if (!strcmp(k, "gateg")) s->gate_gain_db = f;
        else if (!strcmp(k, "talk")) s->talk = i;
        else if (!strcmp(k, "codecok")) s->codec_ok = i;
        else if (!strcmp(k, "wifi")) s->wifi = i;
        else if (!strcmp(k, "rssi")) s->rssi = i;
        else if (!strcmp(k, "ch")) s->channel = i;
        else if (!strcmp(k, "g5")) s->band_5g = i;
        else if (!strcmp(k, "ip")) strlcpy(s->ip, v, sizeof(s->ip));
        else if (!strcmp(k, "ssid")) strlcpy(s->ssid, v, sizeof(s->ssid));
    }
}

static void handle_line(char *line)
{
    if ((line[0] != 'S' && line[0] != 'L') || line[1] != ' ') return;
    bool settings = line[0] == 'S';
    xSemaphoreTake(s_lock, portMAX_DELAY);
    char *save;
    for (char *t = strtok_r(line + 2, " \r", &save); t; t = strtok_r(NULL, " \r", &save)) parse_kv(t, settings);
    if (settings) s_st.have_settings = true;
    s_st.lines++;
    s_last_rx = esp_timer_get_time();
    xSemaphoreGive(s_lock);
}

static void send(const char *s)
{
    uart_write_bytes(LINK_UART, s, strlen(s));
}

static void link_task(void *arg)
{
    char line[256];
    int n = 0;
    uint8_t buf[128];
    int64_t last_get = 0;
    while (1) {
        int r = uart_read_bytes(LINK_UART, buf, sizeof(buf), pdMS_TO_TICKS(20));
        for (int i = 0; i < r; i++) {
            char c = (char)buf[i];
            if (c == '\n') {
                line[n] = 0;
                handle_line(line);
                n = 0;
            } else if (n < (int)sizeof(line) - 1) {
                line[n++] = c;
            } else {
                n = 0;   // Zeile zu lang -> verwerfen
            }
        }
        // solange keine Einstellungen da sind, jede Sekunde danach fragen
        int64_t now = esp_timer_get_time();
        if (!s_st.have_settings && now - last_get > 1000000) {
            last_get = now;
            send("GET\n");
        }
    }
}

void link_get(c5_state_t *st)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *st = s_st;
    st->online = s_last_rx && (esp_timer_get_time() - s_last_rx) < 1000000;
    xSemaphoreGive(s_lock);
}

void link_set(const char *name, int value)
{
    char b[48];
    snprintf(b, sizeof(b), "SET %s %d\n", name, value);
    send(b);
}

void link_talk(bool on)
{
    send(on ? "TALK 1\n" : "TALK 0\n");
}

esp_err_t link_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    uart_config_t cfg = {
        .baud_rate = LINK_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err = uart_driver_install(LINK_UART, 1024, 512, 0, NULL, 0);
    if (err != ESP_OK) return err;
    uart_param_config(LINK_UART, &cfg);
    uart_set_pin(LINK_UART, CONFIG_NRD_LINK_TX, CONFIG_NRD_LINK_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    gpio_pullup_en(CONFIG_NRD_LINK_RX);
    xTaskCreate(link_task, "link", 4096, NULL, 6, NULL);
    ESP_LOGI(TAG, "Verbindung zum C5: UART1 TX=GPIO%d RX=GPIO%d, %d Baud", CONFIG_NRD_LINK_TX, CONFIG_NRD_LINK_RX,
             LINK_BAUD);
    return ESP_OK;
}
