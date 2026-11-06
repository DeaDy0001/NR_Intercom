#pragma once
#include <stdint.h>
#include "esp_err.h"

esp_err_t led_init(int gpio);
void      led_set(uint8_t r, uint8_t g, uint8_t b);
