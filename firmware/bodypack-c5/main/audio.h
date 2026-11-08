#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"

#define AUDIO_RATE        48000
#define AUDIO_BLOCK_MS    1
#define AUDIO_BLOCK       (AUDIO_RATE / 1000 * AUDIO_BLOCK_MS)   // 1 ms pro I2S-Block (48 Samples)
#define AUDIO_DMA_DESC    4             // I2S-Puffer je Richtung: 4 x 1 ms -> Mic->Ohr ca. 5-6 ms
#define AUDIO_PLAY_FILL   480           // RTP: Wiedergabepuffer bis 10 ms vorfuellen
#define AUDIO_TX_RING_MS  50            // Mikrofon -> Codec (max. Paket 20 ms)
#define AUDIO_PLAY_RING_MS 60           // Decoder -> I2S (Paket bis 40 ms + Fuellstand 10 ms)

typedef struct {
    uint32_t blocks;          // verarbeitete 1-ms-Bloecke
    uint32_t i2s_errors;
    uint32_t play_underruns;  // Wiedergabe-Puffer leer (nur RTP)
    uint32_t tx_overflows;    // Sende-Puffer voll (nur RTP)
    float    mic_peak_db;     // Spitzenpegel Mikrofon (dBFS, letzte ~200 ms)
    float    out_peak_db;     // Spitzenpegel Ausgang
    uint32_t proc_us_max;     // max. Rechenzeit pro Block
    float    rms_board_db;    // Effektivpegel Board-Mikrofon (links), ~0,5 s gemittelt
    float    rms_jack_db;     // Effektivpegel Buchse (rechts)
    float    gate_gain_db;    // aktuelle Daempfung des Mikrofon-Gates (0 = offen)
    uint32_t clips;           // 1-ms-Bloecke mit uebersteuertem Mikrofon (am Anschlag)
    uint32_t rx_overflows;    // I2S-Aussetzer Eingang (Audio-Task kam zu spaet)
    uint32_t tx_underruns;    // I2S-Aussetzer Ausgang -> hoerbares Knacksen
} audio_stats_t;

esp_err_t audio_init(void);
void      audio_set_mode(int mode);
void      audio_get_stats(audio_stats_t *st);

// Mikrofon-Samples fuer den Netzversand (mono, int16) und Wiedergabe-Samples vom Netz
extern StreamBufferHandle_t g_tx_stream;
extern StreamBufferHandle_t g_play_stream;

// Sprechen (Taste 1). Im RTP-Modus wird nur bei aktivem Talk gesendet.
extern volatile bool g_talk;
