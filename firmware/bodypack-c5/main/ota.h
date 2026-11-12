#pragma once
#include "esp_err.h"

// Updates ueber WLAN (OTA), gesteuert vom Werkzeug NR-Intercom.bat (tools/nrtool.py):
//   UDP 3011  "NRBP?"                          -> Antwort JSON: Name, MAC, IP, Versionen C5/S3, Intercom-Benutzer
//   UDP 3011  "NRBP_OTA <code> <c5|s3> <url>"  -> Update starten, Fortschritt als JSON an den Absender
// C5: laedt das Programm per HTTP in den freien OTA-Bereich, startet neu. Startet die neue Version
//     nicht sauber (kein WLAN innerhalb 90 s), springt der Bootloader auf die alte zurueck.
// S3: der C5 schickt URL und WLAN-Zugang ueber die UART, der S3 laedt selbst.
#define OTA_UDP_PORT 3011

esp_err_t   ota_init(void);
void        ota_s3_line(const char *line);   // Zeile "O ..." vom S3 (Fortschritt) bzw. "V ..." (Version)
const char *ota_s3_version(void);
