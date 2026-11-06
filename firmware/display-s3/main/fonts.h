#pragma once
#include <stdint.h>

typedef struct {
    uint16_t cp;        // Unicode-Codepunkt
    uint32_t offset;    // Start im Bitmap-Array
    uint8_t  w, h;      // Groesse des Bitmaps
    int8_t   x0, y0;    // Versatz zur Zeichenposition (oben links der Zeile)
    uint8_t  adv;       // Vorschub
} glyph_t;

typedef struct {
    const glyph_t *glyphs;
    uint16_t       count;
    const uint8_t *bitmap;   // 4 Bit pro Pixel, zeilenweise, 2 Pixel pro Byte
    uint8_t        line_h;
    uint8_t        ascent;
} font_t;

extern const font_t font_s, font_m, font_l, font_xl;
