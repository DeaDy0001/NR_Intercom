#pragma once
#include "esp_err.h"

// Test-Oberflaeche:
//   Hauptseite: 4 Kanaele (Demo). Drehgeber 1 = Kanal waehlen, kurz = Hoeren, lang = Talk.
//               Drehgeber 2 = Lautstaerke des Kanals, kurz = Menue.
//   Menue: Helligkeit, Display drehen, Testbild, FPS-Test, Info.
esp_err_t ui_start(void);
