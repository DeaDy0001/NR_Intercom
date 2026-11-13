#!/usr/bin/env python3
"""Variante B als "Linien-Schaltplan": fast alle Verbindungen als direkte Leitungen.

Bauteile werden von Hand platziert, die Leitungen verlegt ein einfacher Router (A* auf einem 10-px-Raster,
rechtwinklig, Kreuzungen erlaubt, keine Ueberlappungen). Versorgungen laufen als Sammelschienen,
GND als Masse-Symbol. Nur wenn der Router keinen Weg findet, entsteht ein Verweis (Netzname-Faehnchen).

Aufruf: python tools/schematic_routed.py docs/img
Erzeugt: docs/img/schaltplan-platine-v1-linien.svg
"""
import heapq
import os
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from schematics import Svg, C, INK, BOX, SANS, MONO  # noqa: E402

G = 10  # Rasterweite in px

NET_COLOR = {}


def col_for(net):
    if net in NET_COLOR:
        return NET_COLOR[net]
    n = net.upper()
    if n in ("VBUS", "VBUS_CHG", "DOCK_5V", "VSYS", "+3V3", "+5VA", "−5VA", "BAT+", "REGN") or n.startswith("PWR"):
        return C["pwr"]
    if n.startswith("P48"):
        return C["p48"]
    if "USB" in n:
        return C["usb"]
    if "SDA" in n or "SCL" in n or n in ("BQ_INT", "BAT_ALRT", "CODEC_RST", "ANA_EN", "P48_EN", "P48_SW",
                                         "GAIN_SET", "GAIN_RST", "HP_EN"):
        return C["i2c"]
    if "BCLK" in n or "WS" in n:
        return C["i2s"]
    if "I2S" in n:
        return C["i2sd"]
    if "UART" in n:
        return C["uart"]
    if n.startswith("ENC1"):
        return C["enc"]
    if n.startswith("ENC2"):
        return C["enc2"]
    if n.startswith("T") and len(n) <= 4 or n.startswith("BTN"):
        return C["btn"]
    if "LED" in n:
        return C["led"]
    return C["ana"] if n.startswith(("MIC", "LINE", "HP", "RG", "OUT", "FB", "IN")) else INK


class Board:
    def __init__(self, w, h, title, subtitle):
        self.s = Svg(w, h, title, subtitle)
        self.gw, self.gh = w // G + 1, h // G + 1
        self.blocked = [bytearray(self.gw) for _ in range(self.gh)]
        self.H = [[None] * self.gw for _ in range(self.gh)]
        self.V = [[None] * self.gw for _ in range(self.gh)]
        self.node = [[None] * self.gw for _ in range(self.gh)]
        self.pins = {}          # name -> (x, y, outward dir)
        self.pin_net = {}
        self.nets = defaultdict(list)
        self.net_cells = defaultdict(set)
        self.net_segs = defaultdict(list)
        self.edges = defaultdict(set)
        self.paths = defaultdict(list)
        self.rails = {}
        self.failed = []

    # ---------------------------------------------------------- Hindernisse / Pins
    def obstacle(self, x0, y0, x1, y1, margin=G):
        gx0, gy0 = max(0, (x0 - margin) // G), max(0, (y0 - margin) // G)
        gx1, gy1 = min(self.gw - 1, (x1 + margin) // G), min(self.gh - 1, (y1 + margin) // G)
        for gy in range(gy0, gy1 + 1):
            row = self.blocked[gy]
            for gx in range(gx0, gx1 + 1):
                row[gx] = 1

    def pin(self, name, x, y, d):
        assert x % G == 0 and y % G == 0, (name, x, y)
        self.pins[name] = (x, y, d)
        gx, gy = x // G, y // G
        self.blocked[gy][gx] = 0

    def module(self, key, x, y, w, title, sub, left=(), right=(), pitch=40, top=70, nums=False, fill=None):
        kw = {"fill": fill} if fill else {}
        h = self.s.module(key, x, y, w, title, sub, left, right, pitch=pitch, top=top, nums=nums, **kw)
        self.obstacle(x, y, x + w, y + h, margin=G)
        for side, lst in (("L", left), ("R", right)):
            for i, (pid, num, label, st) in enumerate(lst):
                if st == "gap":
                    continue
                px, py, _ = self.s.pins[f"{key}.{pid}"]
                gy = py // G
                for k in (1, 2):    # Pin-Stummel sperren
                    gx = px // G + (k if side == "L" else -k)
                    self.blocked[gy][gx] = 1
                if st == "nc":
                    self.blocked[gy][px // G] = 1
                    continue
                self.pin(f"{key}.{pid}", px, py, "L" if side == "L" else "R")
        return h

    # ---------------------------------------------------------- Symbole
    def R(self, ref, val, x, y, vert=False, color=INK):
        s = self.s
        if not vert:
            s.line([(x, y), (x + 18, y)], color)
            s.rect(x + 18, y - 7, 24, 14, stroke=color, fill="#ffffff", sw=2.5)
            s.line([(x + 42, y), (x + 60, y)], color)
            s.text(x + 30, y - 15, ref, 11, "#444", anchor="middle")
            s.text(x + 30, y + 17, val, 11, INK, anchor="middle", weight="bold")
            self.obstacle(x + 10, y - 22, x + 50, y + 24, margin=0)
            self.pin(f"{ref}.1", x, y, "L"); self.pin(f"{ref}.2", x + 60, y, "R")
        else:
            s.line([(x, y), (x, y + 18)], color)
            s.rect(x - 7, y + 18, 14, 24, stroke=color, fill="#ffffff", sw=2.5)
            s.line([(x, y + 42), (x, y + 60)], color)
            s.text(x + 11, y + 26, ref, 11, "#444")
            s.text(x + 11, y + 40, val, 11, INK, weight="bold")
            self.obstacle(x - 10, y + 10, x + 10 + 7 * max(len(ref), len(val)), y + 50, margin=0)
            self.pin(f"{ref}.1", x, y, "U"); self.pin(f"{ref}.2", x, y + 60, "D")

    def Cap(self, ref, val, x, y, vert=True, polar=False, color=INK):
        s = self.s
        if vert:
            s.line([(x, y), (x, y + 26)], color)
            s.line([(x - 12, y + 26), (x + 12, y + 26)], color, 3)
            s.line([(x - 12, y + 34), (x + 12, y + 34)], color, 3)
            s.line([(x, y + 34), (x, y + 60)], color)
            if polar:
                s.text(x - 16, y + 20, "+", 12, color)
            s.text(x + 15, y + 24, ref, 11, "#444")
            s.text(x + 15, y + 38, val, 11, INK, weight="bold")
            self.obstacle(x - 14, y + 14, x + 15 + 7 * max(len(ref), len(val)), y + 46, margin=0)
            self.pin(f"{ref}.1", x, y, "U"); self.pin(f"{ref}.2", x, y + 60, "D")
        else:
            s.line([(x, y), (x + 26, y)], color)
            s.line([(x + 26, y - 12), (x + 26, y + 12)], color, 3)
            s.line([(x + 34, y - 12), (x + 34, y + 12)], color, 3)
            s.line([(x + 34, y), (x + 60, y)], color)
            if polar:
                s.text(x + 16, y - 16, "+", 12, color)
            s.text(x + 30, y - 18, ref, 11, "#444", anchor="middle")
            s.text(x + 30, y + 22, val, 11, INK, anchor="middle", weight="bold")
            self.obstacle(x + 14, y - 26, x + 46, y + 28, margin=0)
            self.pin(f"{ref}.1", x, y, "L"); self.pin(f"{ref}.2", x + 60, y, "R")

    def L(self, ref, val, x, y):
        s = self.s
        s.line([(x, y), (x + 10, y)])
        d = f"M{x + 10} {y}" + " a 10 10 0 0 1 20 0" * 4
        s.el.append(f'<path d="{d}" fill="none" stroke="{INK}" stroke-width="3"/>')
        s.line([(x + 90, y), (x + 100, y)])
        s.text(x + 50, y - 18, ref, 11, "#444", anchor="middle")
        s.text(x + 50, y + 16, val, 11, INK, anchor="middle", weight="bold")
        self.obstacle(x + 10, y - 26, x + 90, y + 22, margin=0)
        self.pin(f"{ref}.1", x, y, "L"); self.pin(f"{ref}.2", x + 100, y, "R")

    def D(self, ref, val, x, y, vert=False, led=False, color=INK):
        """Anode = .A (Start), Kathode = .K (Ende)"""
        s = self.s
        if not vert:
            s.line([(x, y), (x + 20, y)], color)
            s.el.append(f'<polygon points="{x + 20},{y - 10} {x + 20},{y + 10} {x + 38},{y}" fill="#ffffff" '
                        f'stroke="{color}" stroke-width="2.5"/>')
            s.line([(x + 38, y - 10), (x + 38, y + 10)], color, 3)
            s.line([(x + 38, y), (x + 60, y)], color)
            if led:
                s.line([(x + 30, y - 14), (x + 38, y - 22)], color, 2); s.line([(x + 38, y - 14), (x + 46, y - 22)], color, 2)
            s.text(x + 30, y - 24 - (8 if led else 0), ref, 11, "#444", anchor="middle")
            s.text(x + 30, y + 22, val, 11, INK, anchor="middle", weight="bold")
            self.obstacle(x + 14, y - 34, x + 46, y + 26, margin=0)
            self.pin(f"{ref}.A", x, y, "L"); self.pin(f"{ref}.K", x + 60, y, "R")
        else:
            s.line([(x, y), (x, y + 20)], color)
            s.el.append(f'<polygon points="{x - 10},{y + 20} {x + 10},{y + 20} {x},{y + 38}" fill="#ffffff" '
                        f'stroke="{color}" stroke-width="2.5"/>')
            s.line([(x - 10, y + 38), (x + 10, y + 38)], color, 3)
            s.line([(x, y + 38), (x, y + 60)], color)
            if led:
                s.line([(x + 13, y + 22), (x + 21, y + 14)], color, 2); s.line([(x + 13, y + 30), (x + 21, y + 22)], color, 2)
            s.text(x + 16, y + 34, ref, 11, "#444")
            s.text(x + 16, y + 48, val, 11, INK, weight="bold")
            self.obstacle(x - 12, y + 12, x + 16 + 7 * max(len(ref), len(val)), y + 52, margin=0)
            self.pin(f"{ref}.A", x, y, "U"); self.pin(f"{ref}.K", x, y + 60, "D")

    def SW(self, ref, x, y, label=None):
        s = self.s
        s.line([(x, y), (x + 36, y)], BOX, 3)
        s.line([(x + 84, y), (x + 120, y)], BOX, 3)
        for cx in (x + 40, x + 80):
            s.el.append(f'<circle cx="{cx}" cy="{y}" r="4" fill="#ffffff" stroke="{BOX}" stroke-width="2"/>')
        s.line([(x + 32, y - 14), (x + 88, y - 14)], BOX, 3)
        s.line([(x + 60, y - 14), (x + 60, y - 26)], BOX, 3)
        s.text(x + 60, y - 36, label or ref, 14, "#1f2f9f", anchor="middle", weight="bold", family=SANS)
        self.obstacle(x + 26, y - 46, x + 94, y + 8, margin=0)
        self.pin(f"{ref}.1", x, y, "L"); self.pin(f"{ref}.2", x + 120, y, "R")

    def gnd(self, x, y):
        s = self.s
        s.line([(x - 14, y), (x + 14, y)], INK, 3)
        s.line([(x - 9, y + 6), (x + 9, y + 6)], INK, 3)
        s.line([(x - 4, y + 12), (x + 4, y + 12)], INK, 3)
        self.obstacle(x - 14, y, x + 14, y + 12, margin=0)

    def gnd_pin(self, pin, length=20):
        """Pin direkt auf Masse (kurzer Strich + Masse-Symbol)"""
        x, y, d = self.pins[pin]
        if d in ("L", "R"):
            dx = -length if d == "L" else length
            self.s.line([(x, y), (x + dx, y), (x + dx, y + 10)])
            self.gnd(x + dx, y + 10)
            self.obstacle(min(x, x + dx), y - 2, max(x, x + dx), y + 10, margin=0)
        else:
            dy = length if d == "D" else -length
            self.s.line([(x, y), (x, y + dy)])
            self.gnd(x, y + dy) if dy > 0 else self.gnd(x, y + 10)
        self.node[y // G][x // G] = "GND"

    def text(self, *a, **k):
        self.s.text(*a, **k)

    def note(self, x, y, lines, title=None, size=13):
        return self.s.note(x, y, lines, title, size=size)

    def block(self, x, y, w, h, title):
        self.s.text(x, y, title, 16, "#666", weight="bold", family=SANS)

    # ---------------------------------------------------------- Netze
    def net(self, name, *pins):
        for p in pins:
            assert p in self.pins, (name, p)
            self.pin_net[p] = name
            self.nets[name].append(p)
            x, y, _ = self.pins[p]
            self.node[y // G][x // G] = name

    def rail(self, name, x0, x1, y, label=True):
        self.s.line([(x0, y), (x1, y)], col_for(name), 3)
        gy = y // G
        for gx in range(x0 // G, x1 // G + 1):
            self.H[gy][gx] = name
            self.net_cells[name].add((gx, gy))
            if gx > x0 // G:
                self.edges[name].add(((gx - 1, gy), (gx, gy)))
        self.net_segs[name].append((x0 // G, gy, x1 // G, gy))
        if label:
            self.s.text(x0 + 4, y - 9, name, 13, col_for(name), weight="bold", family=MONO)
            self.s.text(x1 - 4, y - 9, name, 13, col_for(name), anchor="end", weight="bold", family=MONO)

    def vrail(self, name, x, y0, y1):
        self.s.line([(x, y0), (x, y1)], col_for(name), 3)
        gx = x // G
        for gy in range(y0 // G, y1 // G + 1):
            self.V[gy][gx] = name
            self.net_cells[name].add((gx, gy))
            if gy > y0 // G:
                self.edges[name].add(((gx, gy - 1), (gx, gy)))
        self.net_segs[name].append((gx, y0 // G, gx, y1 // G))
        for yy in (y0, y1):
            self.node[yy // G][gx] = name

    # ---------------------------------------------------------- Router
    DIRS = ((1, 0), (-1, 0), (0, 1), (0, -1))

    def _h(self, net, gx, gy):
        best = 10 ** 9
        for (x0, y0, x1, y1) in self.net_segs[net]:
            dx = 0 if min(x0, x1) <= gx <= max(x0, x1) else min(abs(gx - x0), abs(gx - x1))
            dy = 0 if min(y0, y1) <= gy <= max(y0, y1) else min(abs(gy - y0), abs(gy - y1))
            best = min(best, dx + dy)
        for c in list(self.net_cells[net])[:1]:
            pass
        return best

    def route(self, net, pin):
        x, y, d = self.pins[pin]
        sx, sy = x // G, y // G
        first = {"L": 1, "R": 0, "D": 2, "U": 3}[d]
        targets = self.net_cells[net]
        if (sx, sy) in targets:
            return True
        start = (sx, sy, first)
        openq = [(self._h(net, sx, sy), 0, sx, sy, first)]
        came = {start: None}
        cost = {start: 0}
        BEND, CROSS, ADJ = 10, 14, 5
        found = None
        steps = 0
        while openq:
            f, g, cx, cy, cd = heapq.heappop(openq)
            if cost.get((cx, cy, cd), 1e18) < g:
                continue
            steps += 1
            if steps > 400000:
                break
            here_cross = (self.H[cy][cx] not in (None, net)) or (self.V[cy][cx] not in (None, net))
            for nd, (dx, dy) in enumerate(self.DIRS):
                if (cx, cy) == (sx, sy) and nd != first:
                    continue
                if nd != cd and here_cross:
                    continue
                if (nd ^ 1) == cd and nd // 2 == cd // 2:
                    continue  # nicht umkehren
                nx, ny = cx + dx, cy + dy
                if not (0 <= nx < self.gw and 0 <= ny < self.gh):
                    continue
                if (nx, ny) in targets:
                    # Anschluss an vorhandenes Netz: nur an Stellen ohne fremde Belegung
                    found = (nx, ny, nd, (cx, cy, cd))
                    break
                if self.blocked[ny][nx]:
                    continue
                nn = self.node[ny][nx]
                if nn is not None and nn != net:
                    continue
                horiz = nd < 2
                occ_same = self.H[ny][nx] if horiz else self.V[ny][nx]
                occ_perp = self.V[ny][nx] if horiz else self.H[ny][nx]
                if occ_same is not None and occ_same != net:
                    continue
                ng = g + 1 + (BEND if nd != cd else 0) + (CROSS if occ_perp not in (None, net) else 0)
                # Abstand zu fremden parallelen Leitungen halten
                if horiz:
                    for qy in (ny - 1, ny + 1):
                        o = self.H[qy][nx] if 0 <= qy < self.gh else None
                        if o is not None and o != net:
                            ng += ADJ
                else:
                    for qx in (nx - 1, nx + 1):
                        o = self.V[ny][qx] if 0 <= qx < self.gw else None
                        if o is not None and o != net:
                            ng += ADJ
                st = (nx, ny, nd)
                if ng < cost.get(st, 1e18):
                    cost[st] = ng
                    came[st] = (cx, cy, cd)
                    heapq.heappush(openq, (ng + self._h(net, nx, ny), ng, nx, ny, nd))
            if found:
                break
        if not found:
            return False
        # Pfad zurueckverfolgen
        tx, ty, td, prev = found
        cells = [(tx, ty)]
        st = prev
        while st is not None:
            cells.append((st[0], st[1]))
            st = came[st]
        cells.reverse()
        # belegen
        for i in range(len(cells) - 1):
            (ax, ay), (bx, by) = cells[i], cells[i + 1]
            horiz = ay == by
            for (qx, qy) in ((ax, ay), (bx, by)):
                if horiz:
                    if self.H[qy][qx] is None:
                        self.H[qy][qx] = net
                else:
                    if self.V[qy][qx] is None:
                        self.V[qy][qx] = net
            self.edges[net].add(((ax, ay), (bx, by)) if (ax, ay) < (bx, by) else ((bx, by), (ax, ay)))
        for i, c in enumerate(cells):
            self.net_cells[net].add(c)
            if 0 < i < len(cells) - 1:
                a, b = cells[i - 1], cells[i + 1]
                if (a[0] != b[0]) and (a[1] != b[1]):   # Knick
                    self.node[c[1]][c[0]] = net
        self.node[cells[-1][1]][cells[-1][0]] = net
        self.node[cells[0][1]][cells[0][0]] = net
        # Segmente fuer Heuristik
        pts = [cells[0]]
        for i in range(1, len(cells) - 1):
            a, b, c = cells[i - 1], cells[i], cells[i + 1]
            if (a[0] == b[0] == c[0]) or (a[1] == b[1] == c[1]):
                continue
            pts.append(b)
        pts.append(cells[-1])
        for i in range(len(pts) - 1):
            self.net_segs[net].append((pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1]))
        self.paths[net].append(pts)
        return True

    def route_all(self, order=None):
        names = order or list(self.nets.keys())
        for net in names:
            pins = self.nets[net]
            if not pins:
                continue
            if not self.net_cells[net]:
                p0 = pins[0]
                x, y, _ = self.pins[p0]
                self.net_cells[net].add((x // G, y // G))
                self.net_segs[net].append((x // G, y // G, x // G, y // G))
                rest = pins[1:]
            else:
                rest = list(pins)

            def dist(p):
                x, y, _ = self.pins[p]
                return self._h(net, x // G, y // G)

            rest.sort(key=dist)
            for p in rest:
                if not self.route(net, p):
                    self.failed.append((net, p))

    LABELLED = ("I2S_", "SDA_", "SCL_", "UART_", "_USB_", "USB_D", "BQ_INT", "BAT_ALRT", "PWR_KEY", "P48_SW", "DOCK_",
                "S3_EXP_INT",
                "GAIN_SET", "GAIN_RST", "HP_EN", "ANA_EN", "P48_EN", "LED_DATA", "ENC1_", "ENC2_", "MIC_TO_CODEC", "LINE_")

    def _labelled(self, net):
        return any(k in net for k in self.LABELLED) or net in ("T1", "T2", "T3", "T4", "CODEC_RST")

    def render(self):
        s = self.s
        for net, plist in self.paths.items():
            colr = col_for(net)
            for pts in plist:
                s.line([(x * G, y * G) for x, y in pts], colr, 3)
        # Verbindungspunkte
        for net, es in self.edges.items():
            deg = defaultdict(int)
            for a, b in es:
                deg[a] += 1
                deg[b] += 1
            pin_cells = {(self.pins[p][0] // G, self.pins[p][1] // G) for p in self.nets.get(net, [])}
            for c, n in deg.items():
                extra = 1 if c in pin_cells else 0
                if n + extra >= 3:
                    s.dot(c[0] * G, c[1] * G, "#cc0000")
        # Signalnamen an den Pin-Enden (nur Verbindungen zwischen Baugruppen, zum Verfolgen)
        for net, plist in self.nets.items():
            if not self._labelled(net):
                continue
            for pn in plist:
                if "." in pn and pn.split(".")[0][:1] in ("R", "C") and pn.split(".")[0] not in ("C5",):
                    continue
                x, y, d = self.pins[pn]
                if d == "L":
                    s.text(x - 6, y - 8, net, 10, col_for(net), anchor="end", weight="bold", family=MONO)
                elif d == "R":
                    s.text(x + 6, y - 8, net, 10, col_for(net), weight="bold", family=MONO)
        # Fallback-Verweise
        for net, p in self.failed:
            x, y, d = self.pins[p]
            side = {"L": "L", "R": "R", "U": "R", "D": "R"}[d]
            s.flag(x, y, net, col_for(net), side)
        return s.svg()


# =====================================================================================
def build(priority=()):
    b = Board(5900, 3300, "NR Bodypack · Variante B: Trägerplatine V0.3 als Linien-Schaltplan (Entwurf, S3 2,8″ + Basisstation)",
              "Fast alle Verbindungen als direkte Leitung. Versorgungen = Sammelschienen (VSYS, +3V3, ±5VA, P48), "
              "die rechts außen verbunden sind. GND = Masse-Symbol. Kreuzungen ohne Punkt sind NICHT verbunden. "
              "Werte mit * vor dem Layout prüfen.")
    U = "used"
    s = b.s
    M = b.module
    b.obstacle(0, 0, 5900, 100, margin=0)

    # ================================================================== OBEN: USB, Laden, Akku, Spannungen
    b.block(40, 120, 0, 0, "USB-C · ESD · Hub")
    M("J1", 80, 170, 180, "J1 USB-C", "16-Pin", [],
      [("VBUS", "", "VBUS", U), ("DP", "", "D+", U), ("DM", "", "D−", U), ("CC1", "", "CC1", U),
       ("CC2", "", "CC2", U), ("GND", "", "GND", U)], pitch=40, top=70)
    b.gnd_pin("J1.GND")
    b.R("F1", "PTC 3 A", 310, 240, color=C["pwr"])
    b.R("R1", "5,1 k", 330, 360, vert=True); b.gnd_pin("R1.2", 10)
    b.R("R2", "5,1 k", 390, 400, vert=True); b.gnd_pin("R2.2", 10)
    M("U1", 470, 420, 160, "U1 USBLC6", "ESD", [("IO1", "", "I/O1", U), ("IO2", "", "I/O2", U)],
      [("VBUS", "", "VBUS", U), ("GND", "", "GND", U)], pitch=40, top=70)
    b.gnd_pin("U1.GND")
    M("U2", 820, 200, 240, "U2 CH334F", "USB-Hub, QFN-24",
      [("DP", "", "15 DPU", U), ("DM", "", "14 DMU", U), ("VBUS", "", "19 V5", U), ("PSELF", "", "18 PSELF", U),
       ("V33", "", "20 VDD33", U), ("GND", "", "GND (Pad)", U)],
      [("D1P", "", "12 DP1", U), ("D1M", "", "11 DM1", U), ("D2P", "", "10 DP2", U), ("D2M", "", "9 DM2", U),
       ("XI", "", "4 XI", U), ("XO", "", "3 XO", U)], pitch=40, top=70)
    b.gnd_pin("U2.GND")
    b.R("R63", "10 k", 700, 390); b.gnd_pin("R63.1", 20)
    M("Y2", 1140, 410, 110, "Y2", "", [("X1", "", "1", U), ("X2", "", "2", U)], [], pitch=40, top=20)
    b.Cap("C1", "10 µF", 700, 490); b.gnd_pin("C1.2", 10)
    b.Cap("C2", "1 µF", 760, 520); b.gnd_pin("C2.2", 10)
    M("U15", 560, 640, 350, "U15 TPS2116", "Vorrang Station, sonst USB-C",
      [("IN1", "", "3 VIN1 (Station)", U), ("IN2", "", "6 VIN2 (USB)", U), ("PR1", "", "4 PR1", U),
       ("GND", "", "1 GND", U)],
      [("OUT", "", "2+7 VOUT → Lader", U), ("ST", "", "8 ST", "nc"), ("MODE", "", "5 MODE", U)], pitch=40, top=70)
    b.gnd_pin("U15.GND")
    s.text(1150, 530, "12 MHz", 12, INK, weight="bold")
    b.R("R61", "100 k", 400, 770, vert=True, color=C["pwr"])
    b.R("R62", "33 k", 420, 860); b.gnd_pin("R62.2", 20)

    b.block(1220, 120, 0, 0, "Laderegler BQ25895 (Power-Path)")
    M("U3", 1440, 170, 260, "U3 BQ25895", "Lader 2 A, I2C 0x6A",
      [("VBUS", "", "VBUS", U), ("PMID", "", "PMID", U), ("REGN", "", "REGN", U), ("TS", "", "TS", U),
       ("ILIM", "", "ILIM", U), ("DP", "", "D+", U), ("DM", "", "D−", U), ("OTG", "", "OTG", U),
       ("CE", "", "/CE", U), ("PGND", "", "PGND", U)],
      [("SW", "", "SW", U), ("BTST", "", "BTST", U), ("SYS", "", "SYS", U), ("BAT", "", "BAT", U),
       ("SDA", "", "SDA", U), ("SCL", "", "SCL", U), ("INT", "", "/INT", U), ("STAT", "", "STAT", U),
       ("QON", "", "/QON", U), ("DSEL", "", "DSEL", U)], pitch=40, top=70)
    for p in ("U3.OTG", "U3.CE", "U3.PGND"):
        b.gnd_pin(p)
    b.Cap("C3", "1 µF", 1340, 640); b.gnd_pin("C3.2", 10)
    b.Cap("C4", "10 µF", 1280, 640); b.gnd_pin("C4.2", 10)
    b.Cap("C5", "4,7 µF", 1220, 640); b.gnd_pin("C5.2", 10)
    b.R("R6", "5,23 k*", 1160, 560, vert=True)
    b.R("R7", "30,1 k*", 1100, 660, vert=True); b.gnd_pin("R7.2", 10)
    b.R("RT1", "NTC 10 k", 1040, 660, vert=True); b.gnd_pin("RT1.2", 10)
    b.R("R3", "130 Ω", 1400, 640, vert=True); b.gnd_pin("R3.2", 10)
    b.L("L1", "2,2 µH / 5 A", 1780, 240)
    b.Cap("C6", "47 nF", 1760, 280, vert=False)
    b.Cap("C7", "2× 10 µF", 1960, 370); b.gnd_pin("C7.2", 10)
    b.Cap("C8", "10 µF", 2070, 420); b.gnd_pin("C8.2", 10)
    b.R("R5", "2,2 k", 1790, 520, color=C["pwr"])
    b.D("D1", "LED rot", 1880, 520, led=True)
    b.SW("S1", 1790, 600, "S1 Ein/Aus")
    b.gnd_pin("S1.2", 10)
    b.D("D2", "BAT54", 1800, 680, color=C["btn"])
    b.R("R4", "10 k", 2000, 470, vert=True, color=C["pwr"])
    b.R("R64", "10 k", 1740, 630, vert=True, color=C["pwr"])

    b.block(2120, 120, 0, 0, "Akku 2× 18650 · Schutz · Ladezustand")
    for i, (cx) in enumerate((2200, 2330)):
        s.line([(cx, 200), (cx, 230)], C["pwr"]); s.line([(cx - 22, 230), (cx + 22, 230)], INK, 4)
        s.line([(cx - 12, 240), (cx + 12, 240)], INK, 4); s.line([(cx, 240), (cx, 270)])
        s.text(cx + 28, 236, f"BT{i + 1}", 13, INK, weight="bold")
        b.obstacle(cx - 24, 205, cx + 60, 265, margin=0)
        b.pin(f"BT{i + 1}.P", cx, 200, "U"); b.pin(f"BT{i + 1}.N", cx, 270, "D")
    s.text(2190, 160, "18650, gleiche Zellen, parallel", 12, "#555", family=SANS)
    M("U4", 2300, 350, 160, "U4 DW01A", "Akkuschutz", [("VCC", "", "VCC", U), ("GND", "", "GND", U),
                                                       ("CS", "", "CS", U)],
      [("OD", "", "OD", U), ("OC", "", "OC", U), ("TD", "", "TD", "nc")], pitch=40, top=70)
    b.R("R8", "100 Ω", 2160, 420)
    b.Cap("C9", "100 nF", 2130, 480)
    b.R("R9", "1 k", 2160, 500); b.gnd_pin("R9.1", 10)
    M("Q1", 2300, 580, 160, "Q1 FS8205A", "2× N-MOSFET", [("G1", "", "G1", U), ("S1", "", "S1", U)],
      [("G2", "", "G2", U), ("S2", "", "S2", U)], pitch=40, top=70)
    b.gnd_pin("Q1.S2")
    s.text(2380, 700, "Drains intern verbunden · S2 = P− = GND", 11, "#555", anchor="middle", family=SANS)
    M("U5", 2620, 350, 180, "U5 MAX17048", "Ladezustand, 0x36",
      [("CELL", "", "2 CELL", U), ("VDD", "", "3 VDD", U), ("GND", "", "4 GND + EP", U), ("CTG", "", "1 CTG", U),
       ("QSTRT", "", "6 QSTRT", U)],
      [("SDA", "", "8 SDA", U), ("SCL", "", "7 SCL", U), ("ALRT", "", "5 ALRT", U)], pitch=40, top=70)
    for p in ("U5.GND", "U5.CTG", "U5.QSTRT"):
        b.gnd_pin(p)
    b.Cap("C10", "1 µF", 2560, 470); b.gnd_pin("C10.2", 10)

    b.block(2960, 120, 0, 0, "3,3 V: TPS63021 (fest 3,3 V, PWM)")
    M("U6", 3100, 170, 220, "U6 TPS63021", "PS/SYNC an VIN = PWM",
      [("VIN", "", "VIN", U), ("VINA", "", "VINA", U), ("EN", "", "EN", U), ("PS", "", "PS/SYNC", U),
       ("GND", "", "GND", U)],
      [("L1", "", "L1", U), ("L2", "", "L2", U), ("VOUT", "", "VOUT", U), ("FB", "", "FB → VOUT", U),
       ("PG", "", "PG", "nc")], pitch=40, top=70)
    b.gnd_pin("U6.GND")
    b.Cap("C11", "2× 10 µF", 2920, 280); b.gnd_pin("C11.2", 10)
    b.R("R10", "100 Ω", 2860, 280, vert=True)
    b.L("L2", "1,5 µH / 4 A", 3380, 240)
    b.Cap("C12", "3× 22 µF", 3560, 360); b.gnd_pin("C12.2", 10)

    # ---------------------------------------------------------------- 48 V: LM5155 Boost (~24 V) + Verdoppler
    b.block(3760, 120, 0, 0, "48 V Phantom: LM5155 → ca. 24 V, Spannungsverdoppler → 48 V, P-FET-Schalter")
    M("U8", 3800, 180, 240, "U8 LM5155", "Boost-Controller, ca. 250 kHz",
      [("BIAS", "", "1 BIAS", U), ("VCC", "", "2 VCC", U), ("UVLO", "", "12 UVLO/SYNC", U), ("RT", "", "10 RT", U),
       ("SS", "", "9 SS", U), ("COMP", "", "6 COMP", U), ("AGND", "", "7 AGND", U), ("PGND", "", "4 PGND", U)],
      [("GATE", "", "3 GATE", U), ("CS", "", "5 CS", U), ("FB", "", "8 FB 1,0 V", U), ("PG", "", "11 PGOOD", "nc")],
      pitch=40, top=70)
    b.gnd_pin("U8.AGND"); b.gnd_pin("U8.PGND")
    b.Cap("C70", "1 µF+10 µF", 3640, 480); b.gnd_pin("C70.2", 10)
    b.R("R80", "86,6 k*", 3780, 640, vert=True); b.gnd_pin("R80.2", 10)
    b.Cap("C74", "47 nF*", 3880, 660); b.gnd_pin("C74.2", 10)
    b.R("R85", "10 k*", 3990, 620, vert=True)
    b.Cap("C75", "10 nF*", 3990, 720); b.gnd_pin("C75.2", 10)
    M("Q4", 4180, 200, 190, "Q4 N-FET", "≥ 60 V, Logic Level*", [("G", "", "G", U), ("S", "", "S", U)],
      [("D", "", "D", U)], pitch=40, top=70)
    b.R("R81", "68 mΩ", 4120, 380, vert=True); b.gnd_pin("R81.2", 10)
    b.L("L5", "22 µH / 1,5 A", 4420, 200)
    b.D("D7", "100 V 1 A", 4600, 270, color=C["p48"])
    b.Cap("C71", "1 µF/100 V", 4720, 300); b.gnd_pin("C71.2", 10)
    b.Cap("C72", "1 µF/100 V", 4580, 420, vert=False)
    b.D("D8", "100 V", 4860, 300, vert=True, color=C["p48"])
    b.D("D9", "100 V", 4900, 440, color=C["p48"])
    b.Cap("C73", "1 µF/100 V", 5020, 470); b.gnd_pin("C73.2", 10)
    b.R("R11", "470 k", 4240, 520, vert=True)
    b.R("R12", "10 k", 4240, 640, vert=True); b.gnd_pin("R12.2", 10)
    M("Q2", 5180, 400, 200, "Q2 ZXMP10A13F", "P-FET −100 V", [("S", "", "S", U), ("G", "", "G", U)],
      [("D", "", "D", U)], pitch=40, top=70)
    b.R("R82", "100 k", 5100, 530, vert=True, color=C["p48"])
    b.D("DZ1", "Z 15 V", 5040, 600, vert=True)
    b.R("R83", "47 k", 5240, 600, vert=True)
    M("Q3", 5180, 700, 180, "Q3 MMBTA42", "NPN 300 V", [("B", "", "B", U)], [("C", "", "C", U), ("E", "", "E", U)],
      pitch=40, top=60)
    b.gnd_pin("Q3.E")
    b.R("R84", "10 k", 5040, 760)
    b.R("R13", "100 Ω", 5440, 470, color=C["p48"])
    b.Cap("C16", "47 µF/63 V", 5580, 500, polar=True); b.gnd_pin("C16.2", 10)
    s.text(4420, 860, "Verdoppler: C72 + D8 + D9 laden C73 auf ca. 2 × 24 V. FB misst an den 48 V (470 k / 10 k → 48 V).",
           12, "#444", family=SANS)

    # ---------------------------------------------------------------- ±5 V: TPS65131 (±6 V) + rauscharme LDOs
    b.block(4300, 1090, 0, 0, "±5 V Analog: TPS65131 (±6 V) → TPS7A4901 (+5 V) / TPS7A3001 (−5 V)")
    M("U7", 4700, 1160, 260, "U7 TPS65131", "±6 V, PSP/PSN = PWM",
      [("EN", "", "8+10 ENP/ENN", U), ("VIN", "", "4 VIN", U), ("INN", "", "5+6 INN", U), ("INP", "", "1+24 INP", U),
       ("PS", "", "9+11 PSP/PSN", U), ("BSW", "", "7 BSW", "nc"), ("GND", "", "2,3,19 GND", U)],
      [("VPOS", "", "23 VPOS", U), ("FBP", "", "22 FBP", U), ("VREF", "", "17 VREF", U), ("FBN", "", "16 FBN", U),
       ("VNEG", "", "15 VNEG", U), ("OUTN", "", "13+14 OUTN", U), ("CP", "", "21 CP", U), ("CN", "", "18 CN", U)],
      pitch=40, top=70)
    b.gnd_pin("U7.PS"); b.gnd_pin("U7.GND")
    b.R("R74", "100 Ω", 4440, 1270, color=C["pwr"])
    b.Cap("C60", "100 nF", 4560, 1290); b.gnd_pin("C60.2", 10)
    b.L("L3", "4,7 µH", 4480, 1640)
    b.Cap("C61", "4,7 µF", 4400, 1660); b.gnd_pin("C61.2", 10)
    b.D("D5", "Schottky 1 A", 4700, 1640, color=C["pwr"])
    b.R("R70", "470 k", 5180, 1230, vert=True)
    b.R("R71", "120 k", 5040, 1270); b.gnd_pin("R71.2", 10)
    b.R("R73", "120 k", 5260, 1310, vert=True)
    b.Cap("C63", "220 nF", 5330, 1310, vert=False); b.gnd_pin("C63.2", 10)
    b.R("R72", "590 k", 5020, 1350)
    b.Cap("C62", "22 µF", 5300, 1150)
    b.gnd_pin("C62.2", 10)
    b.D("D6", "Schottky 1 A", 5060, 1700, color=C["pwr"])
    b.L("L4", "4,7 µH", 5180, 1740); b.gnd_pin("L4.2", 10)
    b.Cap("C64", "22 µF", 4980, 1760); b.gnd_pin("C64.2", 10)
    b.Cap("C66", "10 nF", 4860, 1760); b.gnd_pin("C66.2", 10)
    b.Cap("C67", "4,7 nF", 4920, 1840); b.gnd_pin("C67.2", 10)
    M("U17", 5400, 1160, 190, "U17 TPS7A4901", "+5 V, NR 10 nF*",
      [("IN", "", "IN", U), ("EN", "", "EN", U), ("GND", "", "GND", U)], [("OUT", "", "OUT", U), ("FB", "", "FB", U)],
      pitch=40, top=70)
    b.gnd_pin("U17.GND")
    b.R("R75", "32,4 k", 5640, 1240, vert=True, color=C["pwr"])
    b.R("R76", "10 k", 5640, 1350, vert=True); b.gnd_pin("R76.2", 10)
    b.Cap("C76", "10 µF", 5700, 1180)
    b.gnd_pin("C76.2", 10)
    M("U18", 5400, 1600, 190, "U18 TPS7A3001", "−5 V, NR 10 nF*",
      [("IN", "", "IN", U), ("EN", "", "EN → IN", U), ("GND", "", "GND", U)],
      [("OUT", "", "OUT", U), ("FB", "", "FB", U)], pitch=40, top=70)
    b.gnd_pin("U18.GND")
    b.R("R77", "32,4 k", 5640, 1680, vert=True, color=C["pwr"])
    b.R("R78", "10 k", 5640, 1790, vert=True); b.gnd_pin("R78.2", 10)
    b.Cap("C77", "10 µF", 5700, 1620)
    b.gnd_pin("C77.2", 10)
    s.text(4300, 1980, "TPS65131: +6 V = 1,213 V × (1 + 470 k/120 k), −6 V = −1,213 V × 590 k/120 k. "
           "LDOs: VFB 1,185 V / −1,179 V → 32,4 k / 10 k, NR/SS 10 nF, DNC (Pin 7) frei lassen.", 12, "#444", family=SANS)

    # ================================================================== Sammelschienen
    RA = {"VSYS": 920, "+3V3": 950, "+5VA": 980, "−5VA": 1010, "P48": 1040}
    RB = {"VSYS": 2030, "+3V3": 2060, "+5VA": 2090, "−5VA": 2120, "P48": 2150}
    XR0, XR1 = 120, 5740
    for i, n in enumerate(RA):
        b.rail(n, XR0, XR1 + i * 30, RA[n])
        b.rail(n, XR0, XR1 + i * 30, RB[n])
        b.vrail(n, XR1 + i * 30, RA[n], RB[n])
    s.text(5730, 1500, "Schienen oben/unten", 12, "#555", family=SANS, anchor="end")
    s.text(5730, 1516, "sind hier verbunden", 12, "#555", family=SANS, anchor="end")

    # ================================================================== MITTE: Drehgeber, S3, C5, Codec, Expander
    b.block(40, 1100, 0, 0, "Drehgeber EC11")
    enc = []
    for i, (ey, nets) in enumerate(((1200, ("ENC1_A", "ENC1_B", "ENC1_SW")), (1520, ("ENC2_A", "ENC2_B", "ENC2_SW")))):
        k = f"ENC{i + 1}"
        M(k, 80, ey, 150, k, "EC11", [], [("A", "", "A", U), ("B", "", "B", U), ("SW", "", "Taster", U),
                                           ("C", "", "C / T2", U)], pitch=40, top=70)
        b.gnd_pin(f"{k}.C")
        for j in range(3):
            rr = f"R{40 + i * 3 + j}"
            cc = f"C{50 + i * 3 + j}"
            b.R(rr, "10 k", 330 + j * 70, ey - 10 - j * 0, vert=True, color=C["pwr"])
            b.Cap(cc, "10 nF", 330 + j * 70, ey + 200, vert=True)
            b.gnd_pin(f"{cc}.2", 10)
            enc.append((k, ["A", "B", "SW"][j], rr, cc))

    M("S3", 960, 1140, 300, "J3/J4 ESP32-S3-Touch-LCD-2.8B", "2× 12 Buchsenleiste",
      [(f"L{i + 1}", f"L{i + 1}", t, U if t in ("GND",) or (i + 1) in (3, 4, 5, 8, 9, 12) else "nc")
       for i, t in enumerate(["GND", "3V3", "GP15 SDA", "GP7 SCL", "GP16", "GP4 BAT-ADC", "GP0 BOOT", "GP20 D+",
                              "GP19 D−", "GND", "VCC", "5V*"])],
      [(f"R{i + 1}", f"R{i + 1}", t, U if t == "GND" or (i + 1) in (3, 4) else "nc")
       for i, t in enumerate(["GND", "3V3", "GP43 TX", "GP44 RX", "GP37 PSRAM", "GP36 PSRAM", "GP35 PSRAM",
                              "GP34 PSRAM", "GP33 PSRAM", "GND", "BAT (frei)", "5V"])],
      pitch=30, top=70, nums=True)
    for p_ in ("S3.L1", "S3.L10", "S3.R1", "S3.R10"):
        b.gnd_pin(p_, 14)
    s.text(1110, 1600, "5V*: Speisung des Boards aus VSYS – lt. Board-Schaltplan prüfen", 11, "#a33", anchor="middle",
           family=SANS)
    s.text(1110, 1616, "(BAT nicht benutzen, falls das Board einen eigenen Lader hat)", 11, "#a33", anchor="middle",
           family=SANS)

    M("U16", 560, 1700, 230, "U16 TCA9555", "I/O-Expander S3, 0x27",
      [("VCC", "", "VCC", U), ("SDA", "", "SDA", U), ("SCL", "", "SCL", U), ("ADDR", "", "A0–A2 = 3V3", U),
       ("INT", "", "/INT", U), ("GND", "", "GND", U)],
      [("P00", "", "P00 ENC1_A", U), ("P01", "", "P01 ENC1_B", U), ("P02", "", "P02 ENC1_SW", U),
       ("P03", "", "P03 ENC2_A", U), ("P04", "", "P04 ENC2_B", U), ("P05", "", "P05 ENC2_SW", U),
       ("P06", "", "P06 PWR_KEY", U), ("P07", "", "P07 BQ_INT", U), ("P10", "", "P10 BAT_ALRT", U),
       ("P11", "", "P11 DOCK_DET", U)], pitch=30, top=70)
    b.gnd_pin("U16.GND")
    b.R("R60", "10 k", 520, 1640, vert=True, color=C["pwr"])
    b.Cap("C45", "100 nF", 480, 1900); b.gnd_pin("C45.2", 10)

    M("C5", 1900, 1180, 220, "J2 ESP32-C5-Zero", "2× 9 Buchsenleiste",
      [("5V", "", "5V", "nc"), ("GND", "", "GND", U), ("3V3", "", "3V3", U), ("GP0", "", "GP0", U),
       ("GP1", "", "GP1", U), ("GP2", "", "GP2", U), ("GP3", "", "GP3", U), ("GP4", "", "GP4", U),
       ("GP5", "", "GP5", U)],
      [("GP11", "", "GP11", U), ("GP12", "", "GP12", U), ("GP13", "", "GP13 D−", U), ("GP14", "", "GP14 D+", U),
       ("GP10", "", "GP10", U), ("GP9", "", "GP9", U), ("GP8", "", "GP8", U), ("GP7", "", "GP7", U),
       ("GP6", "", "GP6", U)], pitch=60, top=70)
    b.gnd_pin("C5.GND")
    b.Cap("C44", "10 µF", 1760, 1310); b.gnd_pin("C44.2", 10)
    b.R("R14", "4,7 k", 2260, 1100, vert=True, color=C["pwr"])
    b.R("R15", "4,7 k", 2330, 1100, vert=True, color=C["pwr"])

    M("U9", 2800, 1140, 290, "U9 TLV320AIC3204", "Codec 48 kHz, I2C 0x18",
      [("BCLK", "", "BCLK", U), ("WCLK", "", "WCLK", U), ("DIN", "", "DIN", U), ("DOUT", "", "DOUT", U),
       ("SCL", "", "SCL", U), ("SDA", "", "SDA", U), ("RST", "", "RESET", U), ("IOVDD", "", "IOVDD", U),
       ("LDOIN", "", "LDOIN", U), ("LDOSEL", "", "LDO_SELECT", U), ("MCLK", "", "MCLK", U),
       ("SPI", "", "SPI_SELECT", U), ("GND", "", "AVSS/DVSS", U)],
      [("IN1L", "", "IN1_L", U), ("IN1R", "", "IN1_R", "nc"), ("IN2", "", "IN2_L/R", "nc"),
       ("MICB", "", "MICBIAS", "nc"), ("REF", "", "REF", U), ("AVDD", "", "AVDD 1,8 V", U),
       ("DVDD", "", "DVDD 1,8 V", U), ("LOL", "", "LOL", U), ("LOR", "", "LOR", U), ("HP", "", "HPL/HPR", "nc"),
       ("MFP", "", "MFP3–5", "nc")], pitch=40, top=70)
    for p in ("U9.MCLK", "U9.SPI", "U9.GND"):
        b.gnd_pin(p)
    b.R("R16", "10 k", 2640, 1100, vert=True, color=C["pwr"])
    b.Cap("C17", "10 µF+100 nF", 2560, 1560); b.gnd_pin("C17.2", 10)
    b.Cap("C18", "10 µF+100 nF", 2470, 1600); b.gnd_pin("C18.2", 10)
    for i, (pin, ref, val) in enumerate((("U9.REF", "C19", "10 µF"), ("U9.AVDD", "C20", "10 µF+100 nF"),
                                         ("U9.DVDD", "C21", "10 µF+100 nF"))):
        x, y, _ = b.pins[pin]
        b.Cap(ref, val, x + 30 + i * 0, y, vert=False)
        b.gnd_pin(f"{ref}.2", 10)

    M("U10", 3700, 1240, 220, "U10 TCA9555", "I/O-Expander C5, 0x20",
      [("VCC", "", "VCC", U), ("SDA", "", "SDA", U), ("SCL", "", "SCL", U), ("ADDR", "", "A0–A2", U),
       ("INT", "", "/INT (Abfrage)", "nc"), ("GND", "", "GND", U)],
      [("P0", "", "P00 P48_SW", U), ("P1", "", "P01 GAIN_SET", U), ("P2", "", "P02 HP_EN", U), ("P3", "", "P03 RST", U),
       ("P4", "", "P04 ANA_EN", U), ("P5", "", "P05 P48_EN", U), ("P6", "", "P06 GAIN_RST", U), ("P10", "", "P10 T1", U), ("P11", "", "P11 T2", U),
       ("P12", "", "P12 T3", U), ("P13", "", "P13 T4", U)],
      pitch=40, top=70)
    b.gnd_pin("U10.ADDR"); b.gnd_pin("U10.GND")
    b.Cap("C22", "100 nF", 3580, 1480); b.gnd_pin("C22.2", 10)

    # ================================================================== Andock-Kontakte (Basisstation)
    b.block(40, 2780, 0, 0, "Andock-Kontakte Basisstation (LAN über W5500 in der Station, 5 V laden)")
    M("J7", 80, 2850, 170, "J7 Kontaktflächen", "zu Federstiften",
      [], [("SCK", "", "SCK", U), ("MOSI", "", "MOSI", U), ("MISO", "", "MISO", U), ("CS", "", "CS", U),
           ("V5", "", "5V Station", U), ("DET", "", "DET (GND in Station)", U), ("GND", "", "GND", U)],
      pitch=40, top=60)
    b.gnd_pin("J7.GND")
    # TPD4E05U06 ist ein Durchgangs-Baustein (flow-through): IOx links und rechts sind dieselbe Leitung
    M("U14", 420, 2850, 200, "U14 TPD4E05U06", "ESD 4-fach, flow-through",
      [("IO1", "", "IO1", U), ("IO2", "", "IO2", U), ("IO3", "", "IO3", U), ("IO4", "", "IO4", U)],
      [("O1", "", "IO1", U), ("O2", "", "IO2", U), ("O3", "", "IO3", U), ("O4", "", "IO4", U), ("GND", "", "GND", U)],
      pitch=40, top=60)
    b.gnd_pin("U14.GND")
    for i in range(4):
        b.R(f"R{50 + i}", "33 Ω", 700, 2910 + i * 40, color=C["btn"])
    b.Cap("C46", "10 µF", 330, 2700); b.gnd_pin("C46.2", 10)

    # ================================================================== UNTEN: Tasten, LEDs, Mikrofon, Kopfhoerer
    b.block(1300, 2210, 0, 0, "Tasten T1–T4 + RGB-LEDs")
    for i in range(4):
        x0 = 1300 + i * 230
        b.R(f"R{33 + i}", "1 k", x0, 2300, color=C["btn"])
        b.SW(f"T{i + 1}", x0 + 80, 2300, f"T{i + 1}")
        b.gnd_pin(f"T{i + 1}.2", 10)
        b.Cap(f"C{36 + i}", "100 nF", x0 + 70, 2380); b.gnd_pin(f"C{36 + i}.2", 10)
    b.R("R37", "33 Ω", 1300, 2600, color=C["led"])
    for i in range(4):
        x0 = 1420 + i * 220
        M(f"LED{i + 1}", x0, 2540, 110, f"LED{i + 1}", "SK6812MINI", [("DIN", "", "DIN", U), ("VDD", "", "VDD", U)],
          [("DOUT", "", "DOUT", U), ("GND", "", "GND", U)], pitch=40, top=60)
        b.gnd_pin(f"LED{i + 1}.GND", 10)
        b.Cap(f"C{40 + i}", "100 nF", x0 - 40, 2700); b.gnd_pin(f"C{40 + i}.2", 10)

    b.block(2300, 2210, 0, 0, "Mikrofon: Kombibuchse XLR / 6,35 mm, Phantom 48 V, Vorverstärker")
    M("J5", 2300, 2280, 200, "J5 Neutrik Kombi", "NCJ6FI-H", [],
      [("X2", "", "XLR 2 (+)", U), ("X3", "", "XLR 3 (−)", U), ("X1", "", "XLR 1 GND", U), ("TT", "", "Klinke T", U),
       ("TR", "", "Klinke R", U), ("TS", "", "Klinke S", U)], pitch=40, top=70)
    b.gnd_pin("J5.X1"); b.gnd_pin("J5.TS")
    b.R("R17", "6,81 k 0,1 %", 2640, 2200, vert=True, color=C["p48"])
    b.R("R18", "6,81 k 0,1 %", 2730, 2200, vert=True, color=C["p48"])
    b.Cap("C23", "47 µF/63 V", 2800, 2350, vert=False, polar=True)
    b.Cap("C24", "47 µF/63 V", 2800, 2470, vert=False, polar=True)
    b.R("R19", "10 Ω", 2900, 2350)
    b.R("R20", "10 Ω", 2900, 2470)
    M("D3", 3010, 2530, 120, "D3 BAV99", "Klemme +", [("IO", "", "IO", U)], [("P", "", "→ +5VA", U),
                                                                               ("N", "", "→ −5VA", U)], pitch=40, top=60)
    M("D4", 3010, 2680, 120, "D4 BAV99", "Klemme −", [("IO", "", "IO", U)], [("P", "", "→ +5VA", U),
                                                                               ("N", "", "→ −5VA", U)], pitch=40, top=60)
    b.R("R21", "10 k", 3020, 2370, vert=True); b.gnd_pin("R21.2", 10)
    b.Cap("C26", "100 pF", 3060, 2370); b.gnd_pin("C26.2", 10)
    b.Cap("C25", "470 pF", 3130, 2400)
    b.R("R22", "10 k", 3170, 2490, vert=True); b.gnd_pin("R22.2", 10)
    b.Cap("C27", "100 pF", 3220, 2490); b.gnd_pin("C27.2", 10)
    M("U11", 3380, 2280, 200, "U11 INA163", "Mic-Vorverstärker",
      [("INP", "", "V+IN", U), ("INN", "", "V−IN", U), ("RG1", "", "RG", U), ("RG2", "", "RG", U), ("REF", "", "REF", U)],
      [("VP", "", "V+", U), ("VN", "", "V−", U), ("OUT", "", "OUT", U)], pitch=40, top=70)
    b.gnd_pin("U11.REF")
    b.R("RG1", "680 Ω", 3300, 2530, vert=True)
    M("K1", 3060, 2810, 200, "K1 Relais bistabil", "2 Spulen, 3 V*",
      [("COM", "", "COM", U), ("NO", "", "NO", U)],
      [("SP", "", "Spule SET +", U), ("SM", "", "Spule SET −", U), ("RP", "", "Spule RESET +", U),
       ("RM", "", "Spule RESET −", U)], pitch=40, top=60)
    b.D("D10", "1N4148W", 3760, 2900)
    b.D("D11", "1N4148W", 3760, 3120)
    M("Q5", 3520, 2840, 150, "Q5 MMBT3904", "NPN", [("B", "", "B", U)], [("C", "", "C", U), ("E", "", "E", U)],
      pitch=40, top=60)
    M("Q6", 3520, 3060, 150, "Q6 MMBT3904", "NPN", [("B", "", "B", U)], [("C", "", "C", U), ("E", "", "E", U)],
      pitch=40, top=60)
    b.gnd_pin("Q5.E"); b.gnd_pin("Q6.E")
    b.R("R88", "1 k", 3420, 2900)
    b.R("R89", "1 k", 3420, 3120)
    b.R("RG2", "68 Ω", 3320, 2810)
    b.Cap("C28", "10µ ‖ C78 100n", 3720, 2230); b.gnd_pin("C28.2", 10)
    b.Cap("C79", "10µ ‖ C80 100n", 3780, 2400); b.gnd_pin("C79.2", 10)
    b.R("R23", "4,7 k", 3660, 2560, color=C["ana"])
    b.R("R24", "1,2 k", 3760, 2580, vert=True); b.gnd_pin("R24.2", 10)
    b.Cap("C29", "1 µF X7R", 3800, 2560, vert=False)

    b.block(3960, 2210, 0, 0, "Kopfhörer: OPA1622 + 6,35 mm Stereo")
    M("U13", 4120, 2300, 210, "U13 OPA1622", "Kopfhörer-Verstärker",
      [("IN1P", "", "IN1+", U), ("IN1N", "", "IN1−", U), ("IN2P", "", "IN2+", U), ("IN2N", "", "IN2−", U),
       ("EN", "", "EN", U)],
      [("VP", "", "V+", U), ("O1", "", "OUT1", U), ("O2", "", "OUT2", U), ("VN", "", "V−", U)], pitch=40, top=70)
    b.Cap("C30", "2,2 µF", 3990, 2370, vert=False)
    b.Cap("C31", "2,2 µF", 3990, 2450, vert=False)
    b.R("R25", "47 k", 4060, 2560, vert=True); b.gnd_pin("R25.2", 10)
    b.R("R26", "47 k", 3990, 2600, vert=True); b.gnd_pin("R26.2", 10)
    b.R("R27", "1 k", 4400, 2240, color=C["ana"])
    b.R("R28", "1 k", 4400, 2620, color=C["ana"])
    b.R("R29", "1 k", 4080, 2700, vert=True); b.gnd_pin("R29.2", 10)
    b.R("R30", "1 k", 4150, 2740, vert=True); b.gnd_pin("R30.2", 10)
    b.R("R31", "10 Ω", 4400, 2420, color=C["ana"])
    b.R("R32", "10 Ω", 4400, 2500, color=C["ana"])
    b.Cap("C32", "10µ ‖ C33 100n", 4500, 2230); b.gnd_pin("C32.2", 10)
    b.Cap("C34", "10µ ‖ C35 100n", 4620, 2640); b.gnd_pin("C34.2", 10)
    M("J6", 4580, 2400, 100, "J6", "6,35 Stereo", [("T", "", "T (L)", U), ("R", "", "R (R)", U), ("S", "", "S GND", U)],
      [], pitch=40, top=70)
    b.gnd_pin("J6.S")

    # ================================================================== NETZE
    N = b.net
    # lokale Netze zuerst (kurz), dann Signale, dann Versorgungen
    N("J1VB", "J1.VBUS", "F1.1")
    N("CC1", "J1.CC1", "R1.1"); N("CC2", "J1.CC2", "R2.1")
    N("VBUS", "F1.2", "U1.VBUS", "U2.VBUS", "C1.1", "U15.IN2")
    N("VBUS_CHG", "U15.OUT", "U3.VBUS", "C3.1")
    N("DOCK_5V", "J7.V5", "U15.IN1", "U15.MODE", "R61.1", "C46.1")
    N("PR1", "U15.PR1", "R61.2", "R62.1")
    N("HUB_XI", "U2.XI", "Y2.X1"); N("HUB_XO", "U2.XO", "Y2.X2"); N("PSELF", "U2.PSELF", "R63.2")
    N("DSEL", "U3.DSEL", "R64.1")
    N("USB_DP", "J1.DP", "U1.IO1", "U2.DP", "U3.DP")
    N("USB_DM", "J1.DM", "U1.IO2", "U2.DM", "U3.DM")
    N("HUB_V33", "U2.V33", "C2.1")
    N("PMID", "U3.PMID", "C4.1")
    N("REGN", "U3.REGN", "C5.1", "R6.1")
    N("TS", "U3.TS", "R6.2", "R7.1", "RT1.1")
    N("ILIM", "U3.ILIM", "R3.1")
    N("SW_BQ", "U3.SW", "L1.1", "C6.2")
    N("BTST", "U3.BTST", "C6.1")
    N("BAT+", "U3.BAT", "C8.1", "BT1.P", "BT2.P", "R8.1", "U5.CELL", "U5.VDD", "C10.1")
    N("STAT", "U3.STAT", "D1.K")
    N("LEDSTAT", "R5.2", "D1.A")
    N("QON", "U3.QON", "S1.1", "D2.K")
    N("BAT−", "BT1.N", "BT2.N", "U4.GND", "Q1.S1", "C9.2")
    N("DW_VCC", "U4.VCC", "R8.2", "C9.1")
    N("DW_CS", "U4.CS", "R9.2")
    N("OD", "U4.OD", "Q1.G1"); N("OC", "U4.OC", "Q1.G2")
    N("VINA", "U6.VINA", "R10.2")
    N("SW1_63", "U6.L1", "L2.1"); N("SW2_63", "U6.L2", "L2.2")
    N("FB48", "U8.FB", "R11.2", "R12.1")
    N("RT48", "U8.RT", "R80.1"); N("SS48", "U8.SS", "C74.1"); N("COMP48", "U8.COMP", "R85.1")
    N("COMP48B", "R85.2", "C75.1")
    N("GATE48", "U8.GATE", "Q4.G"); N("CS48", "U8.CS", "Q4.S", "R81.1")
    N("SW48", "L5.2", "Q4.D", "D7.A", "C72.1")
    N("P24", "D7.K", "C71.1", "D8.A"); N("DBL", "C72.2", "D8.K", "D9.A")
    N("P48_RAW", "D9.K", "C73.1", "R11.1", "Q2.S", "R82.1", "DZ1.K")
    N("P48D", "Q2.D", "R13.1")
    N("QG", "Q2.G", "R82.2", "DZ1.A", "R83.1"); N("QC", "R83.2", "Q3.C"); N("QB", "R84.2", "Q3.B")
    N("VINF", "R74.2", "C60.1", "U7.VIN")
    N("SWP", "L3.2", "U7.INP", "D5.A")
    N("V6P", "D5.K", "U7.VPOS", "C62.1", "R70.1", "U17.IN", "U17.EN")
    N("FBP", "U7.FBP", "R70.2", "R71.1")
    N("VREF65", "U7.VREF", "R73.1", "C63.1")
    N("FBN", "U7.FBN", "R72.1", "R73.2")
    N("V6N", "U7.VNEG", "R72.2", "D6.A", "C64.1", "U18.IN", "U18.EN")
    N("SWN", "U7.OUTN", "D6.K", "L4.1")
    N("CP65", "U7.CP", "C66.1"); N("CN65", "U7.CN", "C67.1")
    N("FB5P", "U17.FB", "R75.2", "R76.1"); N("FB5N", "U18.FB", "R77.2", "R78.1")
    N("CODEC_RST", "U9.RST", "R16.2", "U10.P3")
    N("MICP", "J5.X2", "J5.TT", "R17.2", "C23.1")
    N("MICN", "J5.X3", "J5.TR", "R18.2", "C24.1")
    N("MICP2", "C23.2", "R19.1"); N("MICN2", "C24.2", "R20.1")
    N("INP", "R19.2", "D3.IO", "R21.1", "C26.1", "C25.1", "U11.INP")
    N("INN", "R20.2", "D4.IO", "R22.1", "C27.1", "C25.2", "U11.INN")
    N("RGA", "U11.RG1", "RG1.1", "K1.COM")
    N("RGB", "U11.RG2", "RG1.2", "RG2.2")
    N("RGS", "K1.NO", "RG2.1")
    N("KSET", "K1.SM", "D10.A", "Q5.C"); N("KRST", "K1.RM", "D11.A", "Q6.C")
    N("KB1", "R88.2", "Q5.B"); N("KB2", "R89.2", "Q6.B")
    N("PRE_OUT", "U11.OUT", "R23.1")
    N("ATT", "R23.2", "R24.1", "C29.1")
    N("MIC_TO_CODEC", "C29.2", "U9.IN1L")
    N("LINE_L", "U9.LOL", "C30.1"); N("LINE_R", "U9.LOR", "C31.1")
    N("HP1IN", "C30.2", "R25.1", "U13.IN1P"); N("HP2IN", "C31.2", "R26.1", "U13.IN2P")
    N("HP1FB", "U13.IN1N", "R27.1", "R29.1"); N("HP2FB", "U13.IN2N", "R28.1", "R30.1")
    N("HP1OUT", "U13.O1", "R27.2", "R31.1"); N("HP2OUT", "U13.O2", "R28.2", "R32.1")
    N("HPL", "R31.2", "J6.T"); N("HPR", "R32.2", "J6.R")
    N("REF", "U9.REF", "C19.1"); N("AVDD", "U9.AVDD", "C20.1"); N("DVDD", "U9.DVDD", "C21.1")
    for i in range(4):
        N(f"T{i + 1}K", f"R{33 + i}.2", f"C{36 + i}.1", f"T{i + 1}.1")
    N("LD1", "R37.2", "LED1.DIN")
    for i in range(3):
        N(f"LD{i + 2}", f"LED{i + 1}.DOUT", f"LED{i + 2}.DIN")
    for (k, sig, rr, cc) in enc:
        pin_s3 = {"ENC1.A": "U16.P00", "ENC1.B": "U16.P01", "ENC1.SW": "U16.P02", "ENC2.A": "U16.P03",
                  "ENC2.B": "U16.P04", "ENC2.SW": "U16.P05"}[f"{k}.{sig}"]
        N(f"{k}_{sig}", f"{k}.{sig}", f"{rr}.2", f"{cc}.1", pin_s3)
    # Signale zwischen den Modulen
    N("I2S_BCLK", "C5.GP4", "U9.BCLK"); N("I2S_WS", "C5.GP5", "U9.WCLK")
    N("I2S_DOUT", "C5.GP8", "U9.DIN"); N("I2S_DIN", "C5.GP6", "U9.DOUT")
    N("SDA_A", "C5.GP9", "R14.2", "U9.SDA", "U10.SDA"); N("SCL_A", "C5.GP10", "R15.2", "U9.SCL", "U10.SCL")
    N("UART_S3_TX", "S3.R3", "C5.GP3"); N("UART_C5_TX", "C5.GP2", "S3.R4")      # gekreuzt: TX -> RX
    N("T1", "U10.P10", "R33.1"); N("T2", "U10.P11", "R34.1"); N("T3", "U10.P12", "R35.1"); N("T4", "U10.P13", "R36.1")
    N("DOCK_SCK", "C5.GP11", "R50.2"); N("DOCK_MOSI", "C5.GP12", "R51.2")
    N("DOCK_MISO", "C5.GP1", "R52.2"); N("DOCK_CS", "C5.GP0", "R53.2")
    N("DK_SCK", "J7.SCK", "U14.IO1"); N("DK_MOSI", "J7.MOSI", "U14.IO2")
    N("DK_MISO", "J7.MISO", "U14.IO3"); N("DK_CS", "J7.CS", "U14.IO4")
    N("DK2_SCK", "U14.O1", "R50.1"); N("DK2_MOSI", "U14.O2", "R51.1")
    N("DK2_MISO", "U14.O3", "R52.1"); N("DK2_CS", "U14.O4", "R53.1")
    N("DOCK_DET", "J7.DET", "U16.P11")                 # Pull-up im TCA9555, Station zieht auf GND
    N("LED_DATA", "C5.GP7", "R37.1")
    N("C5_USB_DP", "U2.D1P", "C5.GP14"); N("C5_USB_DM", "U2.D1M", "C5.GP13")
    N("S3_USB_DP", "U2.D2P", "S3.L8"); N("S3_USB_DM", "U2.D2M", "S3.L9")
    N("SDA_S3", "U3.SDA", "U5.SDA", "U16.SDA", "S3.L3"); N("SCL_S3", "U3.SCL", "U5.SCL", "U16.SCL", "S3.L4")
    N("S3_EXP_INT", "U16.INT", "R60.2", "S3.L5")
    N("BQ_INT", "U3.INT", "R4.2", "U16.P07"); N("BAT_ALRT", "U5.ALRT", "U16.P10")
    N("PWR_KEY", "D2.A", "U16.P06")
    N("P48_SW", "U10.P0", "R84.1"); N("GAIN_SET", "U10.P1", "R88.1"); N("GAIN_RST", "U10.P6", "R89.1"); N("HP_EN", "U10.P2", "U13.EN")
    N("ANA_EN", "U10.P4", "U7.EN"); N("P48_EN", "U10.P5", "U8.UVLO")
    # Versorgungen (an Sammelschienen)
    N("VSYS", "U3.SYS", "L1.2", "C7.1", "R5.1", "U6.VIN", "C11.1", "R10.1", "U6.EN", "U6.PS", "R74.1", "U7.INN", "L3.1", "C61.1",
      "U8.BIAS", "U8.VCC", "C70.1", "L5.1", "S3.L12", "LED1.VDD", "LED2.VDD", "LED3.VDD", "LED4.VDD", "C40.1", "C41.1", "C42.1", "C43.1")
    N("+3V3", "U6.VOUT", "C12.1", "U6.FB", "C5.3V3", "C44.1", "R14.1", "R15.1", "R4.1", "R16.1", "U9.IOVDD",
      "U9.LDOIN", "U9.LDOSEL", "C17.1", "C18.1", "U10.VCC", "C22.1", "U16.VCC", "U16.ADDR", "C45.1", "R60.1", "R64.2", "K1.SP", "K1.RP", "D10.K", "D11.K",

      *[f"{rr}.1" for (_, _, rr, _) in enc])
    N("+5VA", "U17.OUT", "R75.1", "C76.1", "U11.VP", "C28.1", "U13.VP", "C32.1", "D3.P", "D4.P")
    N("−5VA", "U18.OUT", "R77.1", "C77.1", "U11.VN", "C79.1", "U13.VN", "C34.1", "D3.N", "D4.N")
    N("P48", "R13.2", "C16.1", "R17.1", "R18.1")

    local_first = [n for n in b.nets if b.nets[n] and n not in ("VSYS", "+3V3", "+5VA", "−5VA", "P48")]
    local_first.sort(key=lambda n: len(b.nets[n]) * 1000 + spread(b, n))
    order = local_first + ["P48", "−5VA", "+5VA", "+3V3", "VSYS"]
    order = [n for n in priority if n in order] + [n for n in order if n not in priority]
    b.route_all(order)

    # Hinweise
    b.note(60, 2300, [
        "So lesen:",
        "• Jede farbige Linie ist eine Leitung. Kreuzen sich",
        "  zwei Linien ohne roten Punkt, sind sie NICHT verbunden.",
        "• Roter Punkt = Verbindung.",
        "• Sammelschienen VSYS, +3V3, +5VA, −5VA, P48 laufen",
        "  quer über das Blatt (oben und unten) und sind rechts",
        "  außen senkrecht miteinander verbunden.",
        "• GND = Masse-Symbol (alle Massen sind eine Fläche).",
        "• IC-Pins mit Namen, Nummern aus dem EasyEDA-Symbol.",
        "• Werte mit * vor dem Layout gegen das Datenblatt prüfen.",
    ], size=14)
    svg = b.render()
    return svg, b.failed


def spread(b, n):
    xs = [b.pins[p][0] for p in b.nets[n]]
    ys = [b.pins[p][1] for p in b.nets[n]]
    return (max(xs) - min(xs)) + (max(ys) - min(ys))


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    os.makedirs(out, exist_ok=True)
    prio, best = [], None
    for attempt in range(6):
        svg, failed = build(prio)
        print(f"Durchlauf {attempt + 1}: {len(failed)} Verweise", [f[0] for f in failed])
        if best is None or len(failed) < len(best[1]):
            best = (svg, failed)
        if not failed:
            break
        for n, _ in failed:
            if n not in prio:
                prio.insert(0, n)
    svg, failed = best
    path = os.path.join(out, "schaltplan-platine-v1-linien.svg")
    with open(path, "w", encoding="utf-8") as f:
        f.write(svg)
    print("geschrieben:", path)
    print("nicht geroutet (Verweise):", len(failed))
    for n, p in failed:
        print("  ", n, p)
