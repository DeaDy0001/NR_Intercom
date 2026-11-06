#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum {
    EV_ENC1_TURN,      // value = Rastungen (+/-)
    EV_ENC2_TURN,
    EV_ENC1_PRESS,     // kurz gedrueckt
    EV_ENC2_PRESS,
    EV_ENC1_LONG,      // lang gedrueckt (> 600 ms)
    EV_ENC2_LONG,
} input_ev_type_t;

typedef struct {
    input_ev_type_t type;
    int             value;
} input_ev_t;

extern QueueHandle_t g_input_q;

esp_err_t input_init(void);
void      input_raw_counts(int *enc1, int *enc2);   // Rohzaehler (fuer die Info-Seite)
