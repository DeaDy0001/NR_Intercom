#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/stream_buffer.h"

#define AUDIO_RATE        48000
#define AUDIO_BLOCK       240           // 5 ms pro I2S-Block
#define AUDIO_RING_MS     100

typedef struct {
    uint32_t blocks;          // verarbeitete 5-ms-Bloecke
    uint32_t i2s_errors;
    uint32_t play_underruns;  // Wiedergabe-Puffer leer (nur RTP)
    uint32_t tx_overflows;    // Sende-Puffer voll (nur RTP)
    float    mic_peak_db;     // Spitzenpegel Mikrofon (dBFS, letzte ~200 ms)
    float    out_peak_db;     // Spitzenpegel Ausgang
    uint32_t proc_us_max;     // max. Rechenzeit pro Block
} audio_stats_t;

esp_err_t audio_init(void);
void      audio_set_mode(int mode);
void      audio_get_stats(audio_stats_t *st);

// Mikrofon-Samples fuer den Netzversand (mono, int16) und Wiedergabe-Samples vom Netz
extern StreamBufferHandle_t g_tx_stream;
extern StreamBufferHandle_t g_play_stream;

// Sprechen (Taste 1). Im RTP-Modus wird nur bei aktivem Talk gesendet.
extern volatile bool g_talk;
