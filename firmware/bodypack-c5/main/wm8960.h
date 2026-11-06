#pragma once
#include "esp_err.h"
#include "driver/i2c_master.h"

// WM8960 als I2S-Master: Der 24-MHz-Oszillator auf dem Waveshare-Board liefert MCLK,
// die PLL macht daraus SYSCLK = 12,288 MHz -> 48 kHz, BCLK = 1,536 MHz (16 Bit Stereo).
// Der ESP32 ist I2S-Slave, eine MCLK-Leitung zum ESP gibt es nicht.

esp_err_t wm8960_init(i2c_master_bus_handle_t bus);
esp_err_t wm8960_set_mic(int src, int pga_db, int boost);   // src: 0 = Board-Mic (L), 1 = Buchse (R)
esp_err_t wm8960_set_hp_volume(int percent);                // 0 = stumm
esp_err_t wm8960_write(uint8_t reg, uint16_t val);
uint16_t  wm8960_read_cache(uint8_t reg);                   // WM8960 ist nicht auslesbar
