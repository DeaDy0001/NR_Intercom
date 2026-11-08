// Intercom-Client: NR_Com Device-Gateway (HTTP/JSON + RTP), siehe docs/device-gateway-api.md
#include <stdio.h>
#include <string.h>
#include "com.h"
#include "settings.h"
#include "audio.h"
#include "net.h"
#include "rtp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_http_client.h"
#include "cJSON.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

static const char *TAG = "com";

typedef struct {
    char cmd[8];
    int  ch_id;
    int  value;
} cmd_t;

static SemaphoreHandle_t s_lock;
static QueueHandle_t s_q;
static TaskHandle_t s_task;
static com_channel_t s_ch[COM_MAX_CH];
static int64_t s_pending_until[COM_MAX_CH];   // lokale Aenderung, Server-Stand noch nicht abwarten
static com_status_t s_st;
static char s_token[48];
static char s_base[64];                        // http://<ip>:<port>/device/v1
static volatile bool s_restart;
static volatile bool s_relogin;

const char *com_state_name(com_state_t s)
{
    switch (s) {
    case COM_OFF:       return "aus";
    case COM_NO_WIFI:   return "kein WLAN";
    case COM_SEARCH:    return "suche Server";
    case COM_PIN_WRONG: return "PIN abgelehnt";
    case COM_ONLINE:    return "verbunden";
    default:            return "Fehler";
    }
}

static void set_state(com_state_t st, const char *msg)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    bool changed = s_st.state != st || strcmp(s_st.msg, msg ? msg : "");
    s_st.state = st;
    strlcpy(s_st.msg, msg ? msg : "", sizeof(s_st.msg));
    xSemaphoreGive(s_lock);
    if (changed) ESP_LOGI(TAG, "%s%s%s", com_state_name(st), msg && msg[0] ? ": " : "", msg ? msg : "");
}

// wartet, bricht aber sofort ab, wenn com_restart() aufgerufen wird
static void nap(int ms)
{
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(ms));
}

// ---------------------------------------------------------------- HTTP
static int http_req(const char *url, const char *body, char *resp, int resp_max, int timeout_ms)
{
    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = timeout_ms,
        .buffer_size = 1024,
        .buffer_size_tx = 512,
        .disable_auto_redirect = true,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return -1;
    int blen = body ? (int)strlen(body) : 0;
    esp_http_client_set_method(c, body ? HTTP_METHOD_POST : HTTP_METHOD_GET);
    if (body) esp_http_client_set_header(c, "Content-Type", "application/json");
    int status = -1;
    if (resp_max > 0) resp[0] = 0;
    if (esp_http_client_open(c, blen) == ESP_OK) {
        if (blen == 0 || esp_http_client_write(c, body, blen) == blen) {
            esp_http_client_fetch_headers(c);
            status = esp_http_client_get_status_code(c);
            int n = 0, r;
            while (n < resp_max - 1 && (r = esp_http_client_read(c, resp + n, resp_max - 1 - n)) > 0) n += r;
            if (resp_max > 0) resp[n] = 0;
        }
    }
    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    return status;
}

static bool resolve(const char *host, char ip[16])
{
    struct in_addr a;
    if (inet_pton(AF_INET, host, &a) == 1) {          // schon eine IP
        strlcpy(ip, host, 16);
        return true;
    }
    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_STREAM };
    struct addrinfo *res = NULL;
    if (getaddrinfo(host, NULL, &hints, &res) != 0 || !res) return false;   // .local per mDNS
    inet_ntop(AF_INET, &((struct sockaddr_in *)res->ai_addr)->sin_addr, ip, 16);
    freeaddrinfo(res);
    return true;
}

// UTF-8 nicht mitten im Zeichen abschneiden
static void utf8_copy(char *dst, const char *src, size_t n)
{
    strlcpy(dst, src, n);
    size_t len = strlen(dst);
    if (len < n - 1) return;
    size_t i = len;
    while (i > 0 && ((unsigned char)dst[i - 1] & 0xC0) == 0x80) i--;    // Folgebytes
    if (i > 0 && ((unsigned char)dst[i - 1] & 0xC0) == 0xC0) {          // Startbyte eines Mehrbyte-Zeichens
        unsigned char lead = (unsigned char)dst[i - 1];
        size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : 2;
        if (len - (i - 1) < need) dst[i - 1] = 0;
    }
}

// ---------------------------------------------------------------- Server-Suche
// Broadcast "NRCOM_DISCOVER 1" an UDP 3010, jeder NR_Com-Server antwortet mit
// {"nrcom":1,"api":1,"name":"NR_Com","project":{"id":1,"name":"..."},"port":3002}
// GET /device/v1/hello an ip:port; true = Gateway antwortet
static bool probe_hello(const char *ip, uint16_t port, com_server_t *sv)
{
    char url[64], resp[256];
    snprintf(url, sizeof(url), "http://%s:%u/device/v1/hello", ip, port);
    if (http_req(url, NULL, resp, sizeof(resp), 1500) != 200) return false;
    cJSON *root = cJSON_Parse(resp);
    if (!root) return false;
    const cJSON *api = cJSON_GetObjectItem(root, "api");
    bool ok = cJSON_IsNumber(api);
    if (ok) {
        strlcpy(sv->ip, ip, sizeof(sv->ip));
        sv->port = port;
        const cJSON *nm = cJSON_GetObjectItem(root, "name");
        const cJSON *pr = cJSON_GetObjectItem(root, "project");
        const cJSON *pn = pr ? cJSON_GetObjectItem(pr, "name") : NULL;
        utf8_copy(sv->name, cJSON_IsString(nm) ? nm->valuestring : "NR_Com", sizeof(sv->name));
        utf8_copy(sv->project, cJSON_IsString(pn) ? pn->valuestring : "nr-com.local", sizeof(sv->project));
    }
    cJSON_Delete(root);
    return ok;
}

int com_discover(com_server_t *out, int max, int ms)
{
    int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s < 0) return 0;
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
    struct timeval tv = { .tv_sec = 0, .tv_usec = 200000 };
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    struct sockaddr_in any = { .sin_family = AF_INET, .sin_port = 0, .sin_addr.s_addr = htonl(INADDR_ANY) };
    bind(s, (struct sockaddr *)&any, sizeof(any));

    static const char req[] = "NRCOM_DISCOVER 1\n";
    int n = 0;
    int64_t end = esp_timer_get_time() + (int64_t)ms * 1000, next_send = 0;
    char buf[300];
    while (esp_timer_get_time() < end) {
        int64_t now = esp_timer_get_time();
        if (now >= next_send) {                          // alle 500 ms wiederholen (UDP kann verloren gehen)
            next_send = now + 500000;
            struct sockaddr_in dst = { .sin_family = AF_INET, .sin_port = htons(COM_DISC_PORT) };
            dst.sin_addr.s_addr = htonl(INADDR_BROADCAST);
            sendto(s, req, sizeof(req) - 1, 0, (struct sockaddr *)&dst, sizeof(dst));
            uint32_t bc = net_broadcast_addr();
            if (bc) {
                dst.sin_addr.s_addr = bc;
                sendto(s, req, sizeof(req) - 1, 0, (struct sockaddr *)&dst, sizeof(dst));
            }
        }
        struct sockaddr_in from;
        socklen_t fl = sizeof(from);
        int len = recvfrom(s, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&from, &fl);
        if (len <= 0) continue;
        buf[len] = 0;
        cJSON *root = cJSON_Parse(buf);
        if (!root) continue;
        const cJSON *ok = cJSON_GetObjectItem(root, "nrcom");
        const cJSON *port = cJSON_GetObjectItem(root, "port");
        if (cJSON_IsNumber(ok) && cJSON_IsNumber(port)) {
            char ip[16];
            inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
            int k;
            for (k = 0; k < n; k++) if (!strcmp(out[k].ip, ip)) break;
            if (k == n && n < max) {
                com_server_t *sv = &out[n++];
                strlcpy(sv->ip, ip, sizeof(sv->ip));
                sv->port = (uint16_t)port->valueint;
                const cJSON *nm = cJSON_GetObjectItem(root, "name");
                const cJSON *pr = cJSON_GetObjectItem(root, "project");
                const cJSON *pn = pr ? cJSON_GetObjectItem(pr, "name") : NULL;
                utf8_copy(sv->name, cJSON_IsString(nm) ? nm->valuestring : "NR_Com", sizeof(sv->name));
                utf8_copy(sv->project, cJSON_IsString(pn) ? pn->valuestring : "", sizeof(sv->project));
            }
        }
        cJSON_Delete(root);
    }
    close(s);

    // Rueckfall: Server antwortet (noch) nicht auf die Suche -> mDNS-Namen von NR_Com fragen und
    // direkt am Gateway nachsehen (Port: eingestellt, 3002 = SSL an, 3000 = SSL aus)
    if (n == 0 && max > 0) {
        char ip[16];
        if (resolve("nr-com.local", ip)) {
            const uint16_t ports[3] = { g_set.com_port, 3002, 3000 };
            for (int i = 0; i < 3 && n == 0; i++) {
                if (i > 0 && ports[i] == ports[0]) continue;
                if (probe_hello(ip, ports[i], &out[0])) n = 1;
            }
        }
    }
    return n;
}

// Server-Adresse bestimmen: "auto" = Suche per Broadcast, sonst/danach Name oder IP aufloesen
static bool find_server(char ip[16], uint16_t *port)
{
    const bool autom = !strcmp(g_set.com_host, COM_HOST_AUTO);
    *port = g_set.com_port;
    if (autom) {
        com_server_t sv[1];
        if (com_discover(sv, 1, 1600) > 0) {
            strlcpy(ip, sv[0].ip, 16);
            *port = sv[0].port;
            ESP_LOGI(TAG, "Server gefunden: %s (%s) %s:%u", sv[0].name, sv[0].project, ip, *port);
            return true;
        }
        return false;                                   // com_discover hat nr-com.local schon versucht
    }
    return resolve(g_set.com_host, ip);
}

// ---------------------------------------------------------------- Zustand
static void update_talk_locked(void)
{
    bool any = false;
    for (int i = 0; i < s_st.nch; i++) any |= s_ch[i].joined && s_ch[i].talk;
    g_talk = any;
}

static bool parse_state(const char *json, uint32_t *rev)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) return false;
    const cJSON *jrev = cJSON_GetObjectItem(root, "rev");
    const cJSON *jch = cJSON_GetObjectItem(root, "channels");
    const cJSON *juser = cJSON_GetObjectItem(root, "user");
    if (!cJSON_IsNumber(jrev) || !cJSON_IsArray(jch)) {
        cJSON_Delete(root);
        return false;
    }
    *rev = (uint32_t)jrev->valuedouble;
    int64_t now = esp_timer_get_time();
    xSemaphoreTake(s_lock, portMAX_DELAY);
    com_channel_t old[COM_MAX_CH];
    int old_n = s_st.nch;
    memcpy(old, s_ch, sizeof(old));
    int n = 0;
    const cJSON *c;
    cJSON_ArrayForEach(c, jch) {
        if (n >= COM_MAX_CH) break;
        com_channel_t *ch = &s_ch[n];
        const cJSON *v;
        ch->id = (v = cJSON_GetObjectItem(c, "id")) && cJSON_IsNumber(v) ? v->valueint : -1;
        v = cJSON_GetObjectItem(c, "name");
        utf8_copy(ch->name, cJSON_IsString(v) ? v->valuestring : "?", sizeof(ch->name));
        ch->joined = cJSON_IsTrue(cJSON_GetObjectItem(c, "joined"));
        ch->talk = cJSON_IsTrue(cJSON_GetObjectItem(c, "talk"));
        ch->volume = (v = cJSON_GetObjectItem(c, "volume")) && cJSON_IsNumber(v) ? v->valueint : 100;
        ch->talkers = (v = cJSON_GetObjectItem(c, "talkers")) && cJSON_IsNumber(v) ? v->valueint : 0;
        // gerade lokal geaendert und Server hat es noch nicht uebernommen: lokalen Stand behalten
        if (n < old_n && old[n].id == ch->id && now < s_pending_until[n]) {
            ch->joined = old[n].joined;
            ch->talk = old[n].talk;
            ch->volume = old[n].volume;
        }
        n++;
    }
    s_st.nch = n;
    s_st.rev++;
    if (cJSON_IsString(juser)) strlcpy(s_st.user, juser->valuestring, sizeof(s_st.user));
    update_talk_locked();
    xSemaphoreGive(s_lock);
    cJSON_Delete(root);
    return true;
}

static void go_offline(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_st.nch = 0;
    s_st.rev++;
    s_st.server_ip[0] = 0;
    g_talk = false;
    xSemaphoreGive(s_lock);
    s_token[0] = 0;
    rtp_update_target();          // zurueck auf das manuell eingestellte RTP-Ziel
}

// ---------------------------------------------------------------- Befehle
static void enqueue(int idx, const char *cmd, int value, void (*apply)(com_channel_t *, int))
{
    cmd_t c = { .value = value };
    strlcpy(c.cmd, cmd, sizeof(c.cmd));
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (idx < 0 || idx >= s_st.nch || s_st.state != COM_ONLINE) {
        xSemaphoreGive(s_lock);
        return;
    }
    c.ch_id = s_ch[idx].id;
    apply(&s_ch[idx], value);                     // sofort anzeigen und hoeren/senden
    s_pending_until[idx] = esp_timer_get_time() + 1500000;
    s_st.rev++;
    update_talk_locked();
    xSemaphoreGive(s_lock);
    if (xQueueSend(s_q, &c, 0) != pdTRUE) ESP_LOGW(TAG, "Befehlswarteschlange voll");
}

static void ap_join(com_channel_t *c, int v)   { c->joined = v; if (!v) c->talk = false; }
static void ap_talk(com_channel_t *c, int v)   { if (c->joined) c->talk = v; }
static void ap_volume(com_channel_t *c, int v) { c->volume = v; }

void com_join(int idx, bool join)   { enqueue(idx, join ? "join" : "leave", join, ap_join); }
void com_talk(int idx, bool on)     { enqueue(idx, "talk", on, ap_talk); }
void com_volume(int idx, int value) { enqueue(idx, "volume", value < 0 ? 0 : value > 100 ? 100 : value, ap_volume); }

void com_talk_all(bool on)
{
    com_channel_t ch[COM_MAX_CH];
    int n = com_get_channels(ch, COM_MAX_CH);
    for (int i = 0; i < n; i++) {
        if (ch[i].joined && ch[i].talk != on) com_talk(i, on);
    }
}

static void cmd_task(void *arg)
{
    static char resp[160];
    char url[96], body[160];
    cmd_t c;
    while (1) {
        xQueueReceive(s_q, &c, portMAX_DELAY);
        if (!s_token[0]) continue;
        snprintf(url, sizeof(url), "%s/cmd", s_base);
        snprintf(body, sizeof(body), "{\"token\":\"%s\",\"cmd\":\"%s\",\"channel\":%d,\"value\":%d}", s_token,
                 c.cmd, c.ch_id, c.value);
        int st = http_req(url, body, resp, sizeof(resp), 3000);
        if (st == 401) s_relogin = true;
        if (st != 200) ESP_LOGW(TAG, "Befehl %s Kanal %d: HTTP %d %s", c.cmd, c.ch_id, st, resp);
    }
}

// ---------------------------------------------------------------- Ablauf
static bool login(const char *ip, uint16_t *rtp_port)
{
    static char resp[384];
    char url[96], body[200], mac[13];
    net_get_mac_hex(mac);
    snprintf(url, sizeof(url), "%s/login", s_base);
    snprintf(body, sizeof(body),
             "{\"pin\":\"%s\",\"deviceId\":\"%s\",\"deviceName\":\"Bodypack %s\",\"rtpPort\":%u,\"ssrc\":%lu}",
             g_set.com_pin, mac, mac + 8, g_set.local_port, (unsigned long)rtp_get_ssrc());
    int st = http_req(url, body, resp, sizeof(resp), 4000);
    if (st == 401) {
        set_state(COM_PIN_WRONG, "PIN falsch");
        return false;
    }
    if (st != 200) {
        char m[48];
        cJSON *e = cJSON_Parse(resp);
        const cJSON *em = e ? cJSON_GetObjectItem(e, "error") : NULL;
        snprintf(m, sizeof(m), "Login: HTTP %d %s", st, cJSON_IsString(em) ? em->valuestring : "");
        cJSON_Delete(e);
        set_state(COM_ERROR, m);
        return false;
    }
    cJSON *root = cJSON_Parse(resp);
    const cJSON *tok = root ? cJSON_GetObjectItem(root, "token") : NULL;
    const cJSON *rtp = root ? cJSON_GetObjectItem(root, "rtp") : NULL;
    const cJSON *port = rtp ? cJSON_GetObjectItem(rtp, "port") : NULL;
    const cJSON *user = root ? cJSON_GetObjectItem(root, "user") : NULL;
    const cJSON *uname = user ? cJSON_GetObjectItem(user, "name") : NULL;
    bool ok = cJSON_IsString(tok) && cJSON_IsNumber(port);
    if (ok) {
        strlcpy(s_token, tok->valuestring, sizeof(s_token));
        *rtp_port = (uint16_t)port->valueint;
        xSemaphoreTake(s_lock, portMAX_DELAY);
        strlcpy(s_st.user, cJSON_IsString(uname) ? uname->valuestring : "", sizeof(s_st.user));
        strlcpy(s_st.server_ip, ip, sizeof(s_st.server_ip));
        xSemaphoreGive(s_lock);
    } else {
        set_state(COM_ERROR, "Login-Antwort unvollstaendig");
    }
    cJSON_Delete(root);
    return ok;
}

static void com_task(void *arg)
{
    static char buf[4096];
    char ip[16], url[160];
    while (1) {
        s_restart = false;
        s_relogin = false;
        if (!g_set.com_on) { set_state(COM_OFF, ""); nap(2000); continue; }
        if (!net_is_connected()) { set_state(COM_NO_WIFI, "warte auf WLAN"); nap(500); continue; }
        if (!g_set.com_host[0]) strlcpy(g_set.com_host, COM_HOST_AUTO, sizeof(g_set.com_host));
        if (!g_set.com_pin[0]) { set_state(COM_PIN_WRONG, "keine PIN eingetragen"); nap(2000); continue; }

        const bool autom = !strcmp(g_set.com_host, COM_HOST_AUTO);
        set_state(COM_SEARCH, autom ? "suche im Netzwerk" : g_set.com_host);
        uint16_t http_port;
        if (!find_server(ip, &http_port)) {
            set_state(COM_SEARCH, autom ? "kein Server im Netzwerk gefunden" : "Servername nicht gefunden");
            nap(2000);
            continue;
        }
        snprintf(s_base, sizeof(s_base), "http://%s:%u/device/v1", ip, http_port);
        snprintf(url, sizeof(url), "%s/hello", s_base);
        if (http_req(url, NULL, buf, sizeof(buf), 3000) != 200) {
            set_state(COM_SEARCH, "Server antwortet nicht");
            nap(3000);
            continue;
        }

        uint16_t rtp_port = 0;
        if (!login(ip, &rtp_port)) { nap(s_st.state == COM_PIN_WRONG ? 10000 : 3000); continue; }

        // Audio: Opus an den PlainTransport des Servers, Mix kommt auf unserem RTP-Port zurueck
        if (g_set.codec != CODEC_OPUS) { g_set.codec = CODEC_OPUS; rtp_codec_changed(); }
        rtp_set_target(ip, rtp_port);
        if (g_set.mode != MODE_RTP) { g_set.mode = MODE_RTP; audio_set_mode(MODE_RTP); }
        set_state(COM_ONLINE, "");

        uint32_t since = 0;
        int fails = 0;
        while (!s_restart && !s_relogin && g_set.com_on) {
            snprintf(url, sizeof(url), "%s/state?token=%s&since=%lu&wait=5000", s_base, s_token, (unsigned long)since);
            int st = http_req(url, NULL, buf, sizeof(buf), 9000);
            if (st == 200 && parse_state(buf, &since)) { fails = 0; continue; }
            if (st == 401) { ESP_LOGW(TAG, "Sitzung abgelaufen, neu anmelden"); break; }
            if (++fails >= 3) { set_state(COM_SEARCH, "Verbindung verloren"); break; }
            nap(1000);
        }
        if (s_restart && s_token[0]) {                 // Abmelden, damit der Server sofort aufraeumt
            char body[80];
            snprintf(url, sizeof(url), "%s/logout", s_base);
            snprintf(body, sizeof(body), "{\"token\":\"%s\"}", s_token);
            http_req(url, body, buf, 64, 2000);
        }
        go_offline();
    }
}

// ---------------------------------------------------------------- Schnittstelle
void com_restart(void)
{
    s_restart = true;
    if (s_task) xTaskNotifyGive(s_task);
}

void com_get_status(com_status_t *st)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *st = s_st;
    xSemaphoreGive(s_lock);
}

int com_get_channels(com_channel_t *out, int max)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    int n = s_st.nch < max ? s_st.nch : max;
    memcpy(out, s_ch, n * sizeof(com_channel_t));
    xSemaphoreGive(s_lock);
    return n;
}

bool com_online(void)
{
    return s_st.state == COM_ONLINE;
}

esp_err_t com_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_q = xQueueCreate(16, sizeof(cmd_t));
    if (!s_lock || !s_q) return ESP_ERR_NO_MEM;
    if (xTaskCreate(com_task, "com", 6144, NULL, 5, &s_task) != pdPASS ||
        xTaskCreate(cmd_task, "com_cmd", 4096, NULL, 5, NULL) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
