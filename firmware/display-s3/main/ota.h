#pragma once
#include <stdbool.h>

// Update des S3 ueber WLAN. Der C5 schickt ueber die UART: "OTA <url hex> <ssid hex> <passwort hex>".
// Der S3 verbindet sich dafuer selbst kurz mit dem WLAN, laedt das Programm vom PC und startet neu.
// Fortschritt geht als "O p=<prozent>" / "O ok" / "O err=<text>" zurueck an den C5 (und von dort ans Werkzeug).
void ota_start(const char *url, const char *ssid, const char *pass);
int  ota_progress(void);            // -1 = kein Update, 0..100 = laeuft, -2 = Fehler (siehe ota_message)
const char *ota_message(void);
void ota_clear_error(void);
void ota_check_new_version(bool c5_online);   // nach einem Update: neue Version bestaetigen oder zurueck
