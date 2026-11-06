#include <stdio.h>
#include <string.h>
#include <math.h>
#include "ui.h"
#include "gfx.h"
#include "input.h"
#include "battery.h"
#include "display.h"
#include "nvs.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_chip_info.h"
#include "esp_idf_version.h"
#include "esp_random.h"
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
typedef struct {
    const char *name;
    bool listen;
    bool talk;
    int  volume;      // 0..100
    float level;      // Demo-Pegel 0..1
} channel_t;

static channel_t s_ch[4] = {
    { "Regie", true, false, 80, 0 },
    { "Ton",   true, false, 70, 0 },
    { "Licht", false, false, 60, 0 },
    { "Bühne", false, false, 60, 0 },
};

typedef enum { SCR_MAIN, SCR_MENU, SCR_INFO, SCR_TEST, SCR_FPS } screen_t;
static screen_t s_scr = SCR_MAIN;
static int s_sel;              // gewaehlter Kanal
static int s_menu_sel;
static int s_brightness = 180;
static bool s_flip;
static int s_bat_mv = -1, s_bat_pct = -1;
static float s_fps;

enum { MI_BRIGHT, MI_FLIP, MI_TEST, MI_FPS, MI_INFO, MI_BACK, MI_COUNT };
static const char *s_menu_names[MI_COUNT] = {
    "Helligkeit", "Display drehen", "Testbild", "FPS-Test", "Info", "Zurück",
};

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
#define TILE_Y   38
#define TILE_H   160
#define TILE_W   124
#define TILE_GAP 8
#define TILE_X(i) (TILE_GAP + (i) * (TILE_W + TILE_GAP))
#define VU_DY    112

static void draw_pill(int x, int y, int w, const char *txt, color_t fg, color_t bg)
{
    gfx_round_rect(x, y, w, 26, 13, bg);
    gfx_text_box(&font_s, x, y + 4, w, ALIGN_CENTER, txt, fg);
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
    // Akkusymbol
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
    gfx_text_box(&font_s, 140, 7, 240, ALIGN_CENTER, "Funk: nicht verbunden", C_DIM);
    draw_battery();
    gfx_hline(0, 32, DISP_W, C_LINE);
}

static void draw_vu(int i)
{
    const channel_t *c = &s_ch[i];
    int x = TILE_X(i) + 12, y = TILE_Y + VU_DY, w = TILE_W - 24;
    gfx_fill_rect(x, y, w, 6, RGB(15, 15, 18));
    int lw = c->listen ? (int)(w * c->level * c->volume / 100.0f) : 0;
    if (lw > 0) gfx_fill_rect(x, y, lw, 6, lw > w * 0.85f ? C_WARN : C_LISTEN);
}

static void draw_tile(int i)
{
    const channel_t *c = &s_ch[i];
    const int x = TILE_X(i), y = TILE_Y;
    color_t bg = c->talk ? C_TALK_BG : (i == s_sel ? C_TILE_HI : C_TILE);
    gfx_fill_rect(x, y, TILE_W, TILE_H, C_BG);
    if (i == s_sel) {                                   // Auswahl: heller Rand
        gfx_round_rect(x, y, TILE_W, TILE_H, 10, C_TEXT);
        gfx_round_rect(x + 3, y + 3, TILE_W - 6, TILE_H - 6, 8, bg);
    } else {
        gfx_round_rect(x, y, TILE_W, TILE_H, 10, bg);
    }
    char num[12];
    snprintf(num, sizeof(num), "%d", i + 1);
    gfx_text(&font_s, x + 10, y + 6, num, C_DIM);
    gfx_text_box(&font_m, x, y + 24, TILE_W, ALIGN_CENTER, c->name, C_TEXT);
    if (c->talk) {
        draw_pill(x + 12, y + 60, TILE_W - 24, "TALK", C_TEXT, C_TALK);
    } else if (c->listen) {
        draw_pill(x + 12, y + 60, TILE_W - 24, "HÖREN", C_BG, C_LISTEN);
    } else {
        draw_pill(x + 12, y + 60, TILE_W - 24, "AUS", C_DIM, RGB(50, 50, 58));
    }
    draw_vu(i);
    // Lautstaerke
    int bx = x + 12, by = y + 128, bw = TILE_W - 24;
    gfx_fill_rect(bx, by, bw, 8, RGB(15, 15, 18));
    gfx_fill_rect(bx, by, bw * c->volume / 100, 8, C_VOL);
    char vol[12];
    snprintf(vol, sizeof(vol), "%d %%", c->volume);
    gfx_text_box(&font_s, x, y + 138, TILE_W, ALIGN_CENTER, vol, C_DIM);
}

static void draw_footer(void)
{
    gfx_fill_rect(0, 204, DISP_W, DISP_H - 204, C_BG);
    gfx_hline(0, 204, DISP_W, C_LINE);
    gfx_text(&font_s, 10, 214, "Links: Kanal · kurz Hören · lang Talk", C_DIM);
    gfx_text_box(&font_s, 300, 214, DISP_W - 310, ALIGN_RIGHT, "Rechts: Lautstärke · Menü", C_DIM);
}

static void draw_main(void)
{
    gfx_clear(C_BG);
    draw_header();
    for (int i = 0; i < 4; i++) draw_tile(i);
    draw_footer();
}

static void draw_menu(void)
{
    gfx_clear(C_BG);
    gfx_text(&font_l, 16, 6, "Menü", C_TEXT);
    gfx_text_box(&font_s, 200, 14, DISP_W - 216, ALIGN_RIGHT, "Links: wählen/OK · Rechts: Wert · drücken: zurück", C_DIM);
    gfx_hline(0, 46, DISP_W, C_LINE);
    for (int i = 0; i < MI_COUNT; i++) {
        int y = 54 + i * 30;
        bool sel = i == s_menu_sel;
        if (sel) gfx_round_rect(8, y - 2, DISP_W - 16, 28, 6, C_TILE_HI);
        gfx_text(&font_m, 20, y, s_menu_names[i], sel ? C_TEXT : C_DIM);
        char val[24] = "";
        if (i == MI_BRIGHT) snprintf(val, sizeof(val), "%d %%", s_brightness * 100 / 255);
        if (i == MI_FLIP) snprintf(val, sizeof(val), "%s", s_flip ? "gedreht" : "normal");
        if (val[0]) gfx_text_box(&font_m, 300, y, DISP_W - 330, ALIGN_RIGHT, val, sel ? C_ACCENT : C_DIM);
    }
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
    snprintf(l[6], 80, "Funk-Modul (ESP32-C5): noch nicht verbunden");
    snprintf(l[7], 80, "Beliebiger Taster: zurück");
    for (int i = 0; i < 8; i++) gfx_text(&font_s, 16, 56 + i * 22, l[i], i == 7 ? C_DIM : C_TEXT);
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

static void redraw(void)
{
    switch (s_scr) {
    case SCR_MAIN: draw_main(); break;
    case SCR_MENU: draw_menu(); break;
    case SCR_INFO: draw_info(); break;
    case SCR_TEST: draw_test(); break;
    case SCR_FPS:  draw_fps(0); break;
    }
}

// ---------------------------------------------------------------- Eingaben
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void handle(const input_ev_t *ev)
{
    switch (s_scr) {
    case SCR_MAIN:
        if (ev->type == EV_ENC1_TURN) {
            int old = s_sel;
            s_sel = clampi(s_sel + ev->value, 0, 3);
            if (old != s_sel) { draw_tile(old); draw_tile(s_sel); }
        } else if (ev->type == EV_ENC1_PRESS) {
            s_ch[s_sel].listen = !s_ch[s_sel].listen;
            draw_tile(s_sel);
        } else if (ev->type == EV_ENC1_LONG) {
            s_ch[s_sel].talk = !s_ch[s_sel].talk;
            draw_tile(s_sel);
        } else if (ev->type == EV_ENC2_TURN) {
            s_ch[s_sel].volume = clampi(s_ch[s_sel].volume + ev->value * 5, 0, 100);
            draw_tile(s_sel);
        } else if (ev->type == EV_ENC2_PRESS) {
            s_scr = SCR_MENU;
            s_menu_sel = 0;
            redraw();
        }
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
            case MI_TEST: s_scr = SCR_TEST; redraw(); break;
            case MI_FPS:  s_scr = SCR_FPS;  redraw(); break;
            case MI_INFO: s_scr = SCR_INFO; redraw(); break;
            case MI_BACK: settings_save(); s_scr = SCR_MAIN; redraw(); break;
            default: break;
            }
        } else if (ev->type == EV_ENC2_PRESS) {
            settings_save();
            s_scr = SCR_MAIN;
            redraw();
        }
        break;

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
    int64_t last_bat = -10000000, last_info = 0, last_vu = 0, fps_t0 = esp_timer_get_time();
    uint32_t frame = 0, fps_frames = 0;
    redraw();
    gfx_flush();
    display_set_brightness((uint8_t)s_brightness);

    while (1) {
        input_ev_t ev;
        TickType_t wait = (s_scr == SCR_FPS) ? 0 : pdMS_TO_TICKS(20);
        while (xQueueReceive(g_input_q, &ev, wait) == pdTRUE) {
            handle(&ev);
            wait = 0;
        }
        int64_t now = esp_timer_get_time();

        if (now - last_bat > 2000000) {          // Akku alle 2 s
            last_bat = now;
            s_bat_mv = battery_mv();
            s_bat_pct = battery_percent();
            if (s_scr == SCR_MAIN) draw_battery();
        }
        if (s_scr == SCR_MAIN && now - last_vu > 40000) {   // Demo-Pegel mit 25 Hz
            last_vu = now;
            for (int i = 0; i < 4; i++) {
                float target = s_ch[i].listen ? (esp_random() % 1000) / 1000.0f : 0;
                s_ch[i].level = s_ch[i].level * 0.6f + target * 0.4f;
                draw_vu(i);
            }
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
