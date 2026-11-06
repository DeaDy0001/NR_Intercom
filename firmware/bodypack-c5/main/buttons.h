#pragma once
#include "esp_err.h"

// 4 Tasten (gegen GND, interne Pull-ups), entprellt.
// Testbelegung:
//   Taste 1: Talk (halten, oder umschalten wenn ptt_latch = 1)
//   Taste 2: Modus wechseln (off -> loop -> tone -> rtp)
//   Taste 3: Mikrofon wechseln (Board <-> Buchse)
//   Taste 4: Status ausgeben
// Spaeter frei programmierbar.
esp_err_t buttons_init(void);
