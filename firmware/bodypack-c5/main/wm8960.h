#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include "driver/i2c_master.h"

// WM8960 als I2S-Master: Der 24-MHz-Oszillator auf dem Waveshare-Board liefert MCLK,
// die PLL macht daraus SYSCLK = 12,288 MHz -> 48 kHz, BCLK = 1,536 MHz (16 Bit Stereo).
// Der ESP32 ist I2S-Slave, eine MCLK-Leitung zum ESP gibt es nicht.

esp_err_t wm8960_init(i2c_master_bus_handle_t bus);           // darf erneut aufgerufen werden
void      wm8960_detach(void);                              // vor i2c_del_master_bus aufrufen
void      wm8960_i2c_recover(int sda, int scl);             // Bus freitakten (vor i2c_new_master_bus)
bool      wm8960_present(void);                             // per I2C gefunden und initialisiert
uint32_t  wm8960_write_errors(void);
esp_err_t wm8960_set_mic(int src, int pga_db, int boost);   // src: 0 = Board-Mic (L), 1 = Buchse (R)
esp_err_t wm8960_set_hp_volume(int percent);                // 0 = stumm
esp_err_t wm8960_write(uint8_t reg, uint16_t val);
uint32_t  wm8960_i2c_retries(void);                       // Anzahl Wiederholungen seit Start
uint32_t  wm8960_get_i2c_hz(void);
void      wm8960_set_i2c_hz(uint32_t hz);                   // gilt ab dem naechsten wm8960_init
int       wm8960_i2c_test(i2c_master_bus_handle_t bus, uint32_t hz, int n, int *probe_ok);
uint16_t  wm8960_read_cache(uint8_t reg);                   // WM8960 ist nicht auslesbar
