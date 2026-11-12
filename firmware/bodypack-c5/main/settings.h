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
    // Waveshare-WM8960-Board: LINPUT1 = MEMS-Mikrofon "Left" (MIC2), RINPUT1 = MEMS-Mikrofon "Right" (MIC1).
    // Die 3,5-mm-Buchse ist NUR Kopfhoerer (MICBIAS und Buchse sind nicht verbunden).
    // Auf der eigenen Platine wird MIC_JACK der Headset-/XLR-Eingang.
    MIC_ONBOARD = 0,  // LINPUT1 (Board-Mikrofon links)
    MIC_JACK    = 1,  // RINPUT1 (Testboard: Board-Mikrofon rechts, eigene Platine: Headset)
    MIC_STEREO  = 2,  // Test: beide Eingaenge an, im Loop links = LINPUT1, rechts = RINPUT1
} mic_src_t;

typedef enum {
    CODEC_OPUS = 0,
    CODEC_L16  = 1,
} codec_sel_t;

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
    uint8_t  frame_ms;      // RTP-Paketlaenge: Opus 5/10/20 ms, L16 5/10 ms
    uint16_t jb_ms;         // Ziel-Jitterbuffer in ms
    uint8_t  ptt_latch;     // 1 = Taste 1 schaltet um, 0 = halten
    uint8_t  codec;         // codec_sel_t
    uint8_t  opus_kbps;     // Opus-Bitrate in kbit/s (8..128)
    uint8_t  opus_cplx;     // Opus-Komplexitaet 0..10
    uint8_t  opus_voip;     // 0 = restricted lowdelay (CELT, minimale Latenz), 1 = voip (SILK/Hybrid + FEC)
    // ab Version 3 (neue Felder immer hinten anhaengen, aeltere Stände werden uebernommen)
    int8_t   gate_db;       // Mikrofon-Gate: Schwelle in dBFS (-80..-20), 0 = aus
    // ab Version 4: Intercom-Server (NR_Com Device-Gateway, docs/device-gateway-api.md)
    char     com_host[48];  // Name oder IP, Standard "nr-com.local"
    uint16_t com_port;      // HTTP-Port des Gateways, Standard 3002
    char     com_pin[16];   // PIN des NR_Com-Benutzers
    uint8_t  com_on;        // 1 = Intercom-Client aktiv
    // ab Version 6
    uint8_t  rx_gain_db;    // Hoerverstaerkung fuer den Empfang (Intercom-Mix), 0..24 dB, mit Begrenzer
    // ab Version 7
    char     ota_code[16];  // Update-Code fuer Updates ueber WLAN (muss im Werkzeug gleich sein)
} settings_t;

extern settings_t g_set;

void      settings_load(void);
esp_err_t settings_save(void);
void      settings_reset(void);
const char *mode_name(int mode);
