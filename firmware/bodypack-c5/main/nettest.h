#pragma once
#include <stdint.h>
#include "esp_err.h"

// Funk-Messung: sendet 50 Pakete/s (Groesse wie ein Sprachpaket) an einen UDP-Echo-Server
// (tools/udp_echo.py) und misst Umlaufzeit, Schwankung und Verlust.
esp_err_t nettest_start(const char *host, uint16_t port, int seconds, int interval_ms, int size);
void      nettest_stop(void);
