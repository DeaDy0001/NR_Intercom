#!/usr/bin/env python3
"""Zeichnet die Verdrahtungsplaene (jeder Pin mit eigener Linie zum Gegenstueck).

Aufruf: python tools/wiring_diagram.py docs/img
Erzeugt:  verkabelung-c5-wm8960.png   ESP32-C5-Zero <-> WM8960 Audio Board <-> 4 Tasten
          verkabelung-s3-drehgeber.png ESP32-S3 AMOLED <-> 2 Drehgeber KY-040
Pins und Belegung entsprechen den Standardwerten in firmware/*/main/Kconfig.projbuild.
"""
import os
import sys
from PIL import Image, ImageDraw, ImageFont

FD = "/usr/share/fonts/truetype/dejavu/"
F = {k: ImageFont.truetype(FD + f, s) for k, (f, s) in {
    "title": ("DejaVuSans-Bold.ttf", 30), "box": ("DejaVuSans-Bold.ttf", 22), "sub": ("DejaVuSans.ttf", 16),
    "pin": ("DejaVuSansMono-Bold.ttf", 16), "num": ("DejaVuSansMono.ttf", 14), "net": ("DejaVuSans-Bold.ttf", 15),
    "note": ("DejaVuSans.ttf", 17), "noteb": ("DejaVuSans-Bold.ttf", 17)}.items()}

BOX = (150, 0, 0)
GRID = (238, 238, 232)
COL = {"pwr": (210, 0, 0), "gnd": (20, 20, 20), "i2c": (120, 60, 200), "i2s": (0, 110, 220),
       "i2s2": (230, 120, 0), "btn": (0, 140, 60), "enc": (0, 140, 60), "enc2": (0, 150, 160)}
STUB = 30


class Sheet:
    def __init__(self, w, h, title, subtitle):
        self.im = Image.new("RGB", (w, h), "white")
        self.d = ImageDraw.Draw(self.im)
        for x in range(0, w, 20):
            self.d.line([(x, 0), (x, h)], fill=GRID)
        for y in range(0, h, 20):
            self.d.line([(0, y), (w, y)], fill=GRID)
        self.d.text((30, 20), title, font=F["title"], fill=(0, 0, 0))
        self.d.text((30, 60), subtitle, font=F["sub"], fill=(90, 90, 90))
        self.pins = {}

    def module(self, key, name, sub, x, y, w, left=(), right=(), pitch=50, top=60, colors=None):
        """left/right: Listen von (pin_id, nummer, beschriftung, status) status: used/nc/free/bad"""
        n = max(len(left), len(right))
        h = top + (n - 1) * pitch + 40
        d = self.d
        d.rectangle([x, y, x + w, y + h], outline=BOX, width=3, fill=(255, 253, 240))
        d.text((x + w / 2, y + 10), name, font=F["box"], fill=BOX, anchor="mt")
        if sub:
            d.text((x + w / 2, y + 36), sub, font=F["sub"], fill=(110, 60, 60), anchor="mt")
        for side, lst in (("L", left), ("R", right)):
            for i, (pid, num, label, st) in enumerate(lst):
                py = y + top + i * pitch
                if st == "gap":
                    continue
                tc = {"used": (0, 0, 0), "nc": (150, 150, 150), "free": (150, 150, 150), "bad": (200, 0, 0)}[st]
                if side == "L":
                    d.line([(x - STUB, py), (x, py)], fill=BOX, width=3)
                    d.text((x + 8, py), label, font=F["pin"], fill=tc, anchor="lm")
                    d.text((x - 4, py - 4), str(num), font=F["num"], fill=(90, 90, 90), anchor="rb")
                    self.pins[f"{key}.{pid}"] = (x - STUB, py)
                    end = (x - STUB, py)
                else:
                    d.line([(x + w, py), (x + w + STUB, py)], fill=BOX, width=3)
                    d.text((x + w - 8, py), label, font=F["pin"], fill=tc, anchor="rm")
                    d.text((x + w + 4, py - 4), str(num), font=F["num"], fill=(90, 90, 90), anchor="lb")
                    self.pins[f"{key}.{pid}"] = (x + w + STUB, py)
                    end = (x + w + STUB, py)
                if st == "nc":
                    self.cross(*end)
        return h

    def cross(self, x, y, s=9):
        self.d.line([(x - s, y - s), (x + s, y + s)], fill=(0, 170, 0), width=3)
        self.d.line([(x - s, y + s), (x + s, y - s)], fill=(0, 170, 0), width=3)

    def dot(self, x, y):
        self.d.ellipse([x - 7, y - 7, x + 7, y + 7], fill=(200, 0, 0))

    def wire(self, a, b, via, color, label=None, label_at=None):
        pa = self.pins[a] if isinstance(a, str) else a
        pb = self.pins[b] if isinstance(b, str) else b
        pts = [pa] + list(via) + [pb]
        self.d.line(pts, fill=color, width=4, joint="curve")
        if label:
            lx, ly = label_at if label_at else ((pts[0][0] + pts[1][0]) / 2, pts[0][1])
            bb = self.d.textbbox((lx, ly - 3), label, font=F["net"], anchor="mb")
            self.d.rectangle([bb[0] - 3, bb[1] - 1, bb[2] + 3, bb[3] + 1], fill="white")
            self.d.text((lx, ly - 3), label, font=F["net"], fill=color, anchor="mb")

    def button(self, name, x, y, label="below"):
        """Taster: Anschluss a links/oben, b rechts/unten. Rueckgabe (a, b)"""
        d = self.d
        if True:
            a, b = (x, y), (x + 120, y)
            d.line([a, (x + 35, y)], fill=BOX, width=3)
            d.line([(x + 85, y), b], fill=BOX, width=3)
            d.ellipse([x + 31, y - 4, x + 39, y + 4], outline=BOX, width=2)
            d.ellipse([x + 81, y - 4, x + 89, y + 4], outline=BOX, width=2)
            d.line([(x + 30, y - 16), (x + 90, y - 16)], fill=BOX, width=3)
            d.line([(x + 60, y - 16), (x + 60, y - 30)], fill=BOX, width=3)
            if label == "right":
                d.text((x + 100, y - 14), name, font=F["box"], fill=(30, 30, 160), anchor="lb")
            elif label == "left":
                d.text((x + 22, y - 6), name, font=F["box"], fill=(30, 30, 160), anchor="rb")
            else:
                d.text((x + 60, y + 10), name, font=F["box"], fill=(30, 30, 160), anchor="mt")
        return a, b

    def note(self, x, y, lines, title=None):
        d = self.d
        if title:
            d.text((x, y), title, font=F["noteb"], fill=(0, 0, 0))
            y += 26
        for ln in lines:
            d.text((x, y), ln, font=F["note"], fill=(40, 40, 40))
            y += 24
        return y

    def legend(self, x, y, items):
        for color, text in items:
            self.d.line([(x, y + 10), (x + 50, y + 10)], fill=color, width=5)
            self.d.text((x + 62, y + 10), text, font=F["note"], fill=(40, 40, 40), anchor="lm")
            y += 28

    def save(self, path):
        self.im.save(path, optimize=True)
        print("geschrieben:", path)


def c5_wm8960(path):
    s = Sheet(2000, 1180, "Verkabelung Testaufbau: ESP32-C5-Zero  +  WM8960 Audio Board  +  4 Tasten",
              "Jede Linie = ein Draht. Pin-Nummern wie auf den Platinen. Firmware: firmware/bodypack-c5 (Standard-Pins).")
    U = "used"
    c5_left = [("5V", "", "5V", "free"), ("GND", "", "GND", U), ("3V3", "", "3V3", U),
               ("GP0", "", "GP0", U), ("GP1", "", "GP1", U), ("GP2", "", "GP2", "free"),
               ("GP3", "", "GP3", "free"), ("GP4", "", "GP4", U), ("GP5", "", "GP5", U)]
    c5_right = [("GP11", "", "GP11", U), ("GP12", "", "GP12", U), ("GP13", "", "GP13 USB", "free"),
                ("GP14", "", "GP14 USB", "free"), ("GP10", "", "GP10", U), ("GP9", "", "GP9", U),
                ("GP8", "", "GP8", U), ("GP7", "", "GP7", "free"), ("GP6", "", "GP6", U)]
    s.module("C5", "ESP32-C5-Zero", "Draufsicht, USB oben", 900, 300, 220, c5_left, c5_right, pitch=50, top=60)
    wm = [(1, "VCC 3V3", U), (2, "GND", U), (3, "SDA", U), (4, "SCL", U), (5, "CLK", U), (6, "WS", U),
          (7, "RXSDA", U), (8, "RXMCLK", "nc"), (9, "VCC 3V3", "free"), (10, "GND", "free"), (11, "NC", "nc"),
          (12, "NC", "nc"), (13, "CLK", "free"), (14, "WS", "free"), (15, "TXSDA", U), (16, "TXMCLK", "nc")]
    s.module("WM", "WM8960", "Audio Board", 1600, 140, 260,
             [(str(n), n, lab, st) for n, lab, st in wm], [], pitch=40, top=60)

    p = s.pins
    # Versorgung
    s.wire("C5.3V3", "WM.1", [(830, p["C5.3V3"][1]), (830, 180), (1540, 180), (1540, p["WM.1"][1])],
           COL["pwr"], "3V3", (1000, 180))
    s.wire("C5.GND", "WM.2", [(850, p["C5.GND"][1]), (850, 215), (1520, 215), (1520, p["WM.2"][1])],
           COL["gnd"], "GND", (1000, 215))
    s.dot(850, p["C5.GND"][1])
    # I2C
    s.wire("C5.GP10", "WM.4", [(1300, p["C5.GP10"][1]), (1300, p["WM.4"][1])], COL["i2c"], "SCL",
           (1440, p["WM.4"][1]))
    s.wire("C5.GP9", "WM.3", [(1330, p["C5.GP9"][1]), (1330, p["WM.3"][1])], COL["i2c"], "SDA",
           (1440, p["WM.3"][1]))
    # I2S
    s.wire("C5.GP8", "WM.7", [(1360, p["C5.GP8"][1]), (1360, p["WM.7"][1])], COL["i2s2"], "DOUT → RXSDA",
           (1255, p["C5.GP8"][1]))
    s.wire("C5.GP6", "WM.15", [], COL["i2s2"], "TXSDA → DIN", (1300, p["C5.GP6"][1]))
    s.wire("C5.GP4", "WM.5", [(790, p["C5.GP4"][1]), (790, 970), (1490, 970), (1490, p["WM.5"][1])],
           COL["i2s"], "BCLK (vom WM8960)", (1150, 970))
    s.wire("C5.GP5", "WM.6", [(810, p["C5.GP5"][1]), (810, 940), (1460, 940), (1460, p["WM.6"][1])],
           COL["i2s"], "WS / LRCLK (vom WM8960)", (1150, 940))

    # Tasten links (GP0, GP1)
    gnd_x = 430
    for name, pin in (("T1", "C5.GP0"), ("T2", "C5.GP1")):
        y = p[pin][1]
        a, b = s.button(name, 560, y, label="left")
        s.wire(b, pin, [], COL["btn"], pin.split(".")[1], (780, y))
        s.wire(a, (gnd_x, y), [], COL["gnd"])
        s.dot(gnd_x, y)
    s.wire(p["C5.GND"], (gnd_x, p["C5.GP1"][1]), [(gnd_x, p["C5.GND"][1])], COL["gnd"], "GND", (600, p["C5.GND"][1]))
    s.dot(gnd_x, p["C5.GP0"][1])

    # Tasten oben (GP11, GP12)
    for name, pin, lane, by in (("T4", "C5.GP11", 1180, 95), ("T3", "C5.GP12", 1210, 145)):
        a, b = s.button(name, 1270, by, label="right")
        s.wire(pin, a, [(lane, p[pin][1]), (lane, by)], COL["btn"], pin.split(".")[1], (lane + 40, p[pin][1]))
    s.wire((1390, 95), (1420, 215), [(1420, 95)], COL["gnd"])
    s.wire((1390, 145), (1420, 145), [], COL["gnd"])
    s.dot(1420, 145)
    s.dot(1420, 215)

    # Hinweise
    s.note(60, 760, [
        "• CLK (5/13) und WS (6/14) sind auf dem WM8960-Board doppelt.",
        "  Je ein Draht genügt. Pin 9/10 = Pin 1/2 (intern).",
        "• RXMCLK / TXMCLK NICHT anschließen: der WM8960 hat einen",
        "  eigenen 24-MHz-Takt und ist I2S-Master.",
        "• Tasten schalten nach GND (Pull-ups sind im ESP32 aktiv).",
        "• Externe Antenne an die IPEX-Buchse (Umschalter GP26 intern).",
        "• GP13/GP14 = USB, frei lassen (Flashen + Konsole).",
    ], "Hinweise")
    s.legend(60, 1010, [(COL["pwr"], "3,3 V"), (COL["gnd"], "GND"), (COL["i2c"], "I2C (Steuerung WM8960)"),
                        (COL["i2s"], "I2S-Takte (WM8960 → ESP)"), (COL["i2s2"], "I2S-Daten"),
                        (COL["btn"], "Tasten")])
    s.cross(1880, 1040)
    s.d.text((1860, 1070), "= nicht anschließen", font=F["note"], fill=(40, 40, 40), anchor="rm")
    s.dot(1880, 1110)
    s.d.text((1860, 1110), "= Verbindung", font=F["note"], fill=(40, 40, 40), anchor="rm")
    s.save(path)


def s3_encoders(path):
    s = Sheet(2000, 1180, "Verkabelung Testaufbau: ESP32-S3 AMOLED 1,91″  +  2 Drehgeber KY-040",
              "Jede Linie = ein Draht. H1/H2 = Stiftleisten des Boards. Firmware: firmware/display-s3 (Standard-Pins).")
    h1 = ["GP43", "GP44", "GND", "GP0 BOOT", "GP1 BAT", "GP2", "GP3", "GND", "GP11", "GP12", "GP13", "GP14",
          "GND", "GP15", "GP19 USB", "GP20 USB", "GP21", "GND", "GP26 PSRAM", "GP33 PSRAM"]
    h2 = ["VBUS", "VSYS", "GND", "3V3_EN", "3V3", "GP17 RESET", "GP16", "GND", "GP10", "GP4", "CHIP_UP",
          "GP40 SDA", "GND", "GP39 SCL", "GP38", "GP37 PSRAM", "GP36 PSRAM", "GND", "GP35 PSRAM", "GP34 PSRAM"]
    used1 = {9, 10, 11, 12, 13, 14}
    used2 = {5, 7, 8}

    def st(lab, num, used):
        if num in used:
            return "used"
        if any(k in lab for k in ("PSRAM", "RESET", "BOOT", "BAT", "USB")):
            return "bad"
        return "free"

    left = [(f"H1_{i+1}", i + 1, lab, st(lab, i + 1, used1)) for i, lab in enumerate(h1)]
    right = [(f"H2_{i+1}", i + 1, lab, st(lab, i + 1, used2)) for i, lab in enumerate(h2)]
    s.module("S3", "ESP32-S3 AMOLED", "Waveshare 1,91″ · Leisten H1 / H2", 880, 140, 360,
             left, right, pitch=32, top=70)
    p = s.pins
    enc = lambda key, x, y, side: s.module(key, "Drehgeber", "KY-040", x, y, 180,
                                           [] if side == "R" else ENC, ENC if side == "R" else [], pitch=32, top=70)
    ENC = [("CLK", "", "CLK", "used"), ("DT", "", "DT", "used"), ("SW", "", "SW", "used"),
           ("GND", "", "GND", "used"), ("+", "", "+", "used")]

    # Drehgeber 1 links: Pins auf Hoehe H1-9 .. H1-13 (GND auf H1-13)
    y1 = p["S3.H1_9"][1] - 70
    s.module("E1", "Drehgeber 1", "KY-040 (links)", 340, y1, 220, [],
             [("CLK", "", "CLK", "used"), ("DT", "", "DT", "used"), ("SW", "", "SW", "used"),
              ("X", "", "", "gap"), ("GND", "", "GND", "used"), ("+", "", "+", "used")], pitch=32, top=70)
    s.wire("E1.CLK", "S3.H1_9", [], COL["enc"], "GP11", (700, p["S3.H1_9"][1]))
    s.wire("E1.DT", "S3.H1_10", [], COL["enc"], "GP12", (700, p["S3.H1_10"][1]))
    s.wire("E1.SW", "S3.H1_11", [], COL["enc"], "GP13", (700, p["S3.H1_11"][1]))
    s.wire("E1.GND", "S3.H1_13", [], COL["gnd"], "GND", (700, p["S3.H1_13"][1]))
    # + von Drehgeber 1 unten herum zu 3V3 (H2-5)
    e1p = p["E1.+"]
    s.wire("E1.+", "S3.H2_5", [(620, e1p[1]), (620, 1030), (1330, 1030), (1330, p["S3.H2_5"][1])],
           COL["pwr"], "3V3", (1000, 1030))
    s.dot(1330, p["S3.H2_5"][1])

    # Drehgeber 2 rechts
    s.module("E2", "Drehgeber 2", "KY-040 (rechts)", 1520, p["S3.H2_5"][1] - 70, 220,
             [("+", "", "+", "used"), ("X1", "", "", "gap"), ("DT", "", "DT", "used"), ("GND", "", "GND", "used"),
              ("CLK", "", "CLK", "used"), ("SW", "", "SW", "used")], [], pitch=32, top=70)
    s.wire("S3.H2_5", "E2.+", [], COL["pwr"], "3V3", (1420, p["S3.H2_5"][1]))
    s.wire("S3.H2_7", "E2.DT", [], COL["enc2"], "GP16", (1420, p["S3.H2_7"][1]))
    s.wire("S3.H2_8", "E2.GND", [], COL["gnd"], "GND", (1420, p["S3.H2_8"][1]))
    # CLK (H1-12) und SW (H1-14) unten herum nach rechts
    clk, sw = p["S3.H1_12"], p["S3.H1_14"]
    s.wire("S3.H1_12", "E2.CLK", [(820, clk[1]), (820, 1000), (1440, 1000), (1440, p["E2.CLK"][1])],
           COL["enc2"], "GP14 → CLK", (1150, 1000))
    s.wire("S3.H1_14", "E2.SW", [(840, sw[1]), (840, 970), (1470, 970), (1470, p["E2.SW"][1])],
           COL["enc2"], "GP15 → SW", (1150, 970))

    s.note(60, 760, [
        "• Rot beschriftete Pins frei lassen: PSRAM (GP26, GP33–37),",
        "  Display-Reset (GP17), BOOT (GP0), Akku (GP1), USB (GP19/20).",
        "• KY-040 an 3,3 V betreiben, nicht an 5 V.",
        "• Zählt ein Drehgeber falsch herum: menuconfig → Drehrichtung.",
        "• Verbindung zum ESP32-C5 (UART) folgt in Phase 2.",
    ], "Hinweise")
    s.legend(60, 1010, [(COL["pwr"], "3,3 V"), (COL["gnd"], "GND"), (COL["enc"], "Drehgeber 1"),
                        (COL["enc2"], "Drehgeber 2")])
    s.save(path)


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(out, exist_ok=True)
    c5_wm8960(os.path.join(out, "verkabelung-c5-wm8960.png"))
    s3_encoders(os.path.join(out, "verkabelung-s3-drehgeber.png"))
