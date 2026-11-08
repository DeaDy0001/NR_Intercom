// S3 als USB-Seriell-Adapter: alles von USB geht 1:1 auf UART1 und zurueck.
// Verdrahtung zum ESP32-C5-Zero (UART0 des C5 = Flash-Schnittstelle des ROM-Bootloaders):
//   S3 GPIO4  (H2-10) = TX  ->  C5 GP12 (U0RXD)
//   S3 GPIO10 (H2-9)  = RX  <-  C5 GP11 (U0TXD)
//   GND                     ->  C5 GND
// Feste 115200 Baud (USB-Serial/JTAG ignoriert die Baudrate des PCs) -> esptool mit --baud 115200 nutzen.
#include "driver/usb_serial_jtag.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BRIDGE_UART  UART_NUM_1
#define PIN_TX       4
#define PIN_RX       10
#define BAUD         115200
#define BUF          4096

static void usb_to_uart(void *arg)
{
    static uint8_t buf[512];
    while (1) {
        int n = usb_serial_jtag_read_bytes(buf, sizeof(buf), pdMS_TO_TICKS(5));
        if (n > 0) uart_write_bytes(BRIDGE_UART, buf, n);
    }
}

static void uart_to_usb(void *arg)
{
    static uint8_t buf[512];
    while (1) {
        int n = uart_read_bytes(BRIDGE_UART, buf, sizeof(buf), pdMS_TO_TICKS(5));
        if (n > 0) usb_serial_jtag_write_bytes(buf, n, pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    usb_serial_jtag_driver_config_t ucfg = { .rx_buffer_size = BUF, .tx_buffer_size = BUF };
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&ucfg));
    uart_config_t cfg = {
        .baud_rate = BAUD, .data_bits = UART_DATA_8_BITS, .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1, .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(BRIDGE_UART, BUF, BUF, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(BRIDGE_UART, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(BRIDGE_UART, PIN_TX, PIN_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    xTaskCreate(usb_to_uart, "u2r", 4096, NULL, 10, NULL);
    xTaskCreate(uart_to_usb, "r2u", 4096, NULL, 10, NULL);
}
