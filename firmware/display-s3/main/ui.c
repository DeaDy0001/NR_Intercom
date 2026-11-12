// Anzeige-Texte werden absichtlich auf die Displaybreite gekuerzt
#pragma GCC diagnostic ignored "-Wformat-truncation"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "ui.h"
#include "gfx.h"
#include "input.h"
#include "battery.h"
#include "display.h"
#include "link.h"
#include "ota.h"
#include "nvs.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_chip_info.h"
#include "esp_idf_version.h"
#include "esp_app_desc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ---------------------------------------------------------------- Farben
#define C_BG       RGB(0, 0, 0)
#define C_TILE     RGB(28, 28, 32)
#define C_TILE_HI  RGB(44, 44, 52)
#define C_TEXT     RGB(240, 240, 240)
#define C_DIM      RGB(130, 130, 140)
#define C_LINE     RGB(60, 60, 70)
#define C_LISTEN   RGB(34, 197, 94)
#define C_TALK     RGB(239, 68, 68)
#define C_TALK_BG  RGB(70, 12, 12)
#define C_ACCENT   RGB(56, 189, 248)
#define C_VOL      RGB(200, 200, 210)
#define C_WARN     RGB(250, 204, 21)

// ---------------------------------------------------------------- Zustand
typedef enum { SCR_MAIN, SCR_MENU, SCR_INFO, SCR_TEST, SCR_FPS, SCR_BP, SCR_BPSTAT,
               SCR_DASH, SCR_WIFI, SCR_TEXT, SCR_COMSET, SCR_DISC } screen_t;
static screen_t s_scr = SCR_MAIN;
static int s_menu_sel, s_menu_top;
static int s_brightness = 180;
static bool s_flip;
static int s_bat_mv = -1, s_bat_pct = -1;
static float s_fps;

enum { MI_WIFI, MI_COMSET, MI_DASH, MI_BP, MI_BPSTAT, MI_BRIGHT, MI_FLIP, MI_TEST, MI_FPS, MI_INFO, MI_BACK,
       MI_COUNT };
static const char *s_menu_names[MI_COUNT] = {
    "WLAN", "Intercom-Server", "Audio-Übersicht", "Bodypack-Einstellungen", "Bodypack-Status", "Helligkeit",
    "Display drehen", "Testbild", "FPS-Test", "Info", "< Zurück",
};
static void redraw(void);
static int clampv(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
#define MENU_ROWS 6

// ---------------------------------------------------------------- Bodypack-Einstellungen (ESP32-C5)
static const char *const N_MODE[]  = { "Aus", "Mithören (Loop)", "Testton 1 kHz", "Funk (RTP)" };
static const char *const N_MIC[]   = { "Board-Mic links", "Board-Mic rechts", "Beide (Test L/R)" };
static const char *const N_BOOST[] = { "0 dB", "+13 dB", "+20 dB", "+29 dB" };
static const char *const N_BAND[]  = { "Auto", "5 GHz", "2,4 GHz" };
static const char *const N_ANT[]   = { "intern", "extern" };
static const char *const N_CODEC[] = { "Opus", "L16 (roh)" };
static const char *const N_LATCH[] = { "halten", "umschalten" };

typedef enum { BI_MODE, BI_MIC, BI_PGA, BI_BOOST, BI_GATE, BI_VOL, BI_RXGAIN, BI_SIDE, BI_LATCH, BI_BAND, BI_ANT,
               BI_CODEC, BI_KBPS, BI_FRAME, BI_JB, BI_COUNT } bp_idx_t;

typedef struct {
    const char *key;        // Name im UART-Protokoll
    const char *label;
    int min, max, step;
    const char *const *names;   // Werte als Text (Index = Wert - min), sonst fmt
    const char *fmt;
} bp_item_t;

static const bp_item_t s_bp[BI_COUNT] = {
    [BI_MODE]  = { "mode",  "Modus",            0, 3, 1, N_MODE, NULL },
    [BI_MIC]   = { "mic",   "Mikrofon",         0, 2, 1, N_MIC, NULL },
    [BI_PGA]   = { "pga",   "Mic-Verstärkung",  -17, 30, 1, NULL, "%+d dB" },
    [BI_BOOST] = { "boost", "Mic-Boost",        0, 3, 1, N_BOOST, NULL },
    [BI_GATE]  = { "gate",  "Rausch-Gate",      -80, 0, 2, NULL, "%d dBFS" },
    [BI_VOL]   = { "vol",   "Kopfhörer",        0, 100, 5, NULL, "%d %%" },
    [BI_RXGAIN]= { "rxgain","Hörverstärkung",   0, 24, 3, NULL, "+%d dB" },
    [BI_SIDE]  = { "side",  "Mithörton (Funk)", 0, 100, 5, NULL, "%d %%" },
    [BI_LATCH] = { "latch", "Talk-Taste",       0, 1, 1, N_LATCH, NULL },
    [BI_BAND]  = { "band",  "WLAN-Band",        0, 2, 1, N_BAND, NULL },
    [BI_ANT]   = { "ant",   "Antenne",          0, 1, 1, N_ANT, NULL },
    [BI_CODEC] = { "codec", "Codec",            0, 1, 1, N_CODEC, NULL },
    [BI_KBPS]  = { "kbps",  "Opus-Bitrate",     16, 128, 8, NULL, "%d kbit/s" },
    [BI_FRAME] = { "frame", "Paketlänge",       5, 20, 5, NULL, "%d ms" },
    [BI_JB]    = { "jb",    "Jitterbuffer",     5, 200, 5, NULL, "%d ms" },
};
#define BP_ROWS 5
#define BP_ITEMS (BI_COUNT + 1)          // letzte Zeile: "Zurück"

static int s_bp_val[BI_COUNT];
static int64_t s_bp_touched[BI_COUNT];   // lokale Aenderung, noch nicht vom C5 bestaetigt
static int s_bp_sel, s_bp_top;
static c5_state_t s_c5;

static int bp_state_val(int i, const c5_state_t *c)
{
    switch (i) {
    case BI_MODE: return c->mode;   case BI_MIC: return c->mic;     case BI_PGA: return c->pga;
    case BI_BOOST: return c->boost; case BI_GATE: return c->gate;   case BI_VOL: return c->vol;
    case BI_RXGAIN: return c->rxgain;
    case BI_SIDE: return c->side;   case BI_LATCH: return c->latch; case BI_BAND: return c->band;
    case BI_ANT: return c->ant;     case BI_CODEC: return c->codec; case BI_KBPS: return c->kbps;
    case BI_FRAME: return c->frame; case BI_JB: return c->jb;
    }
    return 0;
}

static void bp_sync(void)
{
    int64_t now = esp_timer_get_time();
    for (int i = 0; i < BI_COUNT; i++) {
        if (now - s_bp_touched[i] > 1500000) s_bp_val[i] = bp_state_val(i, &s_c5);
    }
}

static void bp_format(int i, char *buf, size_t n)
{
    const bp_item_t *it = &s_bp[i];
    int v = s_bp_val[i];
    if (i == BI_GATE && v >= 0) snprintf(buf, n, "aus");
    else if (it->names) snprintf(buf, n, "%s", it->names[v - it->min < 0 ? 0 : v - it->min]);
    else snprintf(buf, n, it->fmt, v);
}

static void bp_change(int i, int steps)
{
    const bp_item_t *it = &s_bp[i];
    int v = s_bp_val[i];
    if (i == BI_GATE) {
        if (v >= 0) v = steps < 0 ? -20 : 0;          // aus -> -20 dBFS
        else {
            v += steps * it->step;
            if (v > -20) v = 0;                        // ueber -20 -> aus
        }
    } else if (i == BI_FRAME) {
        static const int fr[3] = { 5, 10, 20 };
        int k = v <= 5 ? 0 : v <= 10 ? 1 : 2;
        k += steps > 0 ? 1 : -1;
        v = fr[k < 0 ? 0 : k > 2 ? 2 : k];
    } else if (it->names && (steps > 0 ? v >= it->max : v <= it->min)) {
        v = steps > 0 ? it->min : it->max;             // Auswahllisten laufen im Kreis
    } else {
        v += steps * it->step;
    }
    if (v < it->min) v = it->min;
    if (v > it->max) v = it->max;
    if (v == s_bp_val[i]) return;
    s_bp_val[i] = v;
    s_bp_touched[i] = esp_timer_get_time();
    link_set(it->key, v);
}

// ---------------------------------------------------------------- Einstellungen
static void settings_load(void)
{
    nvs_handle_t h;
    if (nvs_open("nrd", NVS_READONLY, &h) != ESP_OK) return;
    uint8_t v;
    if (nvs_get_u8(h, "bright", &v) == ESP_OK) s_brightness = v;
    if (nvs_get_u8(h, "flip", &v) == ESP_OK) s_flip = v;
    nvs_close(h);
}

static void settings_save(void)
{
    nvs_handle_t h;
    if (nvs_open("nrd", NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "bright", (uint8_t)s_brightness);
    nvs_set_u8(h, "flip", s_flip);
    nvs_commit(h);
    nvs_close(h);
}

// ---------------------------------------------------------------- Zeichnen
static void draw_pill(int x, int y, int w, int h, const font_t *f, const char *txt, color_t fg, color_t bg)
{
    gfx_round_rect(x, y, w, h, h / 2, bg);
    gfx_text_box(f, x, y + (h - f->line_h) / 2, w, ALIGN_CENTER, txt, fg);
}

static void draw_battery(void)
{
    gfx_fill_rect(380, 0, DISP_W - 380, 32, C_BG);
    char buf[24];
    int x = DISP_W - 52;
    if (s_bat_mv < 0) {
        snprintf(buf, sizeof(buf), "--");
    } else if (s_bat_mv > 4400) {
        snprintf(buf, sizeof(buf), "USB");
    } else {
        snprintf(buf, sizeof(buf), "%d %%", s_bat_pct);
    }
    gfx_rect(x, 9, 38, 16, 2, C_DIM);
    gfx_fill_rect(x + 38, 13, 3, 8, C_DIM);
    int pct = s_bat_mv > 4400 ? 100 : (s_bat_pct < 0 ? 0 : s_bat_pct);
    color_t fill = pct < 15 ? C_TALK : (pct < 30 ? C_WARN : C_LISTEN);
    gfx_fill_rect(x + 3, 12, 32 * pct / 100, 10, fill);
    gfx_text_box(&font_s, 380, 7, x - 380 - 8, ALIGN_RIGHT, buf, C_TEXT);
}

static void draw_header(void)
{
    gfx_fill_rect(0, 0, DISP_W, 32, C_BG);
    gfx_text(&font_s, 10, 7, "NR INTERCOM", C_ACCENT);
    char st[48];
    color_t sc = C_DIM;
    if (!s_c5.online) {
        snprintf(st, sizeof(st), "Bodypack: keine Verbindung");
        sc = C_WARN;
    } else if (!s_c5.wifi) {
        snprintf(st, sizeof(st), "Bodypack ok · WLAN aus");
    } else if (s_c5.com_state == COM_ST_ONLINE) {
        snprintf(st, sizeof(st), "%.22s · %d dBm", s_c5.com_user, s_c5.rssi);
        sc = C_LISTEN;
    } else if (s_c5.com_state != COM_ST_OFF) {
        snprintf(st, sizeof(st), "WLAN ok · Intercom: %s",
                 s_c5.com_state == COM_ST_PIN ? "PIN?" : s_c5.com_state == COM_ST_NOWIFI ? "warte" : "sucht");
        sc = C_WARN;
    } else {
        snprintf(st, sizeof(st), "WLAN %d dBm · %s", s_c5.rssi, s_c5.band_5g ? "5 GHz" : "2,4 GHz");
        sc = C_LISTEN;
    }
    gfx_text_box(&font_s, 140, 7, 240, ALIGN_CENTER, st, sc);
    draw_battery();
    gfx_hline(0, 32, DISP_W, C_LINE);
}

// Pegelbalken von lo..0 dB, optional mit Spitzenwert, Gate-Schwelle und Uebersteuerungs-Anzeige
static void draw_meter(int x, int y, int w, int h, float db, float lo, float peak_db, int gate_db, bool clip,
                       color_t col)
{
    gfx_fill_rect(x, y, w, h, RGB(18, 18, 22));
    int lw = (int)((db - lo) / -lo * w);
    lw = lw < 0 ? 0 : lw > w ? w : lw;
    if (lw > 0) gfx_fill_rect(x, y, lw, h, col);
    // Skala alle 10 dB
    for (int d = (int)lo + 10; d < 0; d += 10) gfx_fill_rect(x + (int)((d - lo) / -lo * w), y + h, 1, 4, C_LINE);
    if (peak_db > lo) {
        int px = x + (int)((peak_db - lo) / -lo * w);
        if (px > x + w - 2) px = x + w - 2;
        gfx_fill_rect(px, y, 2, h, peak_db > -3 ? C_TALK : C_TEXT);
    }
    if (gate_db < 0 && gate_db > lo) {
        int gx = x + (int)((gate_db - lo) / -lo * w);
        gfx_fill_rect(gx - 1, y - 4, 3, h + 8, C_ACCENT);
    }
    if (clip) {
        gfx_round_rect(x + w + 6, y - 2, 44, h + 4, 4, C_TALK);
        gfx_text_box(&font_s, x + w + 6, y + (h - 19) / 2, 44, ALIGN_CENTER, "CLIP", C_TEXT);
    }
}

static int64_t s_clip_until;     // CLIP-Anzeige bis zu diesem Zeitpunkt
static uint32_t s_last_clips;

static void draw_main(void)
{
    gfx_clear(C_BG);
    draw_header();
    const c5_state_t *c = &s_c5;
    const bool on = c->online && c->have_settings;

    // Talk links gross
    bool talk = on && c->talk;
    draw_pill(12, 44, 150, 56, &font_l, talk ? "TALK" : "Talk aus", talk ? C_TEXT : C_DIM,
              talk ? C_TALK : C_TILE);
    // Modus und Mikrofon
    char b[64];
    snprintf(b, sizeof(b), "%s", on ? N_MODE[c->mode < 0 || c->mode > 3 ? 0 : c->mode] : "--");
    gfx_text(&font_m, 180, 44, b, C_TEXT);
    snprintf(b, sizeof(b), "%s · Codec %s", on ? N_MIC[c->mic < 0 || c->mic > 2 ? 0 : c->mic] : "--",
             on ? (c->codec_ok ? "ok" : "FEHLT") : "--");
    gfx_text(&font_s, 180, 76, b, on && !c->codec_ok ? C_WARN : C_DIM);

    // Pegel
    int64_t now = esp_timer_get_time();
    if (c->clips != s_last_clips) {
        if (s_last_clips) s_clip_until = now + 1500000;
        s_last_clips = c->clips;
    }
    float mic_db = on ? (c->mic == 1 ? c->jack_db : c->board_db) : -120;
    bool gate_open = c->gate_gain_db > -3.0f;
    gfx_text(&font_s, 12, 114, "Mikrofon", C_DIM);
    draw_meter(100, 116, 370, 14, mic_db, -80, on ? c->peak_db : -120, on ? c->gate : 0, on && now < s_clip_until,
               gate_open ? C_LISTEN : C_DIM);
    gfx_text(&font_s, 12, 144, "Kopfhörer", C_DIM);
    draw_meter(100, 146, 370, 14, on ? c->out_db : -120, -80, -120, 0, false, C_VOL);

    // Werte
    if (on) {
        snprintf(b, sizeof(b), "Lautstärke %d %%", c->vol);
        gfx_text(&font_s, 12, 176, b, C_TEXT);
        snprintf(b, sizeof(b), "Mic %+d dB, Boost %s", c->pga, N_BOOST[c->boost & 3]);
        gfx_text_box(&font_s, 160, 176, 220, ALIGN_CENTER, b, C_TEXT);
        if (c->gate < 0) snprintf(b, sizeof(b), "Gate %d dBFS", c->gate);
        else snprintf(b, sizeof(b), "Gate aus");
        gfx_text_box(&font_s, 380, 176, DISP_W - 392, ALIGN_RIGHT, b, C_TEXT);
    } else {
        gfx_text_box(&font_s, 0, 176, DISP_W, ALIGN_CENTER,
                     "Kabel prüfen: GPIO4 > GP3, GPIO10 < GP2, GND", C_WARN);
    }

    gfx_hline(0, 204, DISP_W, C_LINE);
    gfx_text_box(&font_s, 10, 214, 180, ALIGN_LEFT, "L: Lautst. / lang Talk", C_DIM);
    gfx_text_box(&font_s, 190, 214, 156, ALIGN_CENTER, "drücken: Menü", C_ACCENT);
    gfx_text_box(&font_s, 346, 214, DISP_W - 356, ALIGN_RIGHT, "R: Mic / lang Modus", C_DIM);
}

static void draw_menu(void)
{
    gfx_clear(C_BG);
    gfx_text(&font_l, 16, 6, "Menü", C_TEXT);
    gfx_text_box(&font_s, 140, 14, DISP_W - 156, ALIGN_RIGHT, "L: wählen · L drücken: OK", C_DIM);
    gfx_hline(0, 46, DISP_W, C_LINE);
    if (s_menu_sel < s_menu_top) s_menu_top = s_menu_sel;
    if (s_menu_sel >= s_menu_top + MENU_ROWS) s_menu_top = s_menu_sel - MENU_ROWS + 1;
    for (int i = s_menu_top; i < MI_COUNT && i < s_menu_top + MENU_ROWS; i++) {
        int y = 54 + (i - s_menu_top) * 30;
        bool sel = i == s_menu_sel;
        if (sel) gfx_round_rect(8, y - 2, DISP_W - 16, 28, 6, C_TILE_HI);
        gfx_text(&font_m, 20, y, s_menu_names[i], sel ? C_TEXT : C_DIM);
        char val[24] = "";
        if (i == MI_BRIGHT) snprintf(val, sizeof(val), "%d %%", s_brightness * 100 / 255);
        if (i == MI_FLIP) snprintf(val, sizeof(val), "%s", s_flip ? "gedreht" : "normal");
        if (i == MI_BP || i == MI_BPSTAT) snprintf(val, sizeof(val), "%s", s_c5.online ? "verbunden" : "offline");
        if (i == MI_WIFI) snprintf(val, sizeof(val), "%.20s", !s_c5.online ? "--" : s_c5.wifi ? s_c5.ssid : "nicht verbunden");
        if (i == MI_COMSET) snprintf(val, sizeof(val), "%s", !s_c5.online ? "--" : s_c5.com_state == COM_ST_ONLINE ? "verbunden" :
                                     s_c5.com_state == COM_ST_OFF ? "aus" : s_c5.com_state == COM_ST_PIN ? "PIN falsch" : "sucht");
        if (val[0]) gfx_text_box(&font_m, 300, y, DISP_W - 330, ALIGN_RIGHT, val, sel ? C_ACCENT : C_DIM);
    }
}

// Pegelanzeige -80..0 dBFS mit Gate-Schwelle
static void draw_bp_meter(void)
{
    const int x = 16, y = 52, w = DISP_W - 32, h = 14;
    gfx_fill_rect(0, 48, DISP_W, 32, C_BG);
    float db = s_c5.mic == 1 ? s_c5.jack_db : s_c5.board_db;
    if (!s_c5.online) db = -120;
    int lw = (int)((db + 80.0f) / 80.0f * w);
    lw = lw < 0 ? 0 : lw > w ? w : lw;
    bool open = s_c5.gate_gain_db > -3.0f;
    gfx_fill_rect(x, y, w, h, RGB(15, 15, 18));
    if (lw > 0) gfx_fill_rect(x, y, lw, h, db > -12 ? C_WARN : (open ? C_LISTEN : C_DIM));
    if (s_bp_val[BI_GATE] < 0) {
        int gx = x + (s_bp_val[BI_GATE] + 80) * w / 80;
        gfx_fill_rect(gx - 1, y - 3, 3, h + 6, C_TALK);
    }
    char b[64];
    snprintf(b, sizeof(b), "Mikrofon %.0f dBFS · Gate %s", db < -99 ? -99.0f : db,
             s_bp_val[BI_GATE] >= 0 ? "aus" : (open ? "offen" : "zu"));
    gfx_text(&font_s, x, y + h + 1, b, C_DIM);
}

static void draw_bp(void)
{
    gfx_clear(C_BG);
    gfx_text(&font_l, 16, 6, "Bodypack", C_TEXT);
    gfx_text_box(&font_s, 180, 14, DISP_W - 196, ALIGN_RIGHT,
                 s_c5.online ? "L: wählen + drücken · R: Wert ändern" : "keine Verbindung zum C5",
                 s_c5.online ? C_DIM : C_WARN);
    gfx_hline(0, 46, DISP_W, C_LINE);
    draw_bp_meter();
    if (s_bp_sel < s_bp_top) s_bp_top = s_bp_sel;
    if (s_bp_sel >= s_bp_top + BP_ROWS) s_bp_top = s_bp_sel - BP_ROWS + 1;
    for (int i = s_bp_top; i < BP_ITEMS && i < s_bp_top + BP_ROWS; i++) {
        int y = 86 + (i - s_bp_top) * 30;
        bool sel = i == s_bp_sel;
        if (sel) gfx_round_rect(8, y - 2, DISP_W - 16, 28, 6, C_TILE_HI);
        if (i == BI_COUNT) {                              // Zurueck-Knopf
            gfx_text(&font_m, 20, y, "< Zurück", sel ? C_ACCENT : C_DIM);
            gfx_text_box(&font_s, 260, y + 4, DISP_W - 290, ALIGN_RIGHT, "links drücken", sel ? C_TEXT : C_DIM);
            continue;
        }
        gfx_text(&font_m, 20, y, s_bp[i].label, sel ? C_TEXT : C_DIM);
        char val[32];
        if (s_c5.have_settings) bp_format(i, val, sizeof(val));
        else snprintf(val, sizeof(val), "--");
        bool pending = esp_timer_get_time() - s_bp_touched[i] < 1500000 && s_bp_val[i] != bp_state_val(i, &s_c5);
        gfx_text_box(&font_m, 260, y, DISP_W - 290, ALIGN_RIGHT, val, pending ? C_WARN : (sel ? C_ACCENT : C_DIM));
    }
    // Scroll-Hinweis
    if (s_bp_top > 0) gfx_text(&font_s, DISP_W - 20, 82, "^", C_DIM);
    if (s_bp_top + BP_ROWS < BP_ITEMS) gfx_text(&font_s, DISP_W - 20, 222, "v", C_DIM);
}

// "Zurück"-Knopf unten rechts auf Seiten ohne Auswahl (jeder Druck auf einen Drehgeber geht zurück)
static void draw_back_button(void)
{
    draw_pill(DISP_W - 150, DISP_H - 34, 138, 30, &font_m, "< Zurück", C_BG, C_ACCENT);
}

static void draw_bpstat(void)
{
    gfx_clear(C_BG);
    gfx_text(&font_l, 16, 6, "Bodypack-Status", C_TEXT);
    gfx_hline(0, 46, DISP_W, C_LINE);
    const c5_state_t *c = &s_c5;
    char l[8][160];
    snprintf(l[0], 160, "Verbindung: %s (%lu Zeilen empfangen)", c->online ? "ok" : "KEINE",
             (unsigned long)c->lines);
    snprintf(l[1], 160, "Audio-Codec WM8960: %s", c->codec_ok ? "ok" : "nicht gefunden");
    if (c->wifi)
        snprintf(l[2], 160, "WLAN: %s · %s · %d dBm · Kanal %d (%s)", c->ssid, c->ip, c->rssi, c->channel,
                 c->band_5g ? "5 GHz" : "2,4 GHz");
    else
        snprintf(l[2], 160, "WLAN: nicht verbunden (SSID %s)", c->ssid);
    snprintf(l[3], 160, "Mikrofon: links %.1f dBFS · rechts %.1f dBFS", c->board_db, c->jack_db);
    snprintf(l[4], 160, "Spitze Mic %.1f dBFS · Ausgang %.1f dBFS", c->peak_db, c->out_db);
    snprintf(l[5], 160, "Gate dämpft gerade %.0f dB · übersteuert %lu ms · I2S-Aussetzer %lu",
             c->gate_gain_db < -0.5f ? -c->gate_gain_db : 0.0f, (unsigned long)c->clips, (unsigned long)c->xruns);
    snprintf(l[6], 160, "Modus: %s · Talk %s", N_MODE[c->mode < 0 || c->mode > 3 ? 0 : c->mode],
             c->talk ? "AN" : "aus");
    for (int i = 0; i < 7; i++) gfx_text(&font_s, 16, 54 + i * 21, l[i], C_TEXT);
    draw_back_button();
}

static void draw_info(void)
{
    gfx_clear(C_BG);
    gfx_text(&font_l, 16, 6, "Info", C_TEXT);
    gfx_hline(0, 46, DISP_W, C_LINE);
    esp_chip_info_t ci;
    esp_chip_info(&ci);
    int e1, e2;
    input_raw_counts(&e1, &e2);
    char l[8][80];
    snprintf(l[0], 80, "Chip: ESP32-S3 Rev. %d.%d, %d Kerne, ESP-IDF %s", ci.revision / 100, ci.revision % 100,
             ci.cores, esp_get_idf_version());
    snprintf(l[1], 80, "RAM intern frei: %u KB, PSRAM frei: %u KB",
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
             (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    snprintf(l[2], 80, "Akku: %d mV (%d %%)", s_bat_mv, s_bat_pct);
    snprintf(l[3], 80, "Drehgeber Rohzähler: 1 = %d, 2 = %d", e1, e2);
    snprintf(l[4], 80, "Display: letztes Update %lu µs", (unsigned long)display_last_push_us());
    snprintf(l[5], 80, "Laufzeit: %llu s", (unsigned long long)(esp_timer_get_time() / 1000000));
    snprintf(l[6], 80, "Funk-Modul (ESP32-C5): %s", s_c5.online ? "verbunden" : "keine Verbindung (UART)");
    for (int i = 0; i < 7; i++) gfx_text(&font_s, 16, 54 + i * 21, l[i], C_TEXT);
    draw_back_button();
}

static void draw_test(void)
{
    static const uint8_t bars[8][3] = {
        { 255, 255, 255 }, { 255, 255, 0 }, { 0, 255, 255 }, { 0, 255, 0 },
        { 255, 0, 255 }, { 255, 0, 0 }, { 0, 0, 255 }, { 0, 0, 0 },
    };
    gfx_clear(C_BG);
    int bw = DISP_W / 8;
    for (int i = 0; i < 8; i++) gfx_fill_rect(i * bw, 0, bw + 1, 110, RGB(bars[i][0], bars[i][1], bars[i][2]));
    for (int x = 0; x < DISP_W; x++) {
        int v = x * 255 / (DISP_W - 1);
        gfx_fill_rect(x, 110, 1, 25, RGB(v, 0, 0));
        gfx_fill_rect(x, 135, 1, 25, RGB(0, v, 0));
        gfx_fill_rect(x, 160, 1, 25, RGB(0, 0, v));
        gfx_fill_rect(x, 185, 1, 25, RGB(v, v, v));
    }
    gfx_rect(0, 0, DISP_W, DISP_H, 1, C_TEXT);
    gfx_fill_rect(0, 212, DISP_W, 28, C_BG);
    gfx_text(&font_s, 6, 216, "oben links ist die weisse Farbleiste", C_TEXT);
    gfx_text_box(&font_s, 300, 216, DISP_W - 306, ALIGN_RIGHT, "Taster: zurück", C_DIM);
}

static void draw_fps(uint32_t frame)
{
    gfx_clear(C_BG);
    int x = (int)(frame * 6) % (DISP_W + 80) - 80;
    gfx_fill_rect(x, 50, 80, 140, C_ACCENT);
    char buf[64];
    snprintf(buf, sizeof(buf), "%.1f FPS", s_fps);
    gfx_text_box(&font_xl, 0, 70, DISP_W, ALIGN_CENTER, buf, C_TEXT);
    snprintf(buf, sizeof(buf), "Vollbild-Update: %lu µs  ·  Taster: zurück", (unsigned long)display_last_push_us());
    gfx_text_box(&font_s, 0, 212, DISP_W, ALIGN_CENTER, buf, C_DIM);
}


// ================================================================ Intercom
static bool com_on_line(void) { return s_c5.online && s_c5.com_state == COM_ST_ONLINE; }

static const char *com_state_text(int st)
{
    switch (st) {
    case COM_ST_OFF:    return "ausgeschaltet";
    case COM_ST_NOWIFI: return "wartet auf WLAN";
    case COM_ST_SEARCH: return "sucht Server";
    case COM_ST_PIN:    return "PIN abgelehnt";
    case COM_ST_ONLINE: return "verbunden";
    default:            return "Fehler";
    }
}

// ---------------------------------------------------------------- Kanalliste (Hauptseite, wenn verbunden)
static link_channel_t s_chl[LINK_MAX_CH];
static int s_nchl, s_ch_sel, s_ch_top;
#define CH_ROWS 5

static void draw_channels(void)
{
    gfx_clear(C_BG);
    draw_header();
    s_nchl = link_get_channels(s_chl, LINK_MAX_CH, NULL);
    const int items = s_nchl + 1;                         // letzte Zeile: Menue
    s_ch_sel = clampv(s_ch_sel, 0, items - 1);
    if (s_ch_sel < s_ch_top) s_ch_top = s_ch_sel;
    if (s_ch_sel >= s_ch_top + CH_ROWS) s_ch_top = s_ch_sel - CH_ROWS + 1;
    for (int i = s_ch_top; i < items && i < s_ch_top + CH_ROWS; i++) {
        const int y = 37 + (i - s_ch_top) * 33;
        const bool sel = i == s_ch_sel;
        if (i == s_nchl) {
            gfx_round_rect(8, y, DISP_W - 16, 31, 8, sel ? C_TILE_HI : C_BG);
            gfx_text(&font_m, 36, y + 2, "Menü", sel ? C_ACCENT : C_DIM);
            gfx_text_box(&font_s, 260, y + 6, DISP_W - 280, ALIGN_RIGHT, "WLAN, Server, Einstellungen", C_DIM);
            continue;
        }
        const link_channel_t *c = &s_chl[i];
        color_t bg = c->talk ? C_TALK_BG : (sel ? C_TILE_HI : C_TILE);
        if (sel) {
            gfx_round_rect(8, y, DISP_W - 16, 31, 8, C_TEXT);
            gfx_round_rect(10, y + 2, DISP_W - 20, 27, 7, bg);
        } else {
            gfx_round_rect(8, y, DISP_W - 16, 31, 8, bg);
        }
        if (c->talkers > 0) gfx_round_rect(18, y + 10, 11, 11, 5, C_WARN);   // jemand spricht
        gfx_text(&font_m, 36, y + 2, c->name, c->joined ? C_TEXT : C_DIM);
        if (c->joined) draw_pill(262, y + 4, 84, 23, &font_s, "HÖRT", C_BG, C_LISTEN);
        else draw_pill(262, y + 4, 84, 23, &font_s, "aus", C_DIM, RGB(50, 50, 58));
        if (c->talk) draw_pill(352, y + 4, 72, 23, &font_s, "TALK", C_TEXT, C_TALK);
        // Lautstaerke
        gfx_fill_rect(432, y + 13, 56, 6, RGB(15, 15, 18));
        gfx_fill_rect(432, y + 13, 56 * clampv(c->volume, 0, 100) / 100, 6, c->joined ? C_VOL : C_DIM);
        char v[8];
        snprintf(v, sizeof(v), "%d", c->volume);
        gfx_text_box(&font_s, 488, y + 6, 36, ALIGN_RIGHT, v, C_DIM);
    }
    if (s_nchl == 0) gfx_text_box(&font_m, 0, 90, DISP_W, ALIGN_CENTER, "Keine Kanäle im Projekt", C_DIM);
    gfx_hline(0, 206, DISP_W, C_LINE);
    gfx_text_box(&font_s, 10, 214, 330, ALIGN_LEFT, "L: wählen · drücken: Hören · lang: Talk", C_DIM);
    gfx_text_box(&font_s, 340, 214, DISP_W - 350, ALIGN_RIGHT, "R: Lautstärke", C_DIM);
}

static void handle_channels(const input_ev_t *ev)
{
    const int items = s_nchl + 1;
    const int i = s_ch_sel;
    const bool is_ch = i < s_nchl;
    if (ev->type == EV_ENC1_TURN) {
        s_ch_sel = clampv(s_ch_sel + ev->value, 0, items - 1);
    } else if (ev->type == EV_ENC1_PRESS) {
        if (!is_ch) { s_scr = SCR_MENU; s_menu_sel = 0; s_menu_top = 0; redraw(); return; }
        link_channel(i, s_chl[i].joined ? "leave" : "join", 0);
    } else if (ev->type == EV_ENC1_LONG || ev->type == EV_ENC2_LONG) {
        if (is_ch) {
            if (!s_chl[i].joined) link_channel(i, "join", 0);
            else link_channel(i, "talk", !s_chl[i].talk);
        }
    } else if (ev->type == EV_ENC2_TURN && is_ch) {
        link_channel(i, "vol", clampv(s_chl[i].volume + ev->value * 5, 0, 100));
    } else if (ev->type == EV_ENC2_PRESS) {
        s_scr = SCR_MENU; s_menu_sel = 0; s_menu_top = 0; redraw(); return;
    }
    draw_channels();
}

// ---------------------------------------------------------------- Texteingabe mit Zeichenrad
enum { CS_FULL, CS_HOST, CS_DIGITS };
enum { TX_WIFI_PASS, TX_COM_HOST, TX_COM_PORT, TX_COM_PIN };
#define TX_SPECIAL 3                                       // 0 = OK, 1 = Löschen, 2 = Abbrechen
static const char *const CS_GROUPS[3][4] = {
    { "abcdefghijklmnopqrstuvwxyz", "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "0123456789", " .-_@!#$%&*+/=?:;,()[]{}<>^~|'\"\\`" },
    { "abcdefghijklmnopqrstuvwxyz", "0123456789.-", NULL, NULL },
    { "0123456789", NULL, NULL, NULL },
};
static const char *const CS_GROUP_NAMES[3][4] = {
    { "abc", "ABC", "123", "#+=" }, { "abc", "123.-", NULL, NULL }, { "123", NULL, NULL, NULL },
};
static struct {
    char title[48];
    char buf[65];
    int  max, cs, target, sel;
    screen_t back;
} s_tx;

static int tx_count(void)
{
    int n = TX_SPECIAL;
    for (int g = 0; g < 4 && CS_GROUPS[s_tx.cs][g]; g++) n += strlen(CS_GROUPS[s_tx.cs][g]);
    return n;
}

static int tx_group_of(int idx, int *start)
{
    int n = TX_SPECIAL;
    if (idx < TX_SPECIAL) { if (start) *start = 0; return -1; }
    for (int g = 0; g < 4 && CS_GROUPS[s_tx.cs][g]; g++) {
        int l = strlen(CS_GROUPS[s_tx.cs][g]);
        if (idx < n + l) { if (start) *start = n; return g; }
        n += l;
    }
    return -1;
}

static int tx_group_start(int g)
{
    int n = TX_SPECIAL;
    for (int k = 0; k < g && CS_GROUPS[s_tx.cs][k]; k++) n += strlen(CS_GROUPS[s_tx.cs][k]);
    return n;
}

static int tx_groups(void)
{
    int g = 0;
    while (g < 4 && CS_GROUPS[s_tx.cs][g]) g++;
    return g;
}

static char tx_char(int idx)
{
    int start;
    int g = tx_group_of(idx, &start);
    return g < 0 ? 0 : CS_GROUPS[s_tx.cs][g][idx - start];
}

static void tx_label(int idx, bool big, char *out, size_t n)
{
    static const char *const sp_big[TX_SPECIAL] = { "OK", "Löschen", "Abbruch" };
    static const char *const sp_small[TX_SPECIAL] = { "OK", "<-", "X" };
    if (idx < TX_SPECIAL) { snprintf(out, n, "%s", big ? sp_big[idx] : sp_small[idx]); return; }
    char c = tx_char(idx);
    if (c == ' ') snprintf(out, n, "%s", big ? "Leerz." : "_");
    else snprintf(out, n, "%c", c);
}

static void text_open(const char *title, const char *init, int max, int cs, int target, screen_t back)
{
    snprintf(s_tx.title, sizeof(s_tx.title), "%s", title);
    snprintf(s_tx.buf, sizeof(s_tx.buf), "%s", init ? init : "");
    s_tx.max = max < (int)sizeof(s_tx.buf) - 1 ? max : (int)sizeof(s_tx.buf) - 1;
    s_tx.cs = cs;
    s_tx.target = target;
    s_tx.back = back;
    s_tx.sel = TX_SPECIAL;                                 // erstes Zeichen
    s_scr = SCR_TEXT;
    redraw();
}

static void draw_text_entry(void)
{
    gfx_clear(C_BG);
    gfx_text_box(&font_m, 16, 8, DISP_W - 32, ALIGN_LEFT, s_tx.title, C_TEXT);
    // Eingabefeld (bei langem Text das Ende zeigen)
    gfx_round_rect(12, 42, DISP_W - 24, 46, 8, C_TILE);
    const char *t = s_tx.buf;
    while (*t && gfx_text_width(&font_l, t) > DISP_W - 70) t++;
    int w = gfx_text(&font_l, 24, 47, t, C_TEXT);
    gfx_fill_rect(24 + w + 2, 80, 16, 3, C_ACCENT);       // Cursor
    char cnt[16];
    snprintf(cnt, sizeof(cnt), "%d/%d", (int)strlen(s_tx.buf), s_tx.max);
    gfx_text_box(&font_s, DISP_W - 90, 92, 76, ALIGN_RIGHT, cnt, C_DIM);
    // Zeichenrad
    const int cx = DISP_W / 2, cy = 112, n = tx_count();
    char lb[16];
    for (int k = -5; k <= 5; k++) {
        int idx = s_tx.sel + k;
        if (k == 0 || idx < 0 || idx >= n) continue;
        tx_label(idx, false, lb, sizeof(lb));
        int x = cx + k * 44 + (k > 0 ? 30 : -30);
        gfx_text_box(&font_m, x - 20, cy + 14, 40, ALIGN_CENTER, lb, idx < TX_SPECIAL ? C_ACCENT : C_DIM);
    }
    tx_label(s_tx.sel, true, lb, sizeof(lb));
    int bw = gfx_text_width(&font_l, lb) + 28;
    if (bw < 56) bw = 56;
    gfx_round_rect(cx - bw / 2, cy, bw, 50, 10, s_tx.sel < TX_SPECIAL ? C_ACCENT : C_TEXT);
    gfx_text_box(&font_l, cx - bw / 2, cy + 7, bw, ALIGN_CENTER, lb, C_BG);
    // Gruppen
    int g = tx_group_of(s_tx.sel, NULL), ng = tx_groups();
    int gx = 16;
    for (int k = 0; k < ng; k++) {
        const char *gn = CS_GROUP_NAMES[s_tx.cs][k];
        int gw = gfx_text_width(&font_s, gn) + 16;
        if (k == g) gfx_round_rect(gx, 176, gw, 22, 6, C_TILE_HI);
        gfx_text(&font_s, gx + 8, 178, gn, k == g ? C_TEXT : C_DIM);
        gx += gw + 6;
    }
    gfx_hline(0, 206, DISP_W, C_LINE);
    gfx_text_box(&font_s, 10, 214, 360, ALIGN_LEFT, "L: drehen + drücken = Zeichen · lang = löschen", C_DIM);
    gfx_text_box(&font_s, 360, 214, DISP_W - 370, ALIGN_RIGHT, "R: Gruppe", C_DIM);
}

// Intercom-Server: lokale Kopie, wird mit "Übernehmen" (oder beim Verlassen) an den C5 geschickt
static struct {
    int  on, port;
    char host[48], pin[16];
    bool dirty;
    char msg[48];
} s_cs;

static char s_wifi_ssid_sel[33];
static char s_wifi_msg[64];
static int64_t s_wifi_msg_t;

static void tx_finish(bool ok)
{
    if (ok) {
        switch (s_tx.target) {
        case TX_WIFI_PASS:
            link_wifi(s_wifi_ssid_sel, s_tx.buf);
            snprintf(s_wifi_msg, sizeof(s_wifi_msg), "Verbinde mit %.30s ...", s_wifi_ssid_sel);
            s_wifi_msg_t = esp_timer_get_time();
            break;
        case TX_COM_HOST: snprintf(s_cs.host, sizeof(s_cs.host), "%s", s_tx.buf); s_cs.dirty = true; break;
        case TX_COM_PORT: s_cs.port = clampv(atoi(s_tx.buf), 1, 65535); s_cs.dirty = true; break;
        case TX_COM_PIN:  snprintf(s_cs.pin, sizeof(s_cs.pin), "%s", s_tx.buf); s_cs.dirty = true; break;
        }
    }
    s_scr = s_tx.back;
    redraw();
}

static void handle_text(const input_ev_t *ev)
{
    const int n = tx_count();
    size_t len = strlen(s_tx.buf);
    if (ev->type == EV_ENC1_TURN) {
        s_tx.sel = clampv(s_tx.sel + ev->value, 0, n - 1);
    } else if (ev->type == EV_ENC1_PRESS) {
        if (s_tx.sel == 0) { tx_finish(true); return; }
        if (s_tx.sel == 2) { tx_finish(false); return; }
        if (s_tx.sel == 1) { if (len) s_tx.buf[len - 1] = 0; }
        else if ((int)len < s_tx.max) { s_tx.buf[len] = tx_char(s_tx.sel); s_tx.buf[len + 1] = 0; }
    } else if (ev->type == EV_ENC1_LONG) {
        if (len) s_tx.buf[len - 1] = 0;
    } else if (ev->type == EV_ENC2_TURN) {
        int g = tx_group_of(s_tx.sel, NULL) + (ev->value > 0 ? 1 : -1);
        s_tx.sel = g < 0 ? 0 : tx_group_start(clampv(g, 0, tx_groups() - 1));
    } else if (ev->type == EV_ENC2_PRESS) {
        tx_finish(true);
        return;
    }
    draw_text_entry();
}

// ---------------------------------------------------------------- WLAN
static link_ap_t s_aps[LINK_MAX_AP];
static int s_naps, s_wsel, s_wtop;
#define W_ROWS 5
// Zeilen: 0 = Neu suchen, 1 = Band, 2.. = Netze, letzte = Zurück

static void draw_wifi(void)
{
    gfx_clear(C_BG);
    gfx_text(&font_l, 16, 6, "WLAN", C_TEXT);
    char st[64];
    color_t sc = C_DIM;
    if (s_wifi_msg[0] && esp_timer_get_time() - s_wifi_msg_t < 8000000 && !s_c5.wifi) {
        snprintf(st, sizeof(st), "%s", s_wifi_msg);
        sc = C_WARN;
    } else if (s_c5.wifi) {
        snprintf(st, sizeof(st), "verbunden: %.24s (%d dBm)", s_c5.ssid, s_c5.rssi);
        sc = C_LISTEN;
    } else {
        snprintf(st, sizeof(st), "%s", s_c5.online ? "nicht verbunden" : "Bodypack nicht verbunden");
        sc = C_WARN;
    }
    gfx_text_box(&font_s, 120, 14, DISP_W - 136, ALIGN_RIGHT, st, sc);
    gfx_hline(0, 46, DISP_W, C_LINE);
    bool busy = link_scan_busy();
    if (!busy) s_naps = link_get_scan(s_aps, LINK_MAX_AP);
    const int items = 2 + (busy ? 1 : s_naps) + 1;
    s_wsel = clampv(s_wsel, 0, items - 1);
    if (s_wsel < s_wtop) s_wtop = s_wsel;
    if (s_wsel >= s_wtop + W_ROWS) s_wtop = s_wsel - W_ROWS + 1;
    for (int i = s_wtop; i < items && i < s_wtop + W_ROWS; i++) {
        int y = 54 + (i - s_wtop) * 30;
        bool sel = i == s_wsel;
        if (sel) gfx_round_rect(8, y - 2, DISP_W - 16, 28, 6, C_TILE_HI);
        char l[48] = "", v[40] = "";
        color_t lc = sel ? C_TEXT : C_DIM;
        if (i == 0) {
            snprintf(l, sizeof(l), "%s", busy ? "suche ..." : "Neu suchen");
            lc = sel ? C_ACCENT : C_DIM;
        } else if (i == 1) {
            snprintf(l, sizeof(l), "Band");
            snprintf(v, sizeof(v), "%s", N_BAND[clampv(s_c5.band, 0, 2)]);
        } else if (i == items - 1) {
            snprintf(l, sizeof(l), "< Zurück");
            lc = sel ? C_ACCENT : C_DIM;
        } else if (busy) {
            snprintf(l, sizeof(l), "Netzwerke werden gesucht ...");
        } else {
            const link_ap_t *a = &s_aps[i - 2];
            snprintf(l, sizeof(l), "%.28s", a->ssid);
            snprintf(v, sizeof(v), "%d dBm · %s", a->rssi, a->secure ? "Passwort" : "offen");
            if (!strcmp(a->ssid, s_c5.wifi_ssid)) lc = sel ? C_LISTEN : RGB(30, 150, 70);
        }
        gfx_text(&font_m, 20, y, l, lc);
        if (v[0]) gfx_text_box(&font_s, 300, y + 4, DISP_W - 320, ALIGN_RIGHT, v, sel ? C_ACCENT : C_DIM);
    }
    if (!busy && s_naps == 0) gfx_text_box(&font_s, 0, 214, DISP_W, ALIGN_CENTER,
                                          "Keine Netze? Band auf 'Auto' stellen und neu suchen.", C_DIM);
}

static void handle_wifi(const input_ev_t *ev)
{
    bool busy = link_scan_busy();
    const int items = 2 + (busy ? 1 : s_naps) + 1;
    if (ev->type == EV_ENC1_TURN) {
        s_wsel = clampv(s_wsel + ev->value, 0, items - 1);
    } else if (ev->type == EV_ENC1_PRESS) {
        if (s_wsel == 0) { if (!busy) link_scan(); }
        else if (s_wsel == 1) { if (s_c5.have_settings) { bp_sync(); bp_change(BI_BAND, 1); } }
        else if (s_wsel == items - 1) { s_scr = SCR_MENU; redraw(); return; }
        else if (!busy) {
            const link_ap_t *a = &s_aps[s_wsel - 2];
            snprintf(s_wifi_ssid_sel, sizeof(s_wifi_ssid_sel), "%s", a->ssid);
            if (a->secure) {
                char t[48];
                snprintf(t, sizeof(t), "Passwort für %.24s", a->ssid);
                text_open(t, "", 63, CS_FULL, TX_WIFI_PASS, SCR_WIFI);
                return;
            }
            link_wifi(a->ssid, "");
            snprintf(s_wifi_msg, sizeof(s_wifi_msg), "Verbinde mit %.30s ...", a->ssid);
            s_wifi_msg_t = esp_timer_get_time();
        }
    } else if (ev->type == EV_ENC2_PRESS || ev->type == EV_ENC1_LONG) {
        s_scr = SCR_MENU;
        redraw();
        return;
    }
    draw_wifi();
}

// ---------------------------------------------------------------- Intercom-Server
enum { CI_ON, CI_FIND, CI_HOST, CI_PORT, CI_PIN, CI_APPLY, CI_BACK, CI_COUNT };
static bool host_is_auto(const char *h) { return !h[0] || !strcmp(h, "auto"); }

static void comset_load(void)
{
    s_cs.on = s_c5.com_on;
    s_cs.port = s_c5.com_port > 0 ? s_c5.com_port : 3002;
    snprintf(s_cs.host, sizeof(s_cs.host), "%s", s_c5.com_host[0] ? s_c5.com_host : "auto");
    snprintf(s_cs.pin, sizeof(s_cs.pin), "%s", s_c5.com_pin);
    s_cs.dirty = false;
    s_cs.msg[0] = 0;
}

static void comset_apply(void)
{
    link_com(s_cs.on, s_cs.port, s_cs.host, s_cs.pin);
    s_cs.dirty = false;
    snprintf(s_cs.msg, sizeof(s_cs.msg), "übernommen");
}

static int s_csel;
static int s_dsel;

// ---------------------------------------------------------------- Server-Suche
static link_server_t s_srvl[LINK_MAX_SRV];
static int s_nsrvl;
// Zeilen: 0 = Automatisch, 1.. = gefundene Server, dann Neu suchen, Zurück

static void draw_disc(void)
{
    gfx_clear(C_BG);
    gfx_text(&font_l, 16, 6, "Server suchen", C_TEXT);
    bool busy = link_discover_busy();
    if (!busy) s_nsrvl = link_get_servers(s_srvl, LINK_MAX_SRV);
    gfx_text_box(&font_s, 220, 14, DISP_W - 236, ALIGN_RIGHT,
                 busy ? "suche im Netzwerk ..." : !s_c5.wifi ? "erst WLAN verbinden" :
                 s_nsrvl ? "Server wählen" : "kein Server gefunden", busy ? C_WARN : C_DIM);
    gfx_hline(0, 46, DISP_W, C_LINE);
    const int items = 1 + (busy ? 0 : s_nsrvl) + 2;
    s_dsel = clampv(s_dsel, 0, items - 1);
    for (int i = 0; i < items && i < 6; i++) {
        int y = 54 + i * 30;
        bool sel = i == s_dsel;
        if (sel) gfx_round_rect(8, y - 2, DISP_W - 16, 28, 6, C_TILE_HI);
        char l[48] = "", v[48] = "";
        color_t lc = sel ? C_TEXT : C_DIM;
        if (i == 0) {
            snprintf(l, sizeof(l), "Automatisch");
            snprintf(v, sizeof(v), "%s", host_is_auto(s_cs.host) ? "aktiv · empfohlen" : "empfohlen");
        } else if (i == items - 2) {
            snprintf(l, sizeof(l), "%s", busy ? "suche ..." : "Neu suchen");
            lc = sel ? C_ACCENT : C_DIM;
        } else if (i == items - 1) {
            snprintf(l, sizeof(l), "< Zurück");
            lc = sel ? C_ACCENT : C_DIM;
        } else {
            const link_server_t *sv = &s_srvl[i - 1];
            snprintf(l, sizeof(l), "%.22s", sv->project[0] ? sv->project : sv->name);
            snprintf(v, sizeof(v), "%s:%d", sv->ip, sv->port);
            if (!strcmp(sv->ip, s_cs.host)) lc = sel ? C_LISTEN : RGB(30, 150, 70);
        }
        gfx_text(&font_m, 20, y, l, lc);
        if (v[0]) gfx_text_box(&font_s, 260, y + 4, DISP_W - 280, ALIGN_RIGHT, v, sel ? C_ACCENT : C_DIM);
    }
    gfx_text_box(&font_s, 0, 222, DISP_W, ALIGN_CENTER, "Automatisch: das Bodypack sucht bei jedem Start selbst", C_DIM);
}

static void handle_disc(const input_ev_t *ev)
{
    bool busy = link_discover_busy();
    const int items = 1 + (busy ? 0 : s_nsrvl) + 2;
    if (ev->type == EV_ENC1_TURN) {
        s_dsel = clampv(s_dsel + ev->value, 0, items - 1);
    } else if (ev->type == EV_ENC1_PRESS) {
        if (s_dsel == items - 2) { if (!busy) link_discover(); }
        else if (s_dsel == items - 1) { s_scr = SCR_COMSET; redraw(); return; }
        else {
            if (s_dsel == 0) {
                snprintf(s_cs.host, sizeof(s_cs.host), "auto");
            } else {
                snprintf(s_cs.host, sizeof(s_cs.host), "%s", s_srvl[s_dsel - 1].ip);
                s_cs.port = s_srvl[s_dsel - 1].port;
            }
            s_cs.on = 1;
            comset_apply();                                  // gleich verbinden
            s_scr = SCR_COMSET;
            redraw();
            return;
        }
    } else if (ev->type == EV_ENC2_PRESS || ev->type == EV_ENC1_LONG) {
        s_scr = SCR_COMSET;
        redraw();
        return;
    }
    draw_disc();
}

static void draw_comset(void)
{
    gfx_clear(C_BG);
    gfx_text(&font_l, 16, 6, "Intercom-Server", C_TEXT);
    gfx_hline(0, 46, DISP_W, C_LINE);
    // Status
    char st[96];
    color_t sc = s_c5.com_state == COM_ST_ONLINE ? C_LISTEN : C_WARN;
    if (!s_c5.online) snprintf(st, sizeof(st), "Bodypack nicht verbunden");
    else if (s_c5.com_state == COM_ST_ONLINE)
        snprintf(st, sizeof(st), "verbunden als %.20s · %s · %d Kanäle", s_c5.com_user, s_c5.com_ip, s_c5.com_nch);
    else snprintf(st, sizeof(st), "%s%s%.40s", com_state_text(s_c5.com_state), s_c5.com_msg[0] ? ": " : "", s_c5.com_msg);
    gfx_text(&font_s, 16, 52, st, sc);
    for (int i = 0; i < CI_COUNT; i++) {
        int y = 74 + i * 19 + (i >= CI_APPLY ? 6 : 0);
        bool sel = i == s_csel;
        if (sel) gfx_round_rect(8, y - 1, DISP_W - 16, 20, 6, C_TILE_HI);
        static const char *const lbl[CI_COUNT] = { "Intercom", "Server im Netzwerk suchen", "Server (von Hand)", "Port",
                                                   "PIN", "Übernehmen und verbinden", "< Zurück" };
        color_t lc = (i >= CI_APPLY) ? (sel ? C_ACCENT : C_DIM) : (sel ? C_TEXT : C_DIM);
        if (i == CI_APPLY && s_cs.dirty) lc = C_WARN;
        gfx_text(&font_s, 20, y + 1, lbl[i], lc);
        char v[64] = "";
        if (i == CI_ON) snprintf(v, sizeof(v), "%s", s_cs.on ? "ein" : "aus");
        if (i == CI_HOST) snprintf(v, sizeof(v), "%s", host_is_auto(s_cs.host) ? "automatisch" : s_cs.host);
        if (i == CI_FIND) snprintf(v, sizeof(v), "%s", host_is_auto(s_cs.host) ? "automatisch aktiv" : "");
        if (i == CI_PORT) snprintf(v, sizeof(v), "%s", host_is_auto(s_cs.host) ? "automatisch" : "");
        if (i == CI_PORT && !host_is_auto(s_cs.host)) snprintf(v, sizeof(v), "%d", s_cs.port);
        if (i == CI_PIN) snprintf(v, sizeof(v), "%s", s_cs.pin[0] ? s_cs.pin : "(keine)");
        if (i == CI_APPLY && s_cs.msg[0] && !s_cs.dirty) snprintf(v, sizeof(v), "%s", s_cs.msg);
        if (v[0]) gfx_text_box(&font_s, 200, y + 1, DISP_W - 220, ALIGN_RIGHT, v, sel ? C_ACCENT : C_DIM);
    }
}

static void handle_comset(const input_ev_t *ev)
{
    if (ev->type == EV_ENC1_TURN) {
        s_csel = clampv(s_csel + ev->value, 0, CI_COUNT - 1);
    } else if (ev->type == EV_ENC1_PRESS) {
        switch (s_csel) {
        case CI_ON:   s_cs.on = !s_cs.on; s_cs.dirty = true; break;
        case CI_FIND: s_scr = SCR_DISC; s_dsel = 0; link_discover(); redraw(); return;
        case CI_HOST: text_open("Server (Name oder IP)", host_is_auto(s_cs.host) ? "" : s_cs.host, 47, CS_HOST,
                                TX_COM_HOST, SCR_COMSET); return;
        case CI_PORT: { char p[8]; snprintf(p, sizeof(p), "%d", s_cs.port);
                        text_open("Port (Standard 3002)", p, 5, CS_DIGITS, TX_COM_PORT, SCR_COMSET); return; }
        case CI_PIN:  text_open("PIN des Bodypack-Benutzers", s_cs.pin, 15, CS_FULL, TX_COM_PIN, SCR_COMSET); return;
        case CI_APPLY: comset_apply(); break;
        case CI_BACK:
            if (s_cs.dirty) comset_apply();                // Aenderungen nicht verlieren
            s_scr = SCR_MENU; redraw(); return;
        }
    } else if (ev->type == EV_ENC2_PRESS || ev->type == EV_ENC1_LONG) {
        if (s_cs.dirty) comset_apply();
        s_scr = SCR_MENU;
        redraw();
        return;
    }
    draw_comset();
}

// Update laeuft: ganzer Bildschirm zeigt den Fortschritt
static void draw_ota(void)
{
    int p = ota_progress();
    gfx_clear(C_BG);
    gfx_text_box(&font_l, 0, 40, DISP_W, ALIGN_CENTER, p == -2 ? "Update fehlgeschlagen" : "Update läuft ...",
                 p == -2 ? C_TALK : C_TEXT);
    if (p >= 0) {
        gfx_round_rect(40, 110, DISP_W - 80, 30, 8, C_TILE);
        gfx_round_rect(40, 110, (DISP_W - 80) * p / 100 < 16 ? 16 : (DISP_W - 80) * p / 100, 30, 8, C_ACCENT);
        char b[16];
        snprintf(b, sizeof(b), "%d %%", p);
        gfx_text_box(&font_m, 0, 150, DISP_W, ALIGN_CENTER, b, C_TEXT);
        gfx_text_box(&font_s, 0, 200, DISP_W, ALIGN_CENTER, "Nicht ausschalten - das Display startet danach neu", C_DIM);
    } else {
        gfx_text_box(&font_s, 0, 110, DISP_W, ALIGN_CENTER, ota_message(), C_WARN);
        gfx_text_box(&font_s, 0, 200, DISP_W, ALIGN_CENTER, "Beliebiger Taster: weiter", C_DIM);
    }
}

static void redraw(void)
{
    switch (s_scr) {
    case SCR_MAIN: if (com_on_line()) draw_channels(); else draw_main(); break;
    case SCR_DASH: draw_main(); break;
    case SCR_WIFI: draw_wifi(); break;
    case SCR_TEXT: draw_text_entry(); break;
    case SCR_COMSET: draw_comset(); break;
    case SCR_DISC: draw_disc(); break;
    case SCR_MENU: draw_menu(); break;
    case SCR_INFO: draw_info(); break;
    case SCR_TEST: draw_test(); break;
    case SCR_FPS:  draw_fps(0); break;
    case SCR_BP:   draw_bp(); break;
    case SCR_BPSTAT: draw_bpstat(); break;
    }
}

// ---------------------------------------------------------------- Eingaben
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void handle(const input_ev_t *ev)
{
    switch (s_scr) {
    case SCR_WIFI:   handle_wifi(ev); break;
    case SCR_TEXT:   handle_text(ev); break;
    case SCR_COMSET: handle_comset(ev); break;
    case SCR_DISC:   handle_disc(ev); break;
    case SCR_MAIN:
        if (com_on_line()) { handle_channels(ev); break; }
        /* fallthrough */
    case SCR_DASH:
        if (!s_c5.have_settings && (ev->type == EV_ENC1_TURN || ev->type == EV_ENC2_TURN ||
                                    ev->type == EV_ENC1_LONG || ev->type == EV_ENC2_LONG)) {
            break;                                          // ohne Bodypack nichts zu verstellen
        }
        if (ev->type == EV_ENC1_TURN) {
            bp_sync();
            bp_change(BI_VOL, ev->value);
        } else if (ev->type == EV_ENC2_TURN) {
            bp_sync();
            bp_change(BI_PGA, ev->value);
        } else if (ev->type == EV_ENC1_LONG) {
            link_talk(!s_c5.talk);
            s_c5.talk = !s_c5.talk;                          // sofort anzeigen, C5 bestaetigt gleich
        } else if (ev->type == EV_ENC2_LONG) {
            bp_sync();
            bp_change(BI_MODE, 1);
        } else if (ev->type == EV_ENC1_PRESS || ev->type == EV_ENC2_PRESS) {
            s_scr = SCR_MENU;
            s_menu_sel = 0;
            s_menu_top = 0;
            redraw();
            break;
        }
        draw_main();
        break;

    case SCR_MENU:
        if (ev->type == EV_ENC1_TURN) {
            s_menu_sel = clampi(s_menu_sel + ev->value, 0, MI_COUNT - 1);
            draw_menu();
        } else if (ev->type == EV_ENC2_TURN && s_menu_sel == MI_BRIGHT) {
            s_brightness = clampi(s_brightness + ev->value * 13, 10, 255);
            display_set_brightness((uint8_t)s_brightness);
            draw_menu();
        } else if (ev->type == EV_ENC1_PRESS) {
            switch (s_menu_sel) {
            case MI_FLIP:
                s_flip = !s_flip;
                display_set_flip(s_flip);
                draw_menu();
                gfx_invalidate();
                break;
            case MI_WIFI:   s_scr = SCR_WIFI; s_wsel = 0; s_wtop = 0; link_scan(); redraw(); break;
            case MI_COMSET: s_scr = SCR_COMSET; s_csel = 0; comset_load(); redraw(); break;
            case MI_DASH:   s_scr = SCR_DASH; redraw(); break;
            case MI_BP:     s_scr = SCR_BP; bp_sync(); redraw(); break;
            case MI_BPSTAT: s_scr = SCR_BPSTAT; redraw(); break;
            case MI_TEST: s_scr = SCR_TEST; redraw(); break;
            case MI_FPS:  s_scr = SCR_FPS;  redraw(); break;
            case MI_INFO: s_scr = SCR_INFO; redraw(); break;
            case MI_BACK: settings_save(); s_scr = SCR_MAIN; redraw(); break;
            default: break;
            }
        } else if (ev->type == EV_ENC2_PRESS || ev->type == EV_ENC1_LONG) {
            settings_save();
            s_scr = SCR_MAIN;
            redraw();
        }
        break;

    case SCR_BP:
        if (ev->type == EV_ENC1_TURN) {
            s_bp_sel = clampi(s_bp_sel + ev->value, 0, BP_ITEMS - 1);
            draw_bp();
        } else if (ev->type == EV_ENC1_PRESS && s_bp_sel == BI_COUNT) {
            s_scr = SCR_MENU;                            // "Zurück" gewaehlt
            redraw();
        } else if (ev->type == EV_ENC2_TURN && s_c5.have_settings && s_bp_sel < BI_COUNT) {
            bp_change(s_bp_sel, ev->value);
            draw_bp();
        } else if (ev->type == EV_ENC1_PRESS && s_c5.have_settings && s_bp[s_bp_sel].names) {
            bp_change(s_bp_sel, 1);                      // Auswahl: naechster Wert
            draw_bp();
        } else if (ev->type == EV_ENC2_PRESS || ev->type == EV_ENC1_LONG) {
            s_scr = SCR_MENU;
            redraw();
        }
        break;

    case SCR_BPSTAT:
    case SCR_INFO:
    case SCR_TEST:
    case SCR_FPS:
        if (ev->type == EV_ENC1_PRESS || ev->type == EV_ENC2_PRESS) {
            s_scr = SCR_MENU;
            redraw();
        }
        break;
    }
}

// ---------------------------------------------------------------- Ablauf
static void ui_task(void *arg)
{
    int64_t last_bat = -10000000, last_info = 0, fps_t0 = esp_timer_get_time();
    uint32_t frame = 0, fps_frames = 0;
    redraw();
    gfx_flush();
    display_set_brightness((uint8_t)s_brightness);

    while (1) {
        input_ev_t ev;
        TickType_t wait = (s_scr == SCR_FPS) ? 0 : pdMS_TO_TICKS(20);
        while (xQueueReceive(g_input_q, &ev, wait) == pdTRUE) {
            wait = 0;
            if (ota_progress() == -2) {                   // Fehlermeldung des Updates: Taster = weiter
                if (ev.type == EV_ENC1_PRESS || ev.type == EV_ENC2_PRESS) { ota_clear_error(); redraw(); }
                continue;
            }
            if (ota_progress() >= 0) continue;            // waehrend des Updates keine Bedienung
            handle(&ev);
        }
        int64_t now = esp_timer_get_time();

        static int64_t last_link;
        if (ota_progress() != -1) {                       // Update laeuft (oder ist fehlgeschlagen)
            if (now - last_link > 200000) { last_link = now; draw_ota(); gfx_flush(); }
            continue;
        }
        if (now - last_link > 100000) {                   // Bodypack-Werte mit 10 Hz
            last_link = now;
            link_get(&s_c5);
            ota_check_new_version(s_c5.online);
            if (s_scr == SCR_BP) {
                bp_sync();
                draw_bp();
            } else if (s_scr == SCR_BPSTAT && (now / 100000) % 5 == 0) {
                draw_bpstat();
            } else if (s_scr == SCR_MAIN || s_scr == SCR_DASH) {
                redraw();                                  // Kanalliste bzw. Uebersicht mit 10 Hz
            } else if ((s_scr == SCR_WIFI || s_scr == SCR_COMSET || s_scr == SCR_DISC) && (now / 100000) % 5 == 0) {
                redraw();                                  // Suche/Status mit 2 Hz
            }
        }

        if (now - last_bat > 2000000) {          // Akku alle 2 s
            last_bat = now;
            s_bat_mv = battery_mv();
            s_bat_pct = battery_percent();
            if (s_scr == SCR_MAIN || s_scr == SCR_DASH) draw_battery();
        }
        if (s_scr == SCR_INFO && now - last_info > 500000) {
            last_info = now;
            draw_info();
        }
        if (s_scr == SCR_FPS) {
            draw_fps(frame++);
            fps_frames++;
            if (now - fps_t0 > 1000000) {
                s_fps = fps_frames * 1e6f / (float)(now - fps_t0);
                fps_frames = 0;
                fps_t0 = now;
            }
        }
        gfx_flush();
    }
}

esp_err_t ui_start(void)
{
    settings_load();
    display_set_flip(s_flip);
    xTaskCreate(ui_task, "ui", 6144, NULL, 5, NULL);
    return ESP_OK;
}

// ---------------------------------------------------------------- Konsole: status
void ui_print_status(void)
{
    static const char *const scr_names[] = { "Hauptseite", "Menü", "Info", "Testbild", "FPS-Test", "Bodypack-Einstellungen",
                                            "Bodypack-Status", "Audio-Übersicht", "WLAN", "Texteingabe",
                                            "Intercom-Server", "Server-Suche" };
    c5_state_t c;
    link_get(&c);
    const esp_app_desc_t *ad = esp_app_get_description();
    int e1, e2;
    input_raw_counts(&e1, &e2);
    printf("\n=== NR Display (ESP32-S3) === Firmware %s, gebaut %s %s, Laufzeit %llu s\n", ad->version, ad->date, ad->time,
           (unsigned long long)(esp_timer_get_time() / 1000000));
    printf("Anzeige: Seite '%s', Helligkeit %d %%, %s, Akku %d mV (%d %%)\n",
           (int)s_scr < (int)(sizeof(scr_names) / sizeof(scr_names[0])) ? scr_names[s_scr] : "?",
           s_brightness * 100 / 255, s_flip ? "gedreht" : "normal", battery_mv(), battery_percent());
    printf("Drehgeber: Rohzaehler 1 = %d, 2 = %d\n", e1, e2);
    printf("C5-Verbindung (UART): %s, %lu Zeilen empfangen\n", c.online ? "OK" : "KEINE (GPIO4->GP3, GPIO10<-GP2, GND?)",
           (unsigned long)c.lines);
    if (c.online) {
        printf("C5: WLAN %s%s (%d dBm), Codec %s, Talk %s, Mic %.1f dBFS, Ausgang %.1f dBFS, CPU-Last siehe 'c5 status'\n",
               c.wifi ? "verbunden mit " : "nicht verbunden", c.wifi ? c.ssid : "", c.rssi, c.codec_ok ? "ok" : "FEHLT",
               c.talk ? "AN" : "aus", c.mic == 1 ? c.jack_db : c.board_db, c.out_db);
        printf("Intercom: %s%s%s, Server %s, Benutzer '%s', %d Kanaele\n", com_state_text(c.com_state),
               c.com_msg[0] ? " - " : "", c.com_msg, c.com_ip[0] ? c.com_ip : "-", c.com_user, c.com_nch);
        link_channel_t ch[LINK_MAX_CH];
        int n = link_get_channels(ch, LINK_MAX_CH, NULL);
        for (int i = 0; i < n; i++) {
            printf("   %2d %-24s %s %s Lautst. %3d%s\n", i, ch[i].name, ch[i].joined ? "HOERT" : "aus  ",
                   ch[i].talk ? "TALK" : "    ", ch[i].volume, ch[i].talkers ? "  (jemand spricht)" : "");
        }
    }
    printf("RAM: intern frei %u KB, PSRAM frei %u KB\n\n",
           (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
           (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
}
