#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Intercom-Client fuer das NR_Com Device-Gateway (docs/device-gateway-api.md):
// HTTP/JSON fuer Login, Kanalliste (Long-Poll) und Befehle, Audio per RTP/Opus ueber rtp.c.

typedef enum {
    COM_OFF = 0,       // ausgeschaltet (g_set.com_on = 0)
    COM_NO_WIFI,       // wartet auf WLAN
    COM_SEARCH,        // Server wird gesucht / antwortet nicht
    COM_PIN_WRONG,     // Login abgelehnt
    COM_ONLINE,        // angemeldet, Kanalliste aktuell
    COM_ERROR,         // sonstiger Fehler (siehe msg)
} com_state_t;

#define COM_MAX_CH   16
#define COM_NAME_LEN 24

typedef struct {
    int     id;
    char    name[COM_NAME_LEN];
    bool    joined;
    bool    talk;
    uint8_t volume;     // 0..100
    uint8_t talkers;    // andere, die gerade in diesem Kanal sprechen
} com_channel_t;

typedef struct {
    com_state_t state;
    char        server_ip[16];
    char        user[32];
    char        msg[48];
    int         nch;
    uint32_t    rev;    // aendert sich bei jeder neuen Kanalliste
} com_status_t;

#define COM_HOST_AUTO "auto"          // Server automatisch suchen (Broadcast, dann nr-com.local)
#define COM_DISC_PORT 3010            // UDP-Port der Server-Suche

typedef struct {
    char     ip[16];
    uint16_t port;                    // HTTP-Port des Gateways
    char     name[24];
    char     project[32];
} com_server_t;

esp_err_t com_init(void);
int       com_discover(com_server_t *out, int max, int ms);   // blockiert ms Millisekunden
void      com_restart(void);                        // nach Aenderung von Host/Port/PIN/Ein-Aus
void      com_get_status(com_status_t *st);
int       com_get_channels(com_channel_t *out, int max);
bool      com_online(void);
// Befehle an den Server (asynchron). idx = Position in der Kanalliste.
void      com_join(int idx, bool join);
void      com_talk(int idx, bool on);
void      com_volume(int idx, int value);
void      com_talk_all(bool on);                    // Talk in allen betretenen Kanaelen (Taste 1, S3 Talk)
const char *com_state_name(com_state_t s);
