#pragma once
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    MODE_OFF = 0,   // Stille
    MODE_LOOP,      // Mikrofon -> Kopfhoerer (digital, ohne Netz)
    MODE_TONE,      // 1-kHz-Testton auf den Kopfhoerer
    MODE_RTP,       // Mikrofon -> RTP -> Netz, Netz -> RTP -> Kopfhoerer
    MODE_COUNT
} audio_mode_t;

typedef enum {
    MIC_ONBOARD = 0,  // MEMS-Mikrofon auf dem WM8960-Board (LINPUT1)
    MIC_JACK    = 1,  // Headset-Mikrofon ueber 3,5-mm-Buchse (RINPUT1)
} mic_src_t;

typedef enum {
    BAND_AUTO = 0,
    BAND_5G   = 1,
    BAND_2G   = 2,
} wifi_band_sel_t;

typedef struct {
    char     ssid[33];
    char     pass[65];
    char     host[64];      // RTP-Ziel
    uint16_t port;          // RTP-Zielport
    uint16_t local_port;    // lokaler RTP-Port (Senden + Empfangen)
    uint8_t  band;          // wifi_band_sel_t
    uint8_t  ant_ext;       // 1 = externe Antenne (IPEX)
    uint8_t  mode;          // audio_mode_t
    uint8_t  mic;           // mic_src_t
    int8_t   pga_db;        // Mic-PGA -17..+30 dB
    uint8_t  boost;         // Mic-Boost 0=0dB 1=+13 2=+20 3=+29 dB
    uint8_t  hp_vol;        // Kopfhoerer 0..100 %
    uint8_t  sidetone;      // Mithoerton 0..100 %
    uint8_t  frame_ms;      // RTP-Paketlaenge 5 oder 10 ms (L16)
    uint16_t jb_ms;         // Ziel-Jitterbuffer in ms
    uint8_t  ptt_latch;     // 1 = Taste 1 schaltet um, 0 = halten
} settings_t;

extern settings_t g_set;

void      settings_load(void);
esp_err_t settings_save(void);
void      settings_reset(void);
const char *mode_name(int mode);
