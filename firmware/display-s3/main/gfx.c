#include <string.h>
#include "gfx.h"
#include "esp_heap_caps.h"

static uint16_t *s_fb;
static int s_dx0, s_dy0, s_dx1, s_dy1;   // geaenderter Bereich
static bool s_dirty;

static inline void mark(int x0, int y0, int x1, int y1)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= DISP_W) x1 = DISP_W - 1;
    if (y1 >= DISP_H) y1 = DISP_H - 1;
    if (x0 > x1 || y0 > y1) return;
    if (!s_dirty) {
        s_dx0 = x0; s_dy0 = y0; s_dx1 = x1; s_dy1 = y1;
        s_dirty = true;
    } else {
        if (x0 < s_dx0) s_dx0 = x0;
        if (y0 < s_dy0) s_dy0 = y0;
        if (x1 > s_dx1) s_dx1 = x1;
        if (y1 > s_dy1) s_dy1 = y1;
    }
}

esp_err_t gfx_init(void)
{
    s_fb = heap_caps_malloc(DISP_W * DISP_H * 2, MALLOC_CAP_SPIRAM);
    if (!s_fb) return ESP_ERR_NO_MEM;
    gfx_clear(0);
    return ESP_OK;
}

uint16_t *gfx_framebuffer(void)
{
    return s_fb;
}

void gfx_invalidate(void)
{
    mark(0, 0, DISP_W - 1, DISP_H - 1);
}

void gfx_clear(color_t c)
{
    for (int i = 0; i < DISP_W * DISP_H; i++) s_fb[i] = c;
    gfx_invalidate();
}

void gfx_fill_rect(int x, int y, int w, int h, color_t c)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > DISP_W) w = DISP_W - x;
    if (y + h > DISP_H) h = DISP_H - y;
    if (w <= 0 || h <= 0) return;
    for (int r = 0; r < h; r++) {
        uint16_t *p = s_fb + (y + r) * DISP_W + x;
        for (int i = 0; i < w; i++) p[i] = c;
    }
    mark(x, y, x + w - 1, y + h - 1);
}

void gfx_hline(int x, int y, int w, color_t c)
{
    gfx_fill_rect(x, y, w, 1, c);
}

void gfx_rect(int x, int y, int w, int h, int t, color_t c)
{
    gfx_fill_rect(x, y, w, t, c);
    gfx_fill_rect(x, y + h - t, w, t, c);
    gfx_fill_rect(x, y, t, h, c);
    gfx_fill_rect(x + w - t, y, t, h, c);
}

void gfx_round_rect(int x, int y, int w, int h, int r, color_t c)
{
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    for (int row = 0; row < h; row++) {
        int inset = 0;
        int d = -1;
        if (row < r) d = r - row;
        else if (row >= h - r) d = row - (h - r - 1);
        if (d > 0) {
            // Kreisgleichung: inset = r - sqrt(r^2 - (d - 0.5)^2), ganzzahlig angenaehert
            int dy = d;
            int xx = r;
            while (xx > 0 && xx * xx + (dy - 1) * (dy - 1) > r * r) xx--;
            inset = r - xx;
        }
        gfx_fill_rect(x + inset, y + row, w - 2 * inset, 1, c);
    }
}

// --------------------------------------------------------------- Text
static uint32_t utf8_next(const char **s)
{
    const uint8_t *p = (const uint8_t *)*s;
    uint32_t cp;
    if (p[0] < 0x80) { cp = p[0]; *s += 1; }
    else if ((p[0] & 0xE0) == 0xC0 && p[1]) { cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F); *s += 2; }
    else if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2]) {
        cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); *s += 3;
    } else { cp = '?'; *s += 1; }
    return cp;
}

static const glyph_t *find_glyph(const font_t *f, uint32_t cp)
{
    if (cp >= 32 && cp < 127) return &f->glyphs[cp - 32];   // ASCII liegt am Anfang
    for (int i = 95; i < f->count; i++) {
        if (f->glyphs[i].cp == cp) return &f->glyphs[i];
    }
    return &f->glyphs['?' - 32];
}

static inline uint16_t unswap(uint16_t c) { return (uint16_t)((c >> 8) | (c << 8)); }

static inline uint16_t blend(uint16_t bg_sw, uint16_t fg_sw, int a /*0..15*/)
{
    if (a == 0) return bg_sw;
    if (a == 15) return fg_sw;
    uint16_t bg = unswap(bg_sw), fg = unswap(fg_sw);
    int r = (((fg >> 11) & 0x1F) * a + ((bg >> 11) & 0x1F) * (15 - a)) / 15;
    int g = (((fg >> 5) & 0x3F) * a + ((bg >> 5) & 0x3F) * (15 - a)) / 15;
    int b = ((fg & 0x1F) * a + (bg & 0x1F) * (15 - a)) / 15;
    return unswap((uint16_t)((r << 11) | (g << 5) | b));
}

int gfx_text_width(const font_t *f, const char *s)
{
    int w = 0;
    while (*s) w += find_glyph(f, utf8_next(&s))->adv;
    return w;
}

int gfx_text(const font_t *f, int x, int y, const char *s, color_t c)
{
    int x_start = x;
    while (*s) {
        const glyph_t *g = find_glyph(f, utf8_next(&s));
        const uint8_t *bm = f->bitmap + g->offset;
        const int row_bytes = (g->w + 1) / 2;
        for (int r = 0; r < g->h; r++) {
            int py = y + g->y0 + r;
            if (py < 0 || py >= DISP_H) continue;
            for (int col = 0; col < g->w; col++) {
                int px = x + g->x0 + col;
                if (px < 0 || px >= DISP_W) continue;
                uint8_t byte = bm[r * row_bytes + col / 2];
                int a = (col & 1) ? (byte & 0x0F) : (byte >> 4);
                if (a) {
                    uint16_t *p = &s_fb[py * DISP_W + px];
                    *p = blend(*p, c, a);
                }
            }
        }
        x += g->adv;
    }
    mark(x_start, y, x, y + f->line_h);
    return x - x_start;
}

void gfx_text_box(const font_t *f, int x, int y, int w, align_t a, const char *s, color_t c)
{
    int tw = gfx_text_width(f, s);
    int ox = x;
    if (a == ALIGN_CENTER) ox = x + (w - tw) / 2;
    else if (a == ALIGN_RIGHT) ox = x + w - tw;
    gfx_text(f, ox, y, s, c);
}

void gfx_flush(void)
{
    if (!s_dirty) return;
    s_dirty = false;
    display_push(s_fb, s_dx0, s_dy0, s_dx1, s_dy1);
}
