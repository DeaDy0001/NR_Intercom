#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    bool     connected;
    char     ip[16];
    char     ssid[33];
    uint8_t  bssid[6];
    int8_t   rssi;
    uint8_t  channel;
    bool     band_5g;
    uint32_t disconnects;
    uint32_t roams;
} net_info_t;

esp_err_t net_init(void);
esp_err_t net_connect(void);          // nutzt g_set.ssid / g_set.pass / g_set.band
void      net_get_info(net_info_t *info);
bool      net_is_connected(void);
void      net_set_antenna(bool external);
void      net_scan(void);

typedef struct {
    char    ssid[33];
    int8_t  rssi;
    uint8_t channel;
    uint8_t secure;       // 1 = Passwort noetig
} net_ap_t;

int       net_scan_list(net_ap_t *out, int max);   // blockiert 2-4 s, staerkster AP je SSID, sortiert
void      net_get_mac_hex(char out[13]);
uint32_t  net_broadcast_addr(void);                // gerichtete Broadcast-Adresse des WLANs (Netzwerk-Byteorder), 0 = keine
