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
