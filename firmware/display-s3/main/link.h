#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// UART-Verbindung zum ESP32-C5 (Funk + Audio), Protokoll siehe firmware/bodypack-c5/main/link.h
//   S3 GPIO4  = TX  ->  C5 GP3 (RX)
//   S3 GPIO10 = RX  <-  C5 GP2 (TX)
//   GND gemeinsam

typedef struct {
    bool  online;          // in der letzten Sekunde Daten vom C5 empfangen
    bool  have_settings;
    // Einstellungen (S-Zeile)
    int   mode, mic, pga, boost, vol, side, gate, band, ant, codec, kbps, frame, jb, latch;
    // Live-Werte (L-Zeile)
    float jack_db, board_db, peak_db, out_db, gate_gain_db;
    bool  talk, codec_ok, wifi;
    int   rssi, channel;
    bool  band_5g;
    uint32_t clips;        // Millisekunden mit uebersteuertem Mikrofon seit Start des C5
    uint32_t xruns;        // I2S-Aussetzer am C5
    char  ip[16];
    char  ssid[33];
    uint32_t lines;        // empfangene Zeilen (Diagnose)
} c5_state_t;

esp_err_t link_init(void);
void      link_get(c5_state_t *st);
void      link_set(const char *name, int value);   // Einstellung am C5 setzen
void      link_talk(bool on);
