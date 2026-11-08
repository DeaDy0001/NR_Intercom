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
    // Intercom-Einstellungen (S-Zeile)
    int   com_on, com_port;
    char  com_host[48];
    char  com_pin[16];
    char  wifi_ssid[33];   // eingestellte SSID
    // Intercom-Status (N-Zeile)
    int   com_state;       // 0 aus, 1 kein WLAN, 2 suche Server, 3 PIN falsch, 4 verbunden, 5 Fehler
    int   com_nch;
    uint32_t com_rev;
    char  com_ip[16];
    char  com_user[32];
    char  com_msg[48];
} c5_state_t;

#define LINK_MAX_CH   16
#define LINK_MAX_AP   16

typedef struct {
    char name[24];
    bool joined, talk;
    int  volume;           // 0..100
    int  talkers;          // andere, die gerade sprechen
} link_channel_t;

typedef struct {
    char ssid[33];
    int  rssi, channel;
    bool secure;
} link_ap_t;

enum { COM_ST_OFF, COM_ST_NOWIFI, COM_ST_SEARCH, COM_ST_PIN, COM_ST_ONLINE, COM_ST_ERROR };

esp_err_t link_init(void);
void      link_get(c5_state_t *st);
void      link_set(const char *name, int value);   // Einstellung am C5 setzen
void      link_talk(bool on);
int       link_get_channels(link_channel_t *out, int max, uint32_t *gen);   // gen aendert sich mit jeder neuen Liste
void      link_channel(int idx, const char *cmd, int value);              // join | leave | talk | vol
void      link_scan(void);                                                // WLAN-Suche am C5 starten
bool      link_scan_busy(void);
int       link_get_scan(link_ap_t *out, int max);
void      link_wifi(const char *ssid, const char *pass);
void      link_com(bool on, int port, const char *host, const char *pin);

typedef struct {
    char ip[16];
    int  port;
    char name[24];
    char project[32];
} link_server_t;

#define LINK_MAX_SRV 6
void      link_discover(void);                                            // NR_Com-Server im Netz suchen
bool      link_discover_busy(void);
int       link_get_servers(link_server_t *out, int max);
