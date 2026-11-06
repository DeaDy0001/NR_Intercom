#!/usr/bin/env python3
"""Erzeugt geglaettete 4-Bit-Schriften (C-Arrays) fuer die Display-Firmware.

Aufruf: python tools/gen_font.py firmware/display-s3/main/fonts.c
Benoetigt Pillow und die Schrift DejaVu Sans (Bitstream-Vera-Lizenz, frei nutzbar).
"""
import sys
from PIL import Image, ImageDraw, ImageFont

FONT_DIR = "/usr/share/fonts/truetype/dejavu/"
FONTS = [  # (C-Name, Datei, Pixelgroesse)
    ("font_s",  "DejaVuSans.ttf", 16),
    ("font_m",  "DejaVuSans-Bold.ttf", 22),
    ("font_l",  "DejaVuSans-Bold.ttf", 30),
    ("font_xl", "DejaVuSans-Bold.ttf", 44),
]
CHARS = [chr(c) for c in range(32, 127)] + list("ÄÖÜäöüß°µ±·…")


def render(name, path, size):
    font = ImageFont.truetype(FONT_DIR + path, size)
    ascent, descent = font.getmetrics()
    line_h = ascent + descent
    glyphs, data = [], bytearray()
    for ch in CHARS:
        adv = int(round(font.getlength(ch)))
        bbox = font.getbbox(ch)  # (x0, y0, x1, y1) relativ zur Grundlinien-Oberkante
        if bbox is None or bbox[2] <= bbox[0] or bbox[3] <= bbox[1]:
            glyphs.append((ord(ch), len(data), 0, 0, 0, 0, adv))
            continue
        x0, y0, x1, y1 = bbox
        w, h = x1 - x0, y1 - y0
        img = Image.new("L", (w, h), 0)
        ImageDraw.Draw(img).text((-x0, -y0), ch, font=font, fill=255)
        px = img.load()
        off = len(data)
        row_bytes = (w + 1) // 2
        for y in range(h):
            for xb in range(row_bytes):
                a = px[2 * xb, y] >> 4
                b = (px[2 * xb + 1, y] >> 4) if 2 * xb + 1 < w else 0
                data.append((a << 4) | b)
        glyphs.append((ord(ch), off, w, h, x0, y0, adv))
    out = [f"static const uint8_t {name}_bitmap[] = {{"]
    for i in range(0, len(data), 24):
        out.append("    " + ",".join(f"0x{b:02X}" for b in data[i:i + 24]) + ",")
    out.append("};")
    out.append(f"static const glyph_t {name}_glyphs[] = {{")
    for cp, off, w, h, x0, y0, adv in glyphs:
        out.append(f"    {{0x{cp:04X}, {off}, {w}, {h}, {x0}, {y0}, {adv}}},")
    out.append("};")
    out.append(f"const font_t {name} = {{ {name}_glyphs, {len(glyphs)}, {name}_bitmap, {line_h}, {ascent} }};")
    return "\n".join(out), len(data)


def main():
    dst = sys.argv[1] if len(sys.argv) > 1 else "fonts.c"
    parts = ["// Automatisch erzeugt mit tools/gen_font.py - nicht von Hand aendern.",
             "// Schrift: DejaVu Sans (Bitstream Vera License), 4 Bit Graustufen.",
             '#include "fonts.h"', ""]
    for name, path, size in FONTS:
        code, n = render(name, path, size)
        parts.append(code)
        parts.append("")
        print(f"{name}: {path} {size}px, {n} Byte")
    open(dst, "w", encoding="utf-8").write("\n".join(parts))


if __name__ == "__main__":
    main()
