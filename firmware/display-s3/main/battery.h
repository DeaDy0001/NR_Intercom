#pragma once
#include "esp_err.h"

esp_err_t battery_init(void);
int       battery_mv(void);        // Spannung am Akku-Eingang (mV), -1 = unbekannt
int       battery_percent(void);   // 0..100, grobe Li-Ionen-Kennlinie
