#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// RM67162 AMOLED 1,91" (240 x 536), quer betrieben: 536 x 240
#define DISP_W 536
#define DISP_H 240

esp_err_t display_init(void);
void      display_set_brightness(uint8_t level);   // 0..255
void      display_set_flip(bool flip);              // um 180 Grad drehen
// Kopiert ein Rechteck aus dem Bildspeicher (RGB565, bereits byte-getauscht) zum Display.
void      display_push(const uint16_t *fb, int x0, int y0, int x1, int y1);
uint32_t  display_last_push_us(void);
