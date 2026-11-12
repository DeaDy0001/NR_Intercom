#!/usr/bin/env python3
"""Erzeugt die Gesamt-Schaltplaene des NR Bodypacks als SVG.

  docs/img/schaltplan-testaufbau.svg   Variante A: Testaufbau mit Modulen (WM8960-Board, C5-Zero, S3, Drehgeber, Tasten)
  docs/img/schaltplan-platine-v1.svg   Variante B: eigene Traegerplatine (Entwurf V0.1)

Aufruf: python tools/schematics.py docs/img
Nur Python-Standardbibliothek.
"""
import os
import sys
from xml.sax.saxutils import escape

# ---------------------------------------------------------------- Farben / Stil
INK = "#1d1d1f"
BOX = "#9b1c1c"
BOXFILL = "#fffdf3"
ICFILL = "#fff8dc"
GRID = "#eeeee6"
C = {"pwr": "#d11a1a", "gnd": "#1d1d1f", "i2c": "#7a3fc8", "i2s": "#0a6fd6", "i2sd": "#e07400",
     "btn": "#0f8a46", "enc": "#0f8a46", "enc2": "#0b9aa6", "uart": "#c2185b", "usb": "#5b6b7a",
     "ana": "#8a5a00", "p48": "#b00068", "dim": "#8a8a8a", "led": "#2e7d32"}
MONO = "DejaVu Sans Mono, Consolas, Menlo, monospace"
SANS = "DejaVu Sans, Segoe UI, Helvetica, Arial, sans-serif"


class Svg:
    def __init__(self, w, h, title, subtitle):
        self.w, self.h = w, h
        self.el = []
        self.pins = {}
        self.el.append(f'<rect x="0" y="0" width="{w}" height="{h}" fill="#ffffff"/>')
        g = []
        for x in range(0, w, 20):
            g.append(f"M{x} 0V{h}")
        for y in range(0, h, 20):
            g.append(f"M0 {y}H{w}")
        self.el.append(f'<path d="{" ".join(g)}" stroke="{GRID}" stroke-width="1"/>')
        self.text(30, 38, title, 30, INK, weight="bold", family=SANS, base="auto")
        self.text(30, 66, subtitle, 16, "#555", family=SANS, base="auto")

    # ------------------------------------------------------------ Grundformen
    def line(self, pts, color=INK, w=3, dash=None):
        d = "M" + " L".join(f"{x:.1f} {y:.1f}" for x, y in pts)
        da = f' stroke-dasharray="{dash}"' if dash else ""
        self.el.append(f'<path d="{d}" fill="none" stroke="{color}" stroke-width="{w}" '
                       f'stroke-linejoin="round" stroke-linecap="round"{da}/>')

    def rect(self, x, y, w, h, stroke=BOX, fill="none", sw=3, dash=None, rx=0):
        da = f' stroke-dasharray="{dash}"' if dash else ""
        self.el.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}" '
                       f'stroke="{stroke}" stroke-width="{sw}"{da}/>')

    def text(self, x, y, s, size=15, color=INK, anchor="start", weight="normal", family=MONO, base="middle",
             bg=False):
        if bg:
            wpx = len(s) * size * 0.62 + 8
            ox = {"start": -4, "middle": -wpx / 2, "end": -wpx + 4}[anchor]
            oy = -size * 0.62 if base == "middle" else -size
            self.el.append(f'<rect x="{x + ox:.1f}" y="{y + oy:.1f}" width="{wpx:.1f}" height="{size * 1.25:.1f}" '
                           f'fill="#ffffff" opacity="0.92"/>')
        self.el.append(f'<text x="{x:.1f}" y="{y:.1f}" font-family="{family}" font-size="{size}" '
                       f'font-weight="{weight}" fill="{color}" text-anchor="{anchor}" '
                       f'dominant-baseline="{base}">{escape(s)}</text>')

    def dot(self, x, y, color="#cc0000"):
        self.el.append(f'<circle cx="{x}" cy="{y}" r="6" fill="{color}"/>')

    def cross(self, x, y, s=8):
        self.line([(x - s, y - s), (x + s, y + s)], "#14a014", 3)
        self.line([(x - s, y + s), (x + s, y - s)], "#14a014", 3)

    def svg(self):
        return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {self.w} {self.h}" '
                f'width="{self.w}" height="{self.h}" role="img">' + "".join(self.el) + "</svg>")

    # ------------------------------------------------------------ Bauteile
    def block(self, x, y, w, h, title):
        self.rect(x, y, w, h, stroke="#9a9a9a", sw=2, dash="10 6")
        self.text(x + 14, y + 20, title, 17, INK, weight="bold", family=SANS)

    def module(self, key, x, y, w, title, sub, left=(), right=(), pitch=40, top=64, fill=BOXFILL, nums=True):
        """left/right: (pin_id, nummer, text, status) status: used | nc | free | bad | gap"""
        n = max(len(left), len(right), 1)
        h = top + (n - 1) * pitch + 36
        self.rect(x, y, w, h, stroke=BOX, fill=fill, sw=3)
        self.text(x + w / 2, y + 20, title, 19, BOX, anchor="middle", weight="bold", family=SANS)
        if sub:
            self.text(x + w / 2, y + 42, sub, 13, "#7a3a3a", anchor="middle", family=SANS)
        for side, lst in (("L", left), ("R", right)):
            for i, (pid, num, label, st) in enumerate(lst):
                if st == "gap":
                    continue
                py = y + top + i * pitch
                tc = {"used": INK, "nc": "#9a9a9a", "free": "#9a9a9a", "bad": "#c81414"}[st]
                if side == "L":
                    self.line([(x - 30, py), (x, py)], BOX, 3)
                    self.text(x + 8, py, label, 15, tc, weight="bold" if st == "used" else "normal")
                    if nums and num != "":
                        self.text(x - 5, py - 9, str(num), 12, "#666", anchor="end")
                    end = (x - 30, py)
                else:
                    self.line([(x + w, py), (x + w + 30, py)], BOX, 3)
                    self.text(x + w - 8, py, label, 15, tc, anchor="end", weight="bold" if st == "used" else "normal")
                    if nums and num != "":
                        self.text(x + w + 5, py - 9, str(num), 12, "#666")
                    end = (x + w + 30, py)
                self.pins[f"{key}.{pid}"] = (end[0], end[1], side)
                if st == "nc":
                    self.cross(*end)
        return h

    def p(self, name):
        x, y, _ = self.pins[name]
        return (x, y)

    def wire(self, a, b, via=(), color=INK, w=3, label=None, at=None, dash=None):
        pa = self.p(a) if isinstance(a, str) else a
        pb = self.p(b) if isinstance(b, str) else b
        pts = [pa] + list(via) + [pb]
        self.line(pts, color, w, dash)
        if label:
            lx, ly = at if at else ((pts[0][0] + pts[1][0]) / 2, pts[0][1])
            self.text(lx, ly - 10, label, 13, color, anchor="middle", weight="bold", family=SANS, bg=True)

    def gnd(self, x, y):
        self.line([(x, y), (x, y + 12)], INK, 3)
        self.line([(x - 14, y + 12), (x + 14, y + 12)], INK, 3)
        self.line([(x - 9, y + 18), (x + 9, y + 18)], INK, 3)
        self.line([(x - 4, y + 24), (x + 4, y + 24)], INK, 3)

    def pwr(self, x, y, name, color=C["pwr"], up=True):
        d = -1 if up else 1
        self.line([(x, y), (x, y + d * 14)], color, 3)
        self.line([(x - 12, y + d * 14), (x + 12, y + d * 14)], color, 3)
        self.text(x, y + d * 26, name, 13, color, anchor="middle", weight="bold", family=SANS)

    def flag(self, x, y, name, color=INK, side="R"):
        """Netzname am Leitungsende. side = Richtung, in die die Leitung zeigt."""
        wpx = len(name) * 8.4 + 14
        if side == "R":
            pts = f"{x},{y} {x + 8},{y - 10} {x + 8 + wpx},{y - 10} {x + 8 + wpx},{y + 10} {x + 8},{y + 10}"
            tx, anc = x + 12, "start"
        else:
            pts = f"{x},{y} {x - 8},{y - 10} {x - 8 - wpx},{y - 10} {x - 8 - wpx},{y + 10} {x - 8},{y + 10}"
            tx, anc = x - 12, "end"
        self.el.append(f'<polygon points="{pts}" fill="#ffffff" stroke="{color}" stroke-width="2"/>')
        self.text(tx, y, name, 13, color, anchor=anc, weight="bold")

    # passive Bauteile, horizontal (Start links, Ende rechts) oder vertikal (Start oben)
    def resistor(self, x, y, ref, val, vertical=False, color=INK):
        if not vertical:
            self.line([(x, y), (x + 14, y)], color)
            self.rect(x + 14, y - 7, 32, 14, stroke=color, fill="#ffffff", sw=2.5)
            self.line([(x + 46, y), (x + 60, y)], color)
            self.text(x + 30, y - 16, ref, 12, "#444", anchor="middle")
            self.text(x + 30, y + 18, val, 12, INK, anchor="middle", weight="bold")
            return (x + 60, y)
        self.line([(x, y), (x, y + 14)], color)
        self.rect(x - 7, y + 14, 14, 32, stroke=color, fill="#ffffff", sw=2.5)
        self.line([(x, y + 46), (x, y + 60)], color)
        self.text(x + 12, y + 24, ref, 12, "#444")
        self.text(x + 12, y + 38, val, 12, INK, weight="bold")
        return (x, y + 60)

    def cap(self, x, y, ref, val, vertical=True, polar=False, color=INK):
        if vertical:
            self.line([(x, y), (x, y + 22)], color)
            self.line([(x - 13, y + 22), (x + 13, y + 22)], color, 3)
            self.line([(x - 13, y + 30), (x + 13, y + 30)], color, 3)
            self.line([(x, y + 30), (x, y + 52)], color)
            if polar:
                self.text(x - 18, y + 16, "+", 13, color)
            self.text(x + 18, y + 18, ref, 12, "#444")
            self.text(x + 18, y + 34, val, 12, INK, weight="bold")
            return (x, y + 52)
        self.line([(x, y), (x + 22, y)], color)
        self.line([(x + 22, y - 13), (x + 22, y + 13)], color, 3)
        self.line([(x + 30, y - 13), (x + 30, y + 13)], color, 3)
        self.line([(x + 30, y), (x + 52, y)], color)
        if polar:
            self.text(x + 14, y - 16, "+", 13, color)
        self.text(x + 26, y - 22, ref, 12, "#444", anchor="middle")
        self.text(x + 26, y + 26, val, 12, INK, anchor="middle", weight="bold")
        return (x + 52, y)

    def cap_gnd(self, x, y, ref, val, polar=False, color=INK):
        self.dot(x, y, INK)
        e = self.cap(x, y, ref, val, True, polar, color)
        self.gnd(*e)

    def inductor(self, x, y, ref, val):
        self.line([(x, y), (x + 10, y)])
        d = f"M{x + 10} {y}"
        for i in range(4):
            d += f" a 10 10 0 0 1 20 0"
        self.el.append(f'<path d="{d}" fill="none" stroke="{INK}" stroke-width="3"/>')
        self.line([(x + 90, y), (x + 100, y)])
        self.text(x + 50, y - 20, ref, 12, "#444", anchor="middle")
        self.text(x + 50, y + 18, val, 12, INK, anchor="middle", weight="bold")
        return (x + 100, y)

    def diode(self, x, y, ref, val, vertical=False, color=INK, kind="d"):
        """Anode am Start, Kathode am Ende. kind: d | z | led | schottky"""
        if not vertical:
            self.line([(x, y), (x + 14, y)], color)
            self.el.append(f'<polygon points="{x + 14},{y - 11} {x + 14},{y + 11} {x + 34},{y}" fill="#ffffff" '
                           f'stroke="{color}" stroke-width="2.5"/>')
            self.line([(x + 34, y - 11), (x + 34, y + 11)], color, 3)
            self.line([(x + 34, y), (x + 48, y)], color)
            self.text(x + 24, y - 20, ref, 12, "#444", anchor="middle")
            self.text(x + 24, y + 22, val, 12, INK, anchor="middle", weight="bold")
            return (x + 48, y)
        self.line([(x, y), (x, y + 14)], color)
        self.el.append(f'<polygon points="{x - 11},{y + 14} {x + 11},{y + 14} {x},{y + 34}" fill="#ffffff" '
                       f'stroke="{color}" stroke-width="2.5"/>')
        self.line([(x - 11, y + 34), (x + 11, y + 34)], color, 3)
        if kind == "led":
            self.line([(x + 14, y + 18), (x + 24, y + 10)], color, 2)
            self.line([(x + 14, y + 26), (x + 24, y + 18)], color, 2)
        self.line([(x, y + 34), (x, y + 48)], color)
        self.text(x + 16, y + 30, ref, 12, "#444")
        self.text(x + 16, y + 44, val, 12, INK, weight="bold")
        return (x, y + 48)

    def button(self, x, y, name, label_pos="above"):
        """Taster horizontal: Anschluss a = (x, y), b = (x + 110, y)"""
        self.line([(x, y), (x + 32, y)], BOX, 3)
        self.line([(x + 78, y), (x + 110, y)], BOX, 3)
        for cx in (x + 36, x + 74):
            self.el.append(f'<circle cx="{cx}" cy="{y}" r="4" fill="#ffffff" stroke="{BOX}" stroke-width="2"/>')
        self.line([(x + 28, y - 14), (x + 82, y - 14)], BOX, 3)
        self.line([(x + 55, y - 14), (x + 55, y - 26)], BOX, 3)
        if label_pos == "above":
            self.text(x + 55, y - 38, name, 16, "#1f2f9f", anchor="middle", weight="bold", family=SANS)
        else:
            self.text(x + 55, y + 20, name, 16, "#1f2f9f", anchor="middle", weight="bold", family=SANS)
        return (x, y), (x + 110, y)

    def note(self, x, y, lines, title=None, size=16, width=None):
        if title:
            self.text(x, y, title, size + 1, INK, weight="bold", family=SANS)
            y += size + 12
        for ln in lines:
            self.text(x, y, ln, size, "#333", family=SANS)
            y += size + 8
        return y

    def legend(self, x, y, items, size=15):
        for color, txt in items:
            self.line([(x, y), (x + 44, y)], color, 5)
            self.text(x + 56, y, txt, size, "#333", family=SANS)
            y += size + 12
        return y


# =====================================================================================
# Variante A: Testaufbau
# =====================================================================================
def testaufbau():
    s = Svg(2620, 1260, "NR Bodypack · Variante A: Testaufbau mit Modulen",
            "Jede Linie = ein Steckkabel. Pin-Nummern wie auf den Platinen (S3: Leisten H1/H2, C5-Zero: Draufsicht, USB oben). "
            "Standard-Pins der Firmware.")
    U = "used"
    # ---------------- ESP32-S3 AMOLED
    h1 = ["GP43", "GP44", "GND", "GP0 BOOT", "GP1 BAT", "GP2", "GP3", "GND", "GP11", "GP12", "GP13", "GP14",
          "GND", "GP15", "GP19 USB", "GP20 USB", "GP21", "GND", "GP26 PSRAM", "GP33 PSRAM"]
    h2 = ["VBUS", "VSYS", "GND", "3V3_EN", "3V3", "GP17 RESET", "GP16", "GND", "GP10", "GP4", "CHIP_UP",
          "GP40 SDA", "GND", "GP39 SCL", "GP38", "GP37 PSRAM", "GP36 PSRAM", "GND", "GP35 PSRAM", "GP34 PSRAM"]
    u1, u2 = {8, 9, 10, 11, 12, 13, 14}, {5, 7, 8, 9, 10}

    def st(lab, n, used):
        if n in used:
            return U
        return "bad" if any(k in lab for k in ("PSRAM", "RESET", "BOOT", "BAT", "USB")) else "free"

    s.module("S3", 430, 170, 300, "ESP32-S3 AMOLED", "Waveshare 1,91″ · Display + Drehgeber",
             [(f"H1_{i + 1}", f"H1-{i + 1}", t, st(t, i + 1, u1)) for i, t in enumerate(h1)],
             [(f"H2_{i + 1}", f"H2-{i + 1}", t, st(t, i + 1, u2)) for i, t in enumerate(h2)], pitch=30, top=60)
    # ---------------- ESP32-C5-Zero
    c5l = [("5V", "", "5V", "free"), ("GND", "", "GND", U), ("3V3", "", "3V3", U), ("GP0", "", "GP0", U),
           ("GP1", "", "GP1", U), ("GP2", "", "GP2", U), ("GP3", "", "GP3", U), ("GP4", "", "GP4", U),
           ("GP5", "", "GP5", U)]
    c5r = [("GP11", "", "GP11", U), ("GP12", "", "GP12", U), ("GP13", "", "GP13 USB", "free"),
           ("GP14", "", "GP14 USB", "free"), ("GP10", "", "GP10", U), ("GP9", "", "GP9", U), ("GP8", "", "GP8", U),
           ("GP7", "", "GP7", "free"), ("GP6", "", "GP6", U)]
    s.module("C5", 1250, 330, 220, "ESP32-C5-Zero", "Funk + Audio · IPEX-Antenne", c5l, c5r, pitch=50, top=60)
    # ---------------- WM8960
    wm = [(1, "VCC 3V3", U), (2, "GND", U), (3, "SDA", U), (4, "SCL", U), (5, "CLK", U), (6, "WS", U),
          (7, "RXSDA", U), (8, "RXMCLK", "nc"), (9, "VCC 3V3", "free"), (10, "GND", "free"), (11, "NC", "nc"),
          (12, "NC", "nc"), (13, "CLK", "free"), (14, "WS", "free"), (15, "TXSDA", U), (16, "TXMCLK", "nc")]
    s.module("WM", 1950, 150, 260, "WM8960", "Waveshare Audio Board",
             [(str(n), n, t, sta) for n, t, sta in wm], [], pitch=40, top=60)
    p = s.p

    # ---------------- USB-Versorgung (beide Boards einzeln am PC / USB-Netzteil)
    for x, y, name in ((580, 170, "USB-C → PC (Strom + Flashen)"), (1360, 330, "USB-C → PC (Strom + Konsole)")):
        s.rect(x - 34, y - 34, 68, 22, stroke=C["usb"], fill="#eef1f4", sw=2, rx=6)
        s.line([(x, y - 12), (x, y)], C["usb"], 3)
        s.text(x, y - 46, name, 13, C["usb"], anchor="middle", weight="bold", family=SANS)

    # ---------------- C5 <-> WM8960
    s.wire("C5.3V3", "WM.1", [(1185, p("C5.3V3")[1]), (1185, 180), (1880, 180), (1880, p("WM.1")[1])],
           C["pwr"], label="3V3 (vom C5)", at=(1330, 180))
    s.wire((1200, p("C5.GND")[1]), "WM.2", [(1200, 220), (1860, 220), (1860, p("WM.2")[1])], C["gnd"],
           label="GND", at=(1330, 220))
    s.wire("C5.GP10", "WM.4", [(1650, p("C5.GP10")[1]), (1650, p("WM.4")[1])], C["i2c"], label="SCL",
           at=(1800, p("WM.4")[1]))
    s.wire("C5.GP9", "WM.3", [(1680, p("C5.GP9")[1]), (1680, p("WM.3")[1])], C["i2c"], label="SDA",
           at=(1800, p("WM.3")[1]))
    s.wire("C5.GP8", "WM.7", [(1710, p("C5.GP8")[1]), (1710, p("WM.7")[1])], C["i2sd"], label="DOUT → RXSDA",
           at=(1600, p("C5.GP8")[1]))
    s.wire("C5.GP6", "WM.15", [(1740, p("C5.GP6")[1]), (1740, p("WM.15")[1])], C["i2sd"], label="TXSDA → DIN",
           at=(1600, p("C5.GP6")[1]))
    s.wire("C5.GP4", "WM.5", [(1140, p("C5.GP4")[1]), (1140, 1030), (1830, 1030), (1830, p("WM.5")[1])],
           C["i2s"], label="BCLK (Takt kommt vom WM8960)", at=(1480, 1030))
    s.wire("C5.GP5", "WM.6", [(1160, p("C5.GP5")[1]), (1160, 1000), (1800, 1000), (1800, p("WM.6")[1])],
           C["i2s"], label="WS / LRCLK (vom WM8960)", at=(1480, 1000))

    # ---------------- Tasten am C5
    gx = 1040
    for name, pin in (("T1", "C5.GP0"), ("T2", "C5.GP1")):
        y = p(pin)[1]
        a, b = s.button(1070, y, name, "above" if name == "T1" else "below")
        s.wire(b, pin, color=C["btn"])
        s.wire(a, (gx, y), color=C["gnd"])
        s.dot(gx, y, "#cc0000")
    s.wire((gx, p("C5.GND")[1]), (gx, p("C5.GP1")[1]), color=C["gnd"])
    s.dot(gx, p("C5.GND")[1])
    for name, pin, lane, by in (("T4", "C5.GP11", 1530, 90), ("T3", "C5.GP12", 1560, 140)):
        a, b = s.button(1590, by, name, "above" if name == "T4" else "below")
        s.wire(pin, a, [(lane, p(pin)[1]), (lane, by)], C["btn"], label=pin.split(".")[1],
               at=(lane - 2 + 0, p(pin)[1] - 2))
        s.wire(b, (1740, by), color=C["gnd"])
    s.wire((1740, 90), (1740, 220), color=C["gnd"])
    s.dot(1740, 140)
    s.dot(1740, 220)
    s.dot(1200, p("C5.GND")[1])

    # ---------------- S3 <-> C5: GND + UART (vorbereitet)
    s.wire("S3.H2_8", "C5.GND", color=C["gnd"], label="GND gemeinsam (S3 ↔ C5)", at=(890, p("S3.H2_8")[1]))
    s.wire("S3.H2_10", "C5.GP3", [(930, p("S3.H2_10")[1]), (930, p("C5.GP3")[1])], C["uart"],
           label="UART S3-TX (GPIO4) → C5-RX (GP3)", at=(1060, p("C5.GP3")[1]))
    s.wire("S3.H2_9", "C5.GP2", [(900, p("S3.H2_9")[1]), (900, p("C5.GP2")[1])], C["uart"],
           label="UART C5-TX (GP2) → S3-RX (GPIO10)", at=(1060, p("C5.GP2")[1]))

    # ---------------- Drehgeber am S3
    s.module("E1", 110, 340, 180, "Drehgeber 1", "KY-040 · links", [],
             [("+", "", "+", U), ("GND", "", "GND", U), ("CLK", "", "CLK", U), ("DT", "", "DT", U),
              ("SW", "", "SW", U)], pitch=30, top=70)
    s.wire("E1.GND", "S3.H1_8", color=C["gnd"], label="GND", at=(345, p("E1.GND")[1]))
    s.wire("E1.CLK", "S3.H1_9", color=C["enc"], label="GP11", at=(345, p("E1.CLK")[1]))
    s.wire("E1.DT", "S3.H1_10", color=C["enc"], label="GP12", at=(345, p("E1.DT")[1]))
    s.wire("E1.SW", "S3.H1_11", color=C["enc"], label="GP13", at=(345, p("E1.SW")[1]))
    s.wire("E1.+", "S3.H2_5", [(320, p("E1.+")[1]), (320, 100), (800, 100), (800, p("S3.H2_5")[1])],
           C["pwr"], label="3V3 (vom S3, H2-5)", at=(440, 100))
    s.module("E2", 110, 640, 180, "Drehgeber 2", "KY-040 · rechts", [],
             [("CLK", "", "CLK", U), ("GND", "", "GND", U), ("SW", "", "SW", U), ("+", "", "+", U),
              ("DT", "", "DT", U)], pitch=30, top=50)
    s.wire("S3.H1_12", "E2.CLK", [(330, p("S3.H1_12")[1]), (330, p("E2.CLK")[1])], C["enc2"], label="GP14",
           at=(365, p("S3.H1_12")[1]))
    s.wire("S3.H1_13", "E2.GND", [(350, p("S3.H1_13")[1]), (350, p("E2.GND")[1])], C["gnd"], label="GND",
           at=(375, p("S3.H1_13")[1]))
    s.wire("S3.H1_14", "E2.SW", [(370, p("S3.H1_14")[1]), (370, p("E2.SW")[1])], C["enc2"], label="GP15",
           at=(385, p("S3.H1_14")[1]))
    s.wire("E2.+", (800, p("S3.H2_5")[1]), [(310, p("E2.+")[1]), (310, 900), (820, 900), (820, p("S3.H2_5")[1])],
           C["pwr"], label="3V3", at=(560, 900))
    s.dot(800, p("S3.H2_5")[1])
    s.dot(820, p("S3.H2_5")[1])
    s.wire("E2.DT", "S3.H2_7", [(300, p("E2.DT")[1]), (300, 930), (850, 930), (850, p("S3.H2_7")[1])],
           C["enc2"], label="DT → GP16 (H2-7)", at=(560, 930))

    # ---------------- Headset
    s.rect(2270, 270, 300, 230, stroke=BOX, fill=BOXFILL, sw=3)
    s.text(2420, 292, "Headset-Adapter", 18, BOX, anchor="middle", weight="bold", family=SANS)
    s.note(2290, 330, ["XLR-Buchse (Mikrofon) +", "6,35-mm-Buchse (Kopfhörer)", "→ 3,5-mm-Stecker 4-pol. (CTIA)"],
           size=14)
    s.text(2420, 450, "Audio-Technica BPHS1", 15, INK, anchor="middle", weight="bold", family=SANS)
    s.text(2420, 474, "Mic dynamisch: mic jack 30 3", 13, "#555", anchor="middle", family=SANS)
    s.line([(2210, 385), (2270, 385)], C["ana"], 5)
    s.text(2240, 365, "3,5 mm", 13, C["ana"], anchor="middle", weight="bold", family=SANS)
    s.text(2240, 410, "Buchse am", 12, "#555", anchor="middle", family=SANS)
    s.text(2240, 425, "WM8960", 12, "#555", anchor="middle", family=SANS)

    # ---------------- Hinweise / Legende
    s.note(40, 1000, [
        "• Beide Boards einzeln per USB-C versorgen (PC oder USB-Netzteil). GND zwischen S3 und C5 verbinden.",
        "• WM8960: CLK/WS gibt es doppelt (Pin 5/13, 6/14), je ein Draht genügt. RXMCLK/TXMCLK NICHT anschließen.",
        "• KY-040: Pins nach Beschriftung auf dem Modul stecken (Reihenfolge im Plan ist zur Übersicht sortiert).",
        "• UART S3 ↔ C5 über Kreuz: S3 GPIO4 → C5 GP3, C5 GP2 → S3 GPIO10 (läuft, Bodypack-Menü am S3).",
        "• Rot beschriftete S3-Pins frei lassen (PSRAM, Display-Reset, BOOT, Akku, USB).",
    ], "Hinweise", size=15)
    s.legend(1950, 940, [(C["pwr"], "3,3 V"), (C["gnd"], "GND"), (C["i2c"], "I2C Codec-Steuerung"),
                         (C["i2s"], "I2S-Takte (WM8960 → C5)"), (C["i2sd"], "I2S-Daten"), (C["btn"], "Tasten / Drehgeber 1"),
                         (C["enc2"], "Drehgeber 2"), (C["uart"], "UART S3 ↔ C5")], size=14)
    s.cross(2330, 1180)
    s.text(2350, 1180, "= nicht anschließen", 14, "#333", family=SANS)
    s.dot(2330, 1210)
    s.text(2350, 1210, "= Verbindung", 14, "#333", family=SANS)
    return s.svg()


# =====================================================================================
# Variante B: eigene Traegerplatine (Entwurf V0.1)
# =====================================================================================
def platine():
    s = Svg(3660, 3080, "NR Bodypack · Variante B: Trägerplatine V0.2 (Entwurf, S3 2,8″ + Basisstation, nicht für die Fertigung freigegeben)",
            "IC-Pins mit Namen (Pin-Nummern aus dem EasyEDA/LCSC-Symbol übernehmen). Gleiche Netznamen in Fähnchen = "
            "gleiche Leitung. Werte mit * vor dem Layout gegen das Datenblatt prüfen.")
    U = "used"
    p = s.p

    def to_flag(pin, name, col=INK, length=60):
        x, y, side = s.pins[pin]
        d = -1 if side == "L" else 1
        s.line([(x, y), (x + d * length, y)], col)
        s.flag(x + d * length, y, name, col, "L" if side == "L" else "R")

    def to_gnd(pin, length=30):
        x, y, side = s.pins[pin]
        d = -1 if side == "L" else 1
        s.line([(x, y), (x + d * length, y)])
        s.gnd(x + d * length, y)

    def to_text(pin, txt, col=INK, length=24):
        x, y, side = s.pins[pin]
        d = -1 if side == "L" else 1
        s.line([(x, y), (x + d * length, y)], col)
        s.text(x + d * (length + 6), y, txt, 12, col, anchor="end" if side == "L" else "start", weight="bold")

    def cap_row(pin, ref, val, length=20):
        """Kondensator vom Pin waagerecht nach GND (bleibt in der eigenen Zeile)"""
        x, y, side = s.pins[pin]
        if side == "R":
            s.line([(x, y), (x + length, y)])
            e = s.cap(x + length, y, ref, val, vertical=False)
            s.line([e, (e[0] + 14, y)]); s.gnd(e[0] + 14, y)
        else:
            s.line([(x, y), (x - length - 66, y)])
            s.cap(x - length - 66, y, ref, val, vertical=False)
            s.gnd(x - length - 80, y); s.line([(x - length - 66, y), (x - length - 80, y)])

    def small(x, y, txt, col="#444", anchor="start"):
        s.text(x, y, txt, 12, col, anchor=anchor, family=SANS)

    def under(pin, txt, off=40):
        """kleine Bauteil-Angabe unter der Leitung am Pin"""
        x, y, side = s.pins[pin]
        if side == "L":
            small(x - off, y + 18, txt, anchor="end")
        else:
            small(x + off, y + 18, txt)

    # ======================================================================= A · USB
    s.block(30, 100, 1080, 840, "A · USB-C (Laden + Daten), ESD-Schutz, USB-Hub")
    s.module("J1", 90, 170, 190, "J1 USB-C", "16-Pin, USB 2.0", [],
             [("VBUS", "", "VBUS", U), ("CC1", "", "CC1", U), ("CC2", "", "CC2", U), ("DP", "", "D+", U),
              ("DM", "", "D−", U), ("GND", "", "GND / Schirm", U)], pitch=46, top=70, nums=False)
    x, y = p("J1.VBUS")
    s.line([(x, y), (x + 30, y)], C["pwr"])
    e = s.resistor(x + 30, y, "F1", "PTC 3 A", color=C["pwr"])
    s.line([e, (e[0] + 40, y)], C["pwr"]); s.flag(e[0] + 40, y, "VBUS", C["pwr"])
    for pin, ref in (("J1.CC1", "R1"), ("J1.CC2", "R2")):
        x, y = p(pin)
        e = s.resistor(x, y, ref, "5,1 k")
        s.line([e, (e[0] + 20, y), (e[0] + 20, y + 10)]); s.gnd(e[0] + 20, y + 10)
    to_flag("J1.DP", "USB_DP", C["usb"], 130); to_flag("J1.DM", "USB_DM", C["usb"], 130); to_gnd("J1.GND")

    s.module("U1", 680, 170, 220, "U1 USBLC6-2SC6", "ESD-Schutz USB",
             [("I1", "", "I/O1", U), ("I2", "", "I/O2", U), ("GND", "", "GND", U)],
             [("O1", "", "I/O1", U), ("O2", "", "I/O2", U), ("VBUS", "", "VBUS", U)], pitch=46, top=70, nums=False)
    to_flag("U1.I1", "USB_DP", C["usb"], 40); to_flag("U1.I2", "USB_DM", C["usb"], 40); to_gnd("U1.GND")
    to_flag("U1.O1", "USB_DP", C["usb"], 30); to_flag("U1.O2", "USB_DM", C["usb"], 30)
    to_flag("U1.VBUS", "VBUS", C["pwr"], 30)
    small(690, 360, "liegt direkt hinter J1 auf D+/D− und VBUS")

    s.module("U2", 400, 520, 260, "U2 USB-Hub 2.0", "z. B. CH334F (4 Ports)*",
             [("VBUS", "", "VBUS / V5", U), ("V33", "", "V33 (intern)", U), ("DP", "", "UP D+", U),
              ("DM", "", "UP D−", U), ("GND", "", "GND", U)],
             [("D1P", "", "DN1 D+", U), ("D1M", "", "DN1 D−", U), ("D2P", "", "DN2 D+", U), ("D2M", "", "DN2 D−", U),
              ("XT", "", "Takt lt. DB*", "nc")], pitch=50, top=70, nums=False)
    to_flag("U2.VBUS", "VBUS", C["pwr"], 150); under("U2.VBUS", "C1 10 µF → GND", 20)
    to_text("U2.V33", "C2 1 µF → GND")
    to_flag("U2.DP", "USB_DP", C["usb"], 150); to_flag("U2.DM", "USB_DM", C["usb"], 150); to_gnd("U2.GND")
    for pin, name in (("U2.D1P", "C5_USB_DP"), ("U2.D1M", "C5_USB_DM"), ("U2.D2P", "S3_USB_DP"),
                      ("U2.D2M", "S3_USB_DM")):
        to_flag(pin, name, C["usb"], 40)
    s.module("U15", 880, 310, 220, "U15 TPS2116", "5 V: USB / Station",
             [("IN1", "", "VIN1", U), ("IN2", "", "VIN2", U), ("PR1", "", "PR1 lt. DB*", "nc"), ("GND", "", "GND", U)],
             [("OUT", "", "VOUT", U), ("ST", "", "ST", "nc")], pitch=40, top=64, nums=False)
    to_flag("U15.IN1", "VBUS", C["pwr"], 40); to_flag("U15.IN2", "DOCK_5V", C["pwr"], 40); to_gnd("U15.GND")
    to_flag("U15.OUT", "VBUS_CHG", C["pwr"], 30)
    s.note(60, 850, ["Ein USB-C für alles: Laden (über U15 → U3) und Flashen/Konsole beider ESP32 über den Hub.",
                     "In der Basisstation lädt U15 aus deren 5 V (DOCK_5V). Die USB-C-Buchsen der Module bleiben unbenutzt."],
           size=14)

    # ======================================================================= B · Laden + Akku
    s.block(1130, 100, 1300, 840, "B · Laderegler mit Power-Path, 2× 18650 parallel, Schutz, Ladezustand")
    s.module("U3", 1400, 170, 260, "U3 BQ25895", "Lader 3 A, Power-Path, I2C 0x6A",
             [("VBUS", "", "VBUS", U), ("PMID", "", "PMID", U), ("REGN", "", "REGN", U), ("ILIM", "", "ILIM", U),
              ("TS", "", "TS", U), ("DP", "", "D+", U), ("DM", "", "D−", U), ("OTG", "", "OTG", U), ("CE", "", "/CE", U),
              ("PGND", "", "PGND / GND", U)],
             [("SW", "", "SW", U), ("BTST", "", "BTST", U), ("SYS", "", "SYS", U), ("BAT", "", "BAT", U),
              ("SDA", "", "SDA", U), ("SCL", "", "SCL", U), ("INT", "", "/INT", U), ("STAT", "", "STAT", U),
              ("QON", "", "/QON", U), ("DSEL", "", "DSEL", "nc")], pitch=50, top=70, nums=False)
    to_flag("U3.VBUS", "VBUS_CHG", C["pwr"], 120); under("U3.VBUS", "C3 1 µF → GND", 10)
    to_text("U3.PMID", "C4 10 µF → GND")
    to_flag("U3.REGN", "REGN", C["pwr"], 120); under("U3.REGN", "C5 4,7 µF → GND", 10)
    to_text("U3.ILIM", "R3 R_ILIM* → GND")
    to_flag("U3.TS", "TS", C["ana"], 120)
    to_flag("U3.DP", "USB_DP", C["usb"], 120); to_flag("U3.DM", "USB_DM", C["usb"], 120)
    for pin in ("U3.OTG", "U3.CE", "U3.PGND"):
        to_gnd(pin)
    x, y = p("U3.SW"); s.line([(x, y), (x + 20, y)])
    e = s.inductor(x + 20, y, "L1", "2,2 µH / 5 A")
    ys = p("U3.SYS")[1]
    s.line([e, (e[0] + 20, y), (e[0] + 20, ys)], C["pwr"]); s.dot(e[0] + 20, ys, INK)
    xb, yb = p("U3.BTST"); s.line([(xb, yb), (xb + 10, yb)])
    e2 = s.cap(xb + 10, yb, "C6", "47 nF", vertical=False)
    s.line([e2, (xb + 80, yb), (xb + 80, y)]); s.dot(xb + 80, y, INK)
    x, y = p("U3.SYS"); s.line([(x, y), (x + 170, y)], C["pwr"]); s.flag(x + 170, y, "VSYS", C["pwr"])
    small(x + 70, y + 18, "C7 2× 10 µF → GND")
    x, y = p("U3.BAT"); s.line([(x, y), (x + 170, y)], C["pwr"]); s.flag(x + 170, y, "BAT+", C["pwr"])
    small(x + 70, y + 18, "C8 10 µF → GND")
    for pin, name in (("U3.SDA", "SDA_S3"), ("U3.SCL", "SCL_S3"), ("U3.INT", "BQ_INT")):
        to_flag(pin, name, C["i2c"], 170)
    small(p("U3.INT")[0] + 40, p("U3.INT")[1] + 16, "R4 10 k → +3V3 (Pull-up)")
    x, y = p("U3.STAT"); s.line([(x, y), (x + 20, y)])
    e = s.resistor(x + 20, y, "R5", "2,2 k")
    e2 = s.diode(e[0], y, "D1", "LED rot", kind="led")
    s.line([e2, (e2[0] + 20, y)], C["pwr"]); s.flag(e2[0] + 20, y, "VSYS", C["pwr"])
    x, y = p("U3.QON"); s.line([(x, y), (x + 20, y)])
    a, b = s.button(x + 20, y, "S1 EIN/AUS", "below")
    s.line([b, (b[0] + 16, y), (b[0] + 16, y + 6)]); s.gnd(b[0] + 16, y + 6)
    s.dot(x + 10, y, INK); s.line([(x + 10, y), (x + 10, y + 56), (x + 30, y + 56)], C["btn"])
    e = s.diode(x + 30, y + 56, "D2", "BAT54", color=C["btn"])
    s.line([e, (e[0] + 20, y + 56)], C["btn"]); s.flag(e[0] + 20, y + 56, "PWR_KEY", C["btn"])
    small(1690, 880, "S1 kurz: Ein (verlässt den Ship-Mode)")
    small(1690, 900, "S1 lang: S3 meldet ab → Ship-Mode (Aus)")

    # TS-Netzwerk
    bx, by = 1180, 790
    s.text(bx, by - 34, "TS-Netzwerk (Akku-Temperatur)", 13, INK, weight="bold", family=SANS)
    s.flag(bx + 20, by, "REGN", C["pwr"], "L")
    e = s.resistor(bx + 20, by, "R6", "5,23 k*")
    s.line([e, (e[0] + 40, by)], C["ana"]); s.dot(e[0] + 40, by, INK)
    s.line([(e[0] + 40, by), (e[0] + 40, by - 18), (e[0] + 60, by - 18)], C["ana"]); s.flag(e[0] + 60, by - 18, "TS", C["ana"])
    s.gnd(*s.resistor(e[0] + 40, by, "R7", "30,1 k*", vertical=True))
    s.line([(e[0] + 40, by), (e[0] + 130, by)])
    s.gnd(*s.resistor(e[0] + 130, by, "RT1", "NTC 10 k", vertical=True))

    # Akkus
    ax, ay = 2060, 190
    s.text(ax, ay - 16, "BT1, BT2: 2× 18650 parallel (gleiche Zellen!)", 13, INK, weight="bold", family=SANS)
    for i in range(2):
        cx = ax + 40 + i * 130
        s.line([(cx, ay + 10), (cx, ay + 40)], C["pwr"])
        s.line([(cx - 22, ay + 40), (cx + 22, ay + 40)], INK, 4)
        s.line([(cx - 12, ay + 50), (cx + 12, ay + 50)], INK, 4)
        s.line([(cx, ay + 50), (cx, ay + 90)])
        s.text(cx + 28, ay + 45, f"BT{i + 1}", 13, INK, weight="bold")
    s.line([(ax + 40, ay + 10), (ax + 220, ay + 10)], C["pwr"]); s.flag(ax + 220, ay + 10, "BAT+", C["pwr"])
    s.line([(ax + 40, ay + 90), (ax + 220, ay + 90)]); s.flag(ax + 220, ay + 90, "BAT−", INK)
    s.dot(ax + 170, ay + 10, INK); s.dot(ax + 170, ay + 90, INK)

    s.module("U4", 2170, 360, 160, "U4 DW01A", "Schutz · C9 100 nF an VCC",
             [("VCC", "", "VCC", U), ("GND", "", "GND", U), ("CS", "", "CS", U)],
             [("OD", "", "OD", U), ("OC", "", "OC", U), ("TD", "", "TD", "nc")], pitch=40, top=64, nums=False)
    x, y = p("U4.VCC"); s.line([(x, y), (x - 10, y)])
    s.resistor(x - 70, y, "R8", "100 Ω"); s.flag(x - 70, y, "BAT+", C["pwr"], "L")
    to_flag("U4.GND", "BAT−", INK, 90)
    x, y = p("U4.CS"); s.line([(x, y), (x - 10, y)]); s.resistor(x - 70, y, "R9", "1 k"); s.flag(x - 70, y, "P−", INK, "L")
    s.module("Q1", 2170, 560, 160, "Q1 FS8205A", "2× N-MOSFET", [("G1", "", "G1", U), ("S1", "", "S1", U)],
             [("G2", "", "G2", U), ("S2", "", "S2", U)], pitch=40, top=64, nums=False)
    xo, yo = p("U4.OD"); xg, yg = p("Q1.G1")
    s.line([(xo, yo), (xo + 20, yo), (xo + 20, 530), (xg - 20, 530), (xg - 20, yg), (xg, yg)])
    xc, yc = p("U4.OC"); x2_, y2_ = p("Q1.G2")
    s.line([(xc, yc), (xc + 50, yc), (xc + 50, y2_), (x2_, y2_)])
    to_flag("Q1.S1", "BAT−", INK, 40); to_flag("Q1.S2", "P− = GND", INK, 30)
    s.text(2250, 700, "Drains intern verbunden", 12, "#555", anchor="middle", family=SANS)

    s.module("U5", 2170, 740, 180, "U5 MAX17048", "Ladezustand %, I2C 0x36",
             [("CELL", "", "CELL", U), ("VDD", "", "VDD", U), ("GND", "", "GND", U)],
             [("SDA", "", "SDA", U), ("SCL", "", "SCL", U), ("ALRT", "", "ALRT", U)], pitch=34, top=60, nums=False)
    to_flag("U5.CELL", "BAT+", C["pwr"], 60)
    to_text("U5.VDD", "= CELL, C10 1 µF → GND", C["pwr"])
    to_gnd("U5.GND", 20)
    for pin, name in (("U5.SDA", "SDA_S3"), ("U5.SCL", "SCL_S3"), ("U5.ALRT", "BAT_ALRT")):
        to_flag(pin, name, C["i2c"], 30)

    # ======================================================================= C · Spannungen
    s.block(2450, 100, 1180, 840, "C · Spannungen: 3,3 V (Logik), ±5 V (Analog), 48 V (Phantom)")
    s.module("U6", 2680, 170, 230, "U6 TPS63021", "Buck-Boost 3,3 V / 2 A",
             [("VIN", "", "VIN", U), ("VINA", "", "VINA", U), ("EN", "", "EN", U), ("PS", "", "PS/SYNC", U),
              ("GND", "", "GND/PGND", U)],
             [("L1", "", "L1", U), ("L2", "", "L2", U), ("VOUT", "", "VOUT", U), ("FB", "", "FB", U),
              ("PG", "", "PG", "nc")], pitch=46, top=70, nums=False)
    to_flag("U6.VIN", "VSYS", C["pwr"], 160); under("U6.VIN", "C11 2× 10 µF → GND", 96)
    x2, y2 = p("U6.VINA"); s.line([(x2, y2), (x2 - 10, y2)]); s.resistor(x2 - 70, y2, "R10", "100 Ω")
    s.line([(x2 - 70, y2), (x2 - 80, y2), (x2 - 80, p("U6.VIN")[1])], C["pwr"]); s.dot(x2 - 80, p("U6.VIN")[1], INK)
    to_text("U6.EN", "→ VSYS", C["pwr"]); to_text("U6.PS", "→ VSYS (PWM)", C["pwr"])
    to_gnd("U6.GND")
    xa, ya = p("U6.L1"); xb_, yb_ = p("U6.L2")
    s.line([(xa, ya), (xa + 10, ya)]); s.inductor(xa + 10, ya, "L2", "1,5 µH / 4 A")
    s.line([(xa + 110, ya), (xa + 124, ya), (xa + 124, yb_), (xb_, yb_)])
    x, y = p("U6.VOUT"); s.line([(x, y), (x + 200, y)], C["pwr"]); s.flag(x + 200, y, "+3V3", C["pwr"])
    small(x + 40, y + 18, "C12 3× 22 µF → GND")
    to_text("U6.FB", "→ VOUT (Festspannung)")

    s.module("U7", 2680, 560, 230, "U7 LM27762", "±5 V rauscharm, 250 mA",
             [("VIN", "", "VIN", U), ("EN", "", "EN+ / EN−", U), ("GND", "", "GND", U)],
             [("OUTP", "", "OUT+", U), ("OUTN", "", "OUT−", U), ("CP", "", "C1+/C1−*", U)], pitch=50, top=70, nums=False)
    to_flag("U7.VIN", "VSYS", C["pwr"], 160); under("U7.VIN", "C13 10 µF → GND", 20)
    to_flag("U7.EN", "ANA_EN", C["i2c"], 60); to_gnd("U7.GND")
    to_flag("U7.OUTP", "+5VA", C["pwr"], 200); under("U7.OUTP", "C14 10 µF → GND", 20)
    x, y = p("U7.OUTN"); s.line([(x, y), (x + 200, y)], C["pwr"]); s.flag(x + 200, y, "−5VA", C["pwr"])
    small(x + 20, y + 18, "C15 10 µF → GND")
    to_text("U7.CP", "C±, FB lt. DB*")

    s.module("U8", 3290, 170, 220, "U8 Boost 48 V", "z. B. LM5155 + 100-V-FET*",
             [("VIN", "", "VIN", U), ("EN", "", "EN/UVLO", U), ("GND", "", "GND", U)],
             [("VOUT", "", "VOUT", U), ("FB", "", "FB 1,0 V", U)], pitch=46, top=70, nums=False)
    to_flag("U8.VIN", "VSYS", C["pwr"], 60); to_flag("U8.EN", "P48_EN", C["p48"], 60); to_gnd("U8.GND")
    to_flag("U8.VOUT", "P48_RAW", C["p48"], 20); to_text("U8.FB", "R11/R12", INK, 16)
    s.note(3290, 380, ["R11 470 k (VOUT→FB), R12 10 k (FB→GND):", "1,0 V × (1 + 470/10) = 48 V",
                       "L, Diode 100 V, C 2× 1 µF/100 V,", "RT + Kompensation lt. Datenblatt*"], size=13)
    s.text(3230, 500, "P48-Schalter + Filter", 13, INK, weight="bold", family=SANS)
    s.flag(3300, 540, "P48_RAW", C["p48"], "L")
    s.line([(3300, 540), (3320, 540)], C["p48"])
    s.rect(3320, 520, 100, 40, stroke=C["p48"], fill="#ffffff", sw=2.5)
    s.text(3370, 534, "Q2 P-FET", 12, C["p48"], anchor="middle", weight="bold")
    s.text(3370, 549, "≥ 60 V*", 11, INK, anchor="middle")
    e = s.resistor(3420, 540, "R13", "100 Ω", color=C["p48"])
    s.line([e, (e[0] + 40, 540)], C["p48"])
    s.dot(e[0] + 40, 540, INK); s.flag(e[0] + 40, 540, "P48", C["p48"])
    s.note(3230, 610, ["C16 47 µF/63 V (+) am P48-Knoten → GND.",
                       "Gate: 100 k → P48_RAW, NPN + 10 k ← P48_SW,", "Zener 15 V G-S, Sanftanlauf 1 µF*.",
                       "Ausgang 47 k Entladung nach GND.", "Phantom nur auf XLR 2/3, nicht auf die Klinke."], size=13)

    # ======================================================================= D · C5
    s.block(30, 960, 1000, 820, "D · ESP32-C5-Zero (gesteckt): Funk, Audio, Tasten")
    c5l = [("5V", "", "5V", "nc"), ("GND", "", "GND", U), ("3V3", "", "3V3", U), ("GP0", "", "GP0", U),
           ("GP1", "", "GP1", U), ("GP2", "", "GP2", U), ("GP3", "", "GP3", U), ("GP4", "", "GP4", U),
           ("GP5", "", "GP5", U)]
    c5r = [("GP11", "", "GP11", U), ("GP12", "", "GP12", U), ("GP13", "", "GP13 D−", U), ("GP14", "", "GP14 D+", U),
           ("GP10", "", "GP10", U), ("GP9", "", "GP9", U), ("GP8", "", "GP8", U), ("GP7", "", "GP7", U),
           ("GP6", "", "GP6", U)]
    s.module("C5", 420, 1040, 220, "J2 ESP32-C5-Zero", "2× 9 Buchsenleiste", c5l, c5r, pitch=60, top=70)
    to_gnd("C5.GND")
    to_flag("C5.3V3", "+3V3", C["pwr"], 140); under("C5.3V3", "C44 10 µF → GND", 20)
    for k, (name, col) in {"GP0": ("DOCK_CS", C["btn"]), "GP1": ("DOCK_MISO", C["btn"]),
                           "GP2": ("UART_C5_TX", C["uart"]), "GP3": ("UART_S3_TX", C["uart"]), "GP4": ("I2S_BCLK", C["i2s"]),
                           "GP5": ("I2S_WS", C["i2s"])}.items():
        to_flag(f"C5.{k}", name, col, 60)
    for k, (name, col) in {"GP11": ("DOCK_SCK", C["btn"]), "GP12": ("DOCK_MOSI", C["btn"]), "GP13": ("C5_USB_DM", C["usb"]),
                           "GP14": ("C5_USB_DP", C["usb"]), "GP10": ("SCL_A", C["i2c"]), "GP9": ("SDA_A", C["i2c"]),
                           "GP8": ("I2S_DOUT", C["i2sd"]), "GP7": ("LED_DATA", C["led"]),
                           "GP6": ("I2S_DIN", C["i2sd"])}.items():
        to_flag(f"C5.{k}", name, col, 60)
    s.note(60, 1660, ["C5 ist I2S-Master (BCLK/WS zum Codec). Codec-PLL nutzt BCLK, kein MCLK nötig.",
                      "I2C-A (GP9/GP10): Codec U9 (0x18) + I/O-Expander U10 (0x20, auch Tasten T1–T4), Pull-ups R14/R15 4,7 k.",
                      "GP0/GP1/GP11/GP12: SPI zur Basisstation (W5500 dort), über R50–R53 33 Ω und ESD U14 an J7.",
                      "IPEX-Antenne nach oben aus dem Gehäuse führen. Modul-USB-C nicht benutzen."], size=13)

    # ======================================================================= E · S3
    s.block(1050, 960, 1000, 820, "E · ESP32-S3-Touch-LCD-2.8B (gesteckt): Display + Touch, Drehgeber, Akku")
    ln = {3: ("SDA_S3", C["i2c"]), 4: ("SCL_S3", C["i2c"]), 5: ("S3_EXP_INT", C["i2c"]), 8: ("S3_USB_DP", C["usb"]),
          9: ("S3_USB_DM", C["usb"]), 12: ("VSYS", C["pwr"])}
    rn = {3: ("UART_S3_TX", C["uart"]), 4: ("UART_C5_TX", C["uart"])}
    lp = ["GND", "3V3", "GP15 SDA", "GP7 SCL", "GP16", "GP4 BAT-ADC", "GP0 BOOT", "GP20 D+", "GP19 D−", "GND", "VCC",
          "5V*"]
    rp = ["GND", "3V3", "GP43 TX", "GP44 RX", "GP37 PSRAM", "GP36 PSRAM", "GP35 PSRAM", "GP34 PSRAM", "GP33 PSRAM",
          "GND", "BAT (frei)", "5V"]

    def st3(n, lab, used):
        return U if (n in used or lab == "GND") else "nc"

    s.module("S3", 1400, 1000, 300, "J3/J4 ESP32-S3-Touch-LCD-2.8B", "2× 12 Buchsenleiste",
             [(f"L{i + 1}", f"L{i + 1}", t, st3(i + 1, t, ln)) for i, t in enumerate(lp)],
             [(f"R{i + 1}", f"R{i + 1}", t, st3(i + 1, t, rn)) for i, t in enumerate(rp)], pitch=30, top=64)
    for i, t in enumerate(lp):
        if t == "GND":
            to_gnd(f"S3.L{i + 1}", 24)
    for i, t in enumerate(rp):
        if t == "GND":
            to_gnd(f"S3.R{i + 1}", 24)
    for n, (name, col) in ln.items():
        to_flag(f"S3.L{n}", name, col, 50)
    for n, (name, col) in rn.items():
        to_flag(f"S3.R{n}", name, col, 50)
    s.module("U16", 1500, 1450, 260, "U16 TCA9555", "I/O-Expander S3, I2C 0x27",
             [("VCC", "", "VCC", U), ("SDA", "", "SDA", U), ("SCL", "", "SCL", U), ("ADDR", "", "A0–A2", U),
              ("INT", "", "/INT", U), ("GND", "", "GND", U)],
             [("P00", "", "P00", U), ("P01", "", "P01", U), ("P02", "", "P02", U), ("P03", "", "P03", U),
              ("P04", "", "P04", U), ("P05", "", "P05", U), ("P06", "", "P06", U), ("P07", "", "P07", U),
              ("P10", "", "P10", U), ("P11", "", "P11", U)], pitch=26, top=60, nums=False)
    to_flag("U16.VCC", "+3V3", C["pwr"], 50); to_flag("U16.SDA", "SDA_S3", C["i2c"], 50)
    to_flag("U16.SCL", "SCL_S3", C["i2c"], 50); to_flag("U16.ADDR", "+3V3", C["pwr"], 50)
    to_flag("U16.INT", "S3_EXP_INT", C["i2c"], 50); to_gnd("U16.GND")
    for pin, name, col in (("U16.P00", "ENC1_A", C["enc"]), ("U16.P01", "ENC1_B", C["enc"]),
                           ("U16.P02", "ENC1_SW", C["enc"]), ("U16.P03", "ENC2_A", C["enc2"]),
                           ("U16.P04", "ENC2_B", C["enc2"]), ("U16.P05", "ENC2_SW", C["enc2"]),
                           ("U16.P06", "PWR_KEY", C["btn"]), ("U16.P07", "BQ_INT", C["i2c"]),
                           ("U16.P10", "BAT_ALRT", C["i2c"]), ("U16.P11", "DOCK_DET", C["btn"])):
        to_flag(pin, name, col, 20)
    s.note(1070, 1470, ["S3_EXP_INT: R60 10 k → +3V3, C45 100 nF an VCC.",
                        "5V*: Speisung aus VSYS – lt. Board-Schaltplan",
                        "prüfen. BAT nicht nutzen, falls das Board",
                        "einen eigenen Lader hat.",
                        "I2C-S3: U3 0x6A, U5 0x36, U16 0x27 + Touch",
                        "und Bausteine des Boards (Adressen prüfen)."], size=12)

    # ======================================================================= F · Codec
    s.block(2070, 960, 1560, 820, "F · Audio-Codec TLV320AIC3204 + I/O-Expander")
    s.module("U9", 2470, 1020, 280, "U9 TLV320AIC3204", "Codec 48 kHz, I2C 0x18",
             [("BCLK", "", "BCLK", U), ("WCLK", "", "WCLK", U), ("DIN", "", "DIN", U), ("DOUT", "", "DOUT", U),
              ("MCLK", "", "MCLK", U), ("SCL", "", "SCL", U), ("SDA", "", "SDA", U), ("RST", "", "RESET", U),
              ("SPI", "", "SPI_SELECT", U), ("IOVDD", "", "IOVDD", U), ("LDOIN", "", "LDOIN", U),
              ("LDOSEL", "", "LDO_SELECT", U), ("GND", "", "AVSS/DVSS/IOVSS", U)],
             [("IN1L", "", "IN1_L", U), ("IN1R", "", "IN1_R", "nc"), ("IN2L", "", "IN2_L", "nc"),
              ("IN2R", "", "IN2_R", "nc"), ("MICB", "", "MICBIAS", "nc"), ("REF", "", "REF", U),
              ("AVDD", "", "AVDD (1,8 V)", U), ("DVDD", "", "DVDD (1,8 V)", U), ("LOL", "", "LOL", U),
              ("LOR", "", "LOR", U), ("HPL", "", "HPL", "nc"), ("HPR", "", "HPR", "nc"), ("MFP", "", "MFP3/4/5", "nc")],
             pitch=48, top=70, nums=False)
    for pin, (name, col) in {"U9.BCLK": ("I2S_BCLK", C["i2s"]), "U9.WCLK": ("I2S_WS", C["i2s"]),
                             "U9.DIN": ("I2S_DOUT", C["i2sd"]), "U9.DOUT": ("I2S_DIN", C["i2sd"]),
                             "U9.SCL": ("SCL_A", C["i2c"]), "U9.SDA": ("SDA_A", C["i2c"]),
                             "U9.RST": ("CODEC_RST", C["i2c"]), "U9.IOVDD": ("+3V3", C["pwr"]),
                             "U9.LDOIN": ("+3V3", C["pwr"]), "U9.LDOSEL": ("+3V3", C["pwr"])}.items():
        to_flag(pin, name, col, 90)
    for pin in ("U9.MCLK", "U9.SPI", "U9.GND"):
        to_gnd(pin)
    small(p("U9.RST")[0] - 90, p("U9.RST")[1] + 18, "R16 10 k → +3V3", anchor="end")
    small(p("U9.IOVDD")[0] - 90, p("U9.IOVDD")[1] + 18, "C17 10 µF + 100 nF → GND", anchor="end")
    small(p("U9.LDOIN")[0] - 90, p("U9.LDOIN")[1] + 18, "C18 10 µF + 100 nF → GND", anchor="end")
    small(p("U9.LDOSEL")[0] - 90, p("U9.LDOSEL")[1] + 18, "interne LDOs an → AVDD/DVDD 1,8 V", anchor="end")
    to_text("U9.REF", "C19 10 µF → GND"); to_text("U9.AVDD", "C20 10 µF + 100 nF → GND")
    to_text("U9.DVDD", "C21 10 µF + 100 nF → GND")
    to_flag("U9.IN1L", "MIC_TO_CODEC", C["ana"], 50)
    to_flag("U9.LOL", "LINE_L", C["ana"], 50); to_flag("U9.LOR", "LINE_R", C["ana"], 50)

    s.module("U10", 3230, 1200, 240, "U10 TCA9555", "I/O-Expander C5, I2C 0x20",
             [("VCC", "", "VCC", U), ("SDA", "", "SDA", U), ("SCL", "", "SCL", U), ("ADDR", "", "A0–A2", U),
              ("GND", "", "GND", U)],
             [("P0", "", "P00", U), ("P1", "", "P01", U), ("P2", "", "P02", U), ("P3", "", "P03", U), ("P4", "", "P04", U),
              ("P5", "", "P05", U), ("P10", "", "P10", U), ("P11", "", "P11", U), ("P12", "", "P12", U),
              ("P13", "", "P13", U)], pitch=34, top=64, nums=False)
    to_flag("U10.VCC", "+3V3", C["pwr"], 70); small(p("U10.VCC")[0] - 70, p("U10.VCC")[1] + 18, "C22 100 nF", anchor="end")
    to_flag("U10.SDA", "SDA_A", C["i2c"], 70); to_flag("U10.SCL", "SCL_A", C["i2c"], 70)
    to_gnd("U10.ADDR"); to_gnd("U10.GND")
    for pin, name, col in (("U10.P0", "P48_SW", C["p48"]), ("U10.P1", "GAIN_HI", C["ana"]),
                           ("U10.P2", "HP_EN", C["ana"]), ("U10.P3", "CODEC_RST", C["i2c"]),
                           ("U10.P4", "ANA_EN", C["i2c"]), ("U10.P5", "P48_EN", C["p48"]),
                           ("U10.P10", "T1", C["btn"]), ("U10.P11", "T2", C["btn"]), ("U10.P12", "T3", C["btn"]),
                           ("U10.P13", "T4", C["btn"])):
        to_flag(pin, name, col, 20)

    # ======================================================================= G · Mikrofon
    s.block(30, 1800, 1940, 780, "G · Mikrofon-Eingang: Kombibuchse XLR / 6,35 mm, Phantom 48 V, Vorverstärker")
    s.module("J5", 90, 1880, 240, "J5 Neutrik Kombi", "NCJ6FI-H (XLR + 6,35 TRS)", [],
             [("X2", "", "XLR 2 (+)", U), ("X3", "", "XLR 3 (−)", U), ("X1", "", "XLR 1 (GND)", U),
              ("TT", "", "Klinke T (+)", U), ("TR", "", "Klinke R (−)", U), ("TS", "", "Klinke S", U)], pitch=50,
             top=70, nums=False)
    x2, y2 = p("J5.X2"); x3, y3 = p("J5.X3"); xt, yt = p("J5.TT"); xr, yr = p("J5.TR")
    s.line([(xt, yt), (xt + 30, yt), (xt + 30, y2)], C["ana"]); s.dot(xt + 30, y2, INK)
    s.line([(xr, yr), (xr + 55, yr), (xr + 55, y3)], C["ana"]); s.dot(xr + 55, y3, INK)
    to_gnd("J5.X1", 20); to_gnd("J5.TS", 20)
    # Leitungen zur Vorstufe mit Phantom, DC-Block, Schutz
    s.line([(x2, y2), (480, y2)], C["ana"]); s.line([(x3, y3), (560, y3)], C["ana"])
    s.dot(480, y2, INK); s.dot(560, y3, INK)
    e = s.resistor(480, y2 - 100, "R17", "6,81 k 0,1 %", vertical=True, color=C["p48"])
    s.line([e, (480, y2)], C["p48"])
    s.line([(480, y2 - 100), (480, y2 - 112), (520, y2 - 112)], C["p48"]); s.flag(520, y2 - 112, "P48", C["p48"])
    e = s.resistor(560, y3, "R18", "6,81 k 0,1 %", vertical=True, color=C["p48"])
    s.line([e, (560, e[1] + 12), (600, e[1] + 12)], C["p48"]); s.flag(600, e[1] + 12, "P48", C["p48"])
    s.line([(480, y2), (600, y2)], C["ana"]); s.line([(560, y3), (600, y3)], C["ana"])
    for (yy, rc, rr) in ((y2, "C23", "R19"), (y3, "C24", "R20")):
        e = s.cap(600, yy, rc, "47 µF/63 V", vertical=False, polar=True, color=C["ana"])
        s.line([e, (680, yy)], C["ana"])
        e = s.resistor(680, yy, rr, "10 Ω", color=C["ana"])
        s.line([e, (870, yy)], C["ana"])
        s.dot(780, yy, INK)
    s.module("U11", 900, 1880, 220, "U11 INA163", "Mic-Vorverstärker ±5 V",
             [("INP", "", "V+IN", U), ("INN", "", "V−IN", U), ("RG1", "", "RG", U), ("RG2", "", "RG", U)],
             [("VP", "", "V+", U), ("VN", "", "V−", U), ("REF", "", "REF", U), ("OUT", "", "OUT", U)], pitch=50,
             top=70, nums=False)
    xg, yg1 = p("U11.RG1"); _, yg2 = p("U11.RG2")
    s.line([(xg, yg1), (830, yg1)]); e = s.resistor(830, yg1, "RG1", "680 Ω", vertical=True)
    s.line([e, (830, e[1] + 10), (850, e[1] + 10), (850, yg2), (xg, yg2)])
    to_flag("U11.VP", "+5VA", C["pwr"], 70); to_flag("U11.VN", "−5VA", C["pwr"], 70); to_gnd("U11.REF", 20)
    x, y = p("U11.OUT"); s.line([(x, y), (x + 20, y)], C["ana"])
    e = s.resistor(x + 20, y, "R23", "4,7 k", color=C["ana"])
    s.line([e, (e[0] + 20, y)], C["ana"]); s.dot(e[0] + 20, y, INK)
    s.gnd(*s.resistor(e[0] + 20, y, "R24", "1,2 k", vertical=True))
    e3 = s.cap(e[0] + 20, y, "C29", "1 µF Folie", vertical=False, color=C["ana"])
    s.line([e3, (e3[0] + 20, y)], C["ana"]); s.flag(e3[0] + 20, y, "MIC_TO_CODEC", C["ana"])
    s.note(60, 2290, [
        "Schutz/Filter je Ader (am Knoten hinter R19/R20): BAV99 (D3/D4) nach +5VA / −5VA, R21/R22 10 k nach GND,",
        "C25 470 pF zwischen den Adern, C26/C27 100 pF nach GND. Entkopplung U11: C28 10 µF + 100 nF an V+ und V−.",
        "Verstärkung: RG2 68 Ω über Analogschalter U12 (z. B. ADG1201, ±5 V) parallel zu RG1. GAIN_HI aus = +20 dB, an = +40 dB.",
        "Klinke (T/R) liegt parallel zu XLR 2/3. Phantom per Software nur einschalten, wenn ein XLR-Kondensatormikro steckt.",
    ], size=13)
    s.note(1350, 1880, [
        "Pegelplan Mikrofon:",
        "Vorverstärker +20 / +40 dB",
        "→ Teiler R23/R24 −14 dB",
        "→ Codec-PGA 0 … +47,5 dB",
        "= gesamt ca. +6 … +73 dB",
        "",
        "BPHS1 (dynamisch): GAIN_HI an,",
        "PGA ca. +20…30 dB, Phantom AUS.",
        "Umschalten: Codec stumm → P48 →",
        "500 ms warten → Ton an.",
    ], size=13)

    # ======================================================================= H · Kopfhoerer
    s.block(1990, 1800, 780, 780, "H · Kopfhörer-Verstärker + 6,35-mm-Stereo")
    s.module("U13", 2230, 1880, 220, "U13 OPA1622", "Kopfhörer-Verstärker ±5 V",
             [("IN1", "", "IN1+", U), ("IN2", "", "IN2+", U), ("EN", "", "EN", U), ("VP", "", "V+", U),
              ("VN", "", "V−", U)],
             [("O1", "", "OUT1", U), ("O2", "", "OUT2", U), ("FB", "", "IN1−/IN2−", U)], pitch=60, top=70, nums=False)
    for pin, name, rc in (("U13.IN1", "LINE_L", "C30"), ("U13.IN2", "LINE_R", "C31")):
        x, y = p(pin); s.line([(x, y), (x - 20, y)], C["ana"])
        s.cap(x - 72, y, rc, "2,2 µF", vertical=False, color=C["ana"]); s.flag(x - 72, y, name, C["ana"], "L")
    to_flag("U13.EN", "HP_EN", C["ana"], 80); to_flag("U13.VP", "+5VA", C["pwr"], 80); to_flag("U13.VN", "−5VA", C["pwr"], 80)
    to_text("U13.FB", "siehe unten", INK, 16)
    s.module("J6", 2620, 1900, 120, "J6", "6,35 Stereo", [("T", "", "T (L)", U), ("R", "", "R (R)", U),
                                                       ("S", "", "S (GND)", U)], [], pitch=60, top=70, nums=False)
    for po, pj, ref in (("U13.O1", "J6.T", "R31"), ("U13.O2", "J6.R", "R32")):
        x, y = p(po); s.line([(x, y), (x + 10, y)], C["ana"])
        e = s.resistor(x + 10, y, ref, "10 Ω", color=C["ana"])
        s.line([e, (e[0] + 30, y), (e[0] + 30, p(pj)[1]), p(pj)], C["ana"])
    to_gnd("J6.S", 20)
    s.note(2020, 2240, ["R25/R26 47 k: IN1+/IN2+ nach GND (Ruhepunkt).",
                        "Verstärkung ×2: R27/R28 1 k OUT → IN−, R29/R30 1 k IN− → GND.",
                        "Entkopplung C32–C35: je 10 µF + 100 nF an V+ und V−.",
                        "HP_EN erst nach dem Hochfahren einschalten (kein Knacken).",
                        "Stereo: Mono-Mix oder Kanal A links / B rechts (Firmware)."], size=13)

    # ======================================================================= I · Bedienung
    s.block(2790, 1800, 840, 780, "I · Bedienung: 4 Tasten + LEDs, 2 Drehgeber")
    for i, net in enumerate(("T1", "T2", "T3", "T4")):
        y = 1900 + i * 80
        s.flag(2870, y, net, C["btn"], "L")
        e = s.resistor(2870, y, f"R{33 + i}", "1 k", color=C["btn"])
        s.line([e, (e[0] + 30, y)], C["btn"]); s.dot(e[0] + 30, y, INK)
        a, b = s.button(e[0] + 30, y, net, "above")
        s.line([b, (b[0] + 16, y), (b[0] + 16, y + 6)]); s.gnd(b[0] + 16, y + 6)
        small(b[0] + 50, y, f"C{36 + i} 100 nF vom Knoten nach GND")
    yl = 2250
    s.text(2810, yl - 24, "LED-Kette (je Taste eine RGB-LED)", 13, INK, weight="bold", family=SANS)
    s.flag(2870, yl + 20, "LED_DATA", C["led"], "L")
    e = s.resistor(2870, yl + 20, "R37", "33 Ω", color=C["led"]); xx = e[0]
    for i in range(4):
        s.line([(xx, yl + 20), (xx + 12, yl + 20)], C["led"])
        s.rect(xx + 12, yl, 90, 40, stroke=C["led"], fill="#ffffff", sw=2.5)
        s.text(xx + 57, yl + 13, f"LED{i + 1}", 11, C["led"], anchor="middle", weight="bold")
        s.text(xx + 57, yl + 29, "SK6812MINI", 10, INK, anchor="middle")
        xx += 102
    small(2810, yl + 64, "DIN → DOUT → DIN … · VDD aller LEDs = VSYS (3,5–4,2 V), je 100 nF (C40–C43)")
    ye = 2370
    s.text(2810, ye, "Drehgeber EC11 (direkt auf der Platine)", 13, INK, weight="bold", family=SANS)
    for i, nets in enumerate((("ENC1_A", "ENC1_B", "ENC1_SW"), ("ENC2_A", "ENC2_B", "ENC2_SW"))):
        yy = ye + 40 + i * 76
        col = C["enc"] if i == 0 else C["enc2"]
        for j, n in enumerate(nets):
            s.flag(2830 + j * 200, yy, n, col)
        small(2810, yy + 26, f"ENC{i + 1}: A, B, Taster je 10 k → +3V3, 10 nF → GND, an U16 (S3) · C-Pin/Taster-Pin 2 → GND")

    # ======================================================================= J · Andock-Kontakte
    s.block(30, 2600, 1940, 440, "J · Andock-Kontakte zur Basisstation (W5500 + RJ45 + 5 V-Netzteil in der Station)")
    s.module("J7", 90, 2660, 230, "J7 Kontaktflächen", "vergoldet, zu Federstiften", [],
             [("SCK", "", "SCK", U), ("MOSI", "", "MOSI", U), ("MISO", "", "MISO", U), ("CS", "", "CS", U),
              ("V5", "", "5V Station", U), ("DET", "", "DET (GND in Station)", U), ("GND", "", "GND", U)],
             pitch=46, top=60, nums=False)
    for pin, net in (("J7.SCK", "DOCK_SCK"), ("J7.MOSI", "DOCK_MOSI"), ("J7.MISO", "DOCK_MISO"), ("J7.CS", "DOCK_CS")):
        x, y = p(pin)
        s.line([(x, y), (x + 40, y)], C["btn"])
        e = s.resistor(x + 120, y, "R" + str(50 + ("SCK", "MOSI", "MISO", "CS").index(pin.split(".")[1])), "33 Ω",
                       color=C["btn"])
        s.line([(x + 40, y), (x + 120, y)], C["btn"]); s.line([e, (e[0] + 30, y)], C["btn"])
        s.flag(e[0] + 30, y, net, C["btn"])
    to_flag("J7.V5", "DOCK_5V", C["pwr"], 60); small(p("J7.V5")[0] + 10, p("J7.V5")[1] + 18, "C46 10 µF → GND")
    to_flag("J7.DET", "DOCK_DET", C["btn"], 60); to_gnd("J7.GND", 20)
    s.note(900, 2660, ["U14 TPD4E05U06 (ESD, 4-fach) direkt an J7 auf SCK/MOSI/MISO/CS,",
                       "dahinter R50–R53 33 Ω zum C5. DOCK_DET: Pull-up im TCA9555, Station zieht auf GND.",
                       "Station: W5500 (SPI → 100-Mbit-LAN) + RJ45 mit Übertrager + 5 V/3 A, Federstifte geführt.",
                       "Firmware: Station erkannt → LAN starten, Sitzung beim Gateway FORTSETZEN (kein Ab-/Anmelden),",
                       "WLAN optional aus. Herausgenommen → zurück auf WLAN, wieder fortsetzen."], size=13)

    s.legend(1350, 2420, [(C["pwr"], "Versorgung"), (C["i2c"], "I2C / Steuerung"), (C["i2s"], "I2S-Takte"),
                          (C["i2sd"], "I2S-Daten")], size=13)
    s.legend(1600, 2420, [(C["ana"], "Analog-Audio"), (C["p48"], "Phantom 48 V"), (C["usb"], "USB"),
                          (C["uart"], "UART C5 ↔ S3")], size=13)
    return s.svg()


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(out, exist_ok=True)
    for name, fn in (("schaltplan-testaufbau.svg", testaufbau), ("schaltplan-platine-v1.svg", platine)):
        path = os.path.join(out, name)
        with open(path, "w", encoding="utf-8") as f:
            f.write(fn())
        print("geschrieben:", path)
