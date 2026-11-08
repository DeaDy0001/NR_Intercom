#pragma once
#include "esp_err.h"

// Test-Oberflaeche:
//   Hauptseite: Bodypack-Uebersicht (Talk, Modus, Pegel, Werte).
//               Drehgeber 1 = Lautstaerke, lang = Talk. Drehgeber 2 = Mic-Gain, lang = Modus.
//               Beide kurz = Menue.
//   Menue: Helligkeit, Display drehen, Testbild, FPS-Test, Info.
esp_err_t ui_start(void);
