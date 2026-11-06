#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "fonts.h"
#include "display.h"

// Einfache Grafik auf einem Bildspeicher im PSRAM (RGB565, fuer das Display byte-getauscht).
// Gezeichnet wird in den Speicher, gfx_flush() schickt nur den geaenderten Bereich.

typedef uint16_t color_t;

static inline color_t RGB(uint8_t r, uint8_t g, uint8_t b)
{
    uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    return (color_t)((c >> 8) | (c << 8));
}

typedef enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT } align_t;

esp_err_t gfx_init(void);
void gfx_clear(color_t c);
void gfx_fill_rect(int x, int y, int w, int h, color_t c);
void gfx_rect(int x, int y, int w, int h, int t, color_t c);
void gfx_round_rect(int x, int y, int w, int h, int r, color_t c);
void gfx_hline(int x, int y, int w, color_t c);
int  gfx_text(const font_t *f, int x, int y, const char *utf8, color_t c);
int  gfx_text_width(const font_t *f, const char *utf8);
void gfx_text_box(const font_t *f, int x, int y, int w, align_t a, const char *utf8, color_t c);
void gfx_flush(void);
void gfx_invalidate(void);
uint16_t *gfx_framebuffer(void);
