#pragma once
#include <stdint.h>
#include "esp_err.h"

// RTP-Audio ueber UDP, ein Socket fuer Senden und Empfangen (kompatibel zu mediasoup "comedia").
//   Opus: 48 kHz mono, Payload-Typ 111 (wie NR_Com / mediasoup)
//   L16 : 48 kHz mono unkomprimiert, Payload-Typ 96 (zum Vergleich)
// Empfang erkennt den Codec am Payload-Typ.
// Lokaler Codec-Test: "rtp 127.0.0.1 <lokaler port>" schickt die Pakete an sich selbst.

#define RTP_PT_OPUS 111
#define RTP_PT_L16  96

typedef struct {
    uint32_t tx_packets;
    uint32_t tx_bytes;
    uint32_t rx_packets;
    uint32_t rx_lost;         // fehlende Pakete beim Abspielen
    uint32_t rx_fec;          // davon per Opus-FEC wiederhergestellt
    uint32_t rx_late;         // zu spaet angekommen (verworfen)
    uint32_t rx_dropped;      // verworfen, um Latenz abzubauen
    uint32_t rx_resyncs;
    uint32_t rx_bad;          // unbekannter Payload-Typ / Decoder-Fehler
    uint32_t jb_depth_ms;     // aktuelle Fuellung des Jitterbuffers
    float    jitter_ms;       // RFC-3550-Jitter
    uint32_t enc_us_avg, enc_us_max;   // Opus-Encoder Rechenzeit pro Paket
    uint32_t dec_us_avg, dec_us_max;   // Opus-Decoder Rechenzeit pro Paket
    uint32_t codec_stack_free;    // ungenutzter Stack des Codec-Tasks (Byte)
} rtp_stats_t;

esp_err_t rtp_init(void);
void      rtp_get_stats(rtp_stats_t *st);
void      rtp_reset_stats(void);
void      rtp_update_target(void);   // nach Aenderung von host/port
void      rtp_codec_changed(void);   // nach Aenderung von Codec/Bitrate/Komplexitaet/Frame
