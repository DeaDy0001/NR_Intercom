#pragma once
#include <stdint.h>
#include "esp_err.h"

// RTP-Strecke fuer die ersten Tests: L16 (unkomprimiert, 48 kHz mono, Payload-Typ 96).
// Ein UDP-Socket fuer Senden und Empfangen (kompatibel zu mediasoup "comedia").
// Opus folgt, sobald die Opus-Bibliothek eingebunden ist.

#define RTP_PT_L16 96

typedef struct {
    uint32_t tx_packets;
    uint32_t rx_packets;
    uint32_t rx_lost;         // Luecken in der Sequenz
    uint32_t rx_late;         // zu spaet angekommen (verworfen)
    uint32_t rx_concealed;    // verdeckt (fehlendes Paket ersetzt)
    uint32_t rx_dropped;      // verworfen, um Latenz abzubauen
    uint32_t rx_resyncs;
    uint32_t jb_depth_ms;     // aktuelle Fuellung des Jitterbuffers
    float    jitter_ms;       // RFC-3550-Jitter
} rtp_stats_t;

esp_err_t rtp_init(void);
void      rtp_get_stats(rtp_stats_t *st);
void      rtp_reset_stats(void);
void      rtp_update_target(void);   // nach Aenderung von host/port
