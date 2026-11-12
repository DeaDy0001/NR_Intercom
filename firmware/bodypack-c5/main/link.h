#pragma once
#include "esp_err.h"

// UART-Verbindung zum ESP32-S3-Display (Textzeilen, 115200 Baud, 8N1)
//
// S3 -> C5:  GET                 alle Einstellungen schicken
//            SET <name> <wert>   Einstellung setzen (wird sofort angewendet, 2 s spaeter gespeichert)
//            TALK 0|1            Sprechen aus/an
// C5 -> S3:  S name=wert ...     Einstellungen (nach GET, nach jeder Aenderung und alle 5 s)
//            L name=wert ...     Live-Werte alle 100 ms (Pegel, WLAN, Talk, Codec)
//
// Namen: mode mic pga boost vol side gate band ant codec kbps frame jb latch
esp_err_t link_init(void);
void      link_s3_ota(const char *url);   // S3-Update anstossen: URL + WLAN-Zugang an den S3
