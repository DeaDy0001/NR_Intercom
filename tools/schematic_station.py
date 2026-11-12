#!/usr/bin/env python3
"""Basisstation als Linien-Schaltplan (gleicher Router wie tools/schematic_routed.py).

Die Station haelt ein Bodypack, gibt ihm 5 V zum Laden und LAN ueber einen W5500 (SPI ueber Federkontakte).

Aufruf: python tools/schematic_station.py docs/img
Erzeugt: docs/img/schaltplan-basisstation-linien.svg
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from schematics import C  # noqa: E402
import schematic_routed as sr  # noqa: E402
from schematic_routed import Board, spread  # noqa: E402

for n in ("ETH_TXP", "ETH_TXN", "ETH_RXP", "ETH_RXN", "TX_CT", "RX_CT", "J3_TCT", "J3_RCT"):
    sr.NET_COLOR[n] = C["usb"]
for n in ("+5V", "DOCK_5V", "J1VB", "AVDD", "+5V_USB", "+5V_POE"):
    sr.NET_COLOR[n] = C["pwr"]
sr.NET_COLOR["+3V3"] = C["pwr"]
for n in ("POE_A+", "POE_A−", "POE_B+", "POE_B−"):
    sr.NET_COLOR[n] = C["p48"]
for n in ("LINK_K", "ACT_K", "LINKLED", "ACTLED", "PLED", "FLED"):
    sr.NET_COLOR[n] = C["led"]
for n in ("DOCK_SCK", "DOCK_MOSI", "DOCK_MISO", "DOCK_CS", "DK_SCK", "DK_MOSI", "DK_MISO", "DK_CS", "W_MISO"):
    sr.NET_COLOR[n] = C["i2s"]
sr.Board.LABELLED = sr.Board.LABELLED + ("ETH_",)

W, H = 2900, 1760


def build(priority=()):
    b = Board(W, H, "NR Bodypack · Basisstation V0.2 als Linien-Schaltplan (Entwurf, mit PoE)",
              "Bodypack einstecken → 5 V zum Laden über die Kontakte, LAN über den W5500 in der Station (SPI über "
              "Federstifte). Versorgung über USB-C oder PoE. GND = Masse-Symbol. Kreuzungen ohne Punkt sind NICHT verbunden. Werte mit * vor dem "
              "Layout prüfen.")
    U = "used"
    M = b.module
    b.obstacle(0, 0, W, 100, margin=0)

    # ================================================================== Stromeingang
    b.block(40, 120, 0, 0, "Stromeingang: USB-C 5 V / 3 A oder PoE")
    M("J1", 80, 170, 180, "J1 USB-C", "nur Strom, 5 V 3 A", [],
      [("VBUS", "", "VBUS", U), ("CC1", "", "CC1", U), ("CC2", "", "CC2", U), ("GND", "", "GND", U)],
      pitch=40, top=70)
    b.gnd_pin("J1.GND")
    b.R("F1", "PTC 3 A", 310, 240, color=C["pwr"])
    b.R("R1", "5,1 k", 330, 280, vert=True); b.gnd_pin("R1.2", 10)
    b.R("R2", "5,1 k", 390, 320, vert=True); b.gnd_pin("R2.2", 10)
    b.D("D1", "SMBJ5.0CA", 480, 280, vert=True, color=C["pwr"]); b.gnd_pin("D1.K", 10)
    # Umschalter USB-C / PoE (gleicher Baustein wie U15 im Bodypack)
    M("U6", 580, 170, 240, "U6 TPS2116", "5 V: USB-C oder PoE",
      [("IN1", "", "VIN1 (USB-C)", U), ("IN2", "", "VIN2 (PoE)", U), ("PR1", "", "PR1/MODE lt. DB*", "nc"),
       ("GND", "", "GND", U)],
      [("OUT", "", "VOUT", U), ("ST", "", "ST", "nc")], pitch=40, top=70)
    b.gnd_pin("U6.GND")
    b.Cap("C1", "47 µF", 880, 280, polar=True); b.gnd_pin("C1.2", 10)
    b.R("R3", "1 k", 980, 280, vert=True, color=C["pwr"])
    b.D("D2", "LED grün", 980, 380, vert=True, led=True, color=C["btn"]); b.gnd_pin("D2.K", 10)

    # ================================================================== Schalter fuer die 5 V an den Kontakten
    b.block(1120, 120, 0, 0, "5 V an die Kontakte (strombegrenzt)")
    M("U1", 1180, 170, 220, "U1 TPS2557", "Lastschalter, I-Limit",
      [("IN", "", "IN", U), ("EN", "", "EN", U), ("ILIM", "", "ILIM", U), ("GND", "", "GND", U)],
      [("OUT", "", "OUT", U), ("FLT", "", "/FAULT", U)], pitch=40, top=70)
    b.gnd_pin("U1.GND")
    b.R("R4", "R_ILIM*", 1080, 340, vert=True); b.gnd_pin("R4.2", 10)
    b.Cap("C2", "1 µF", 1480, 300); b.gnd_pin("C2.2", 10)
    b.R("R5", "1 k", 1440, 430, color=C["pwr"])
    b.D("D3", "LED rot", 1540, 430, led=True)

    # ================================================================== 3,3 V
    b.block(1700, 120, 0, 0, "3,3 V für den W5500")
    M("U2", 1760, 170, 200, "U2 AP2112K-3.3", "LDO 600 mA",
      [("VIN", "", "VIN", U), ("EN", "", "EN", U), ("GND", "", "GND", U)],
      [("VOUT", "", "VOUT", U), ("NC", "", "NC", "nc")], pitch=40, top=70)
    b.gnd_pin("U2.GND")
    b.Cap("C3", "1 µF", 1650, 300); b.gnd_pin("C3.2", 10)
    b.Cap("C4", "10 µF", 2020, 280); b.gnd_pin("C4.2", 10)

    # ================================================================== Federstifte
    b.block(40, 700, 0, 0, "Federstifte zum Bodypack")
    M("J2", 80, 760, 200, "J2 Federstifte", "7× Pogo-Pin, gefedert",
      [], [("SCK", "", "SCK", U), ("MOSI", "", "MOSI", U), ("MISO", "", "MISO", U), ("CS", "", "CS", U),
           ("V5", "", "5 V → Bodypack", U), ("DET", "", "DET (auf GND)", U), ("GND", "", "GND", U)],
      pitch=40, top=60)
    b.gnd_pin("J2.DET"); b.gnd_pin("J2.GND")
    M("U4", 440, 760, 200, "U4 TPD4E05U06", "ESD 4-fach, flow-through",
      [("IO1", "", "IO1", U), ("IO2", "", "IO2", U), ("IO3", "", "IO3", U), ("IO4", "", "IO4", U)],
      [("O1", "", "IO1", U), ("O2", "", "IO2", U), ("O3", "", "IO3", U), ("O4", "", "IO4", U), ("GND", "", "GND", U)],
      pitch=40, top=60)
    b.gnd_pin("U4.GND")
    b.R("R6", "33 Ω", 760, 870, color=C["i2s"])
    b.R("R7", "10 k", 900, 640, vert=True, color=C["pwr"])

    # ================================================================== W5500
    b.block(1000, 705, 0, 0, "Netzwerk-Chip W5500 (SPI → 100 Mbit Ethernet)")
    M("U3", 1040, 720, 300, "U3 W5500", "Ethernet mit PHY, LQFP-48",
      [("SCLK", "", "SCLK", U), ("MOSI", "", "MOSI", U), ("MISO", "", "MISO", U), ("SCS", "", "/SCS", U),
       ("INT", "", "/INT (Abfrage)", "nc"), ("XI", "", "XI", U), ("XO", "", "XO", U),
       ("PMODE", "", "PMODE0–2 offen = Auto", "nc"), ("RST", "", "/RST", U), ("GND", "", "GND/AGND", U)],
      [("TXP", "", "TXP", U), ("TXN", "", "TXN", U), ("RXP", "", "RXP", U), ("RXN", "", "RXN", U),
       ("LINK", "", "LINKLED", U), ("ACT", "", "ACTLED", U), ("EXRES", "", "EXRES1", U), ("TOCAP", "", "TOCAP", U),
       ("V12", "", "1V2O", U), ("VBG", "", "VBG", "nc"), ("VDD", "", "VDD", U), ("AVDD", "", "AVDD", U)],
      pitch=40, top=70)
    b.gnd_pin("U3.GND")
    b.R("R8", "10 k", 980, 1050, vert=True, color=C["pwr"])
    b.Cap("C5", "100 nF", 920, 1110); b.gnd_pin("C5.2", 10)
    M("Y1", 700, 950, 140, "Y1 25 MHz", "Quarz", [], [("X1", "", "1", U), ("X2", "", "2", U)], pitch=40, top=40)
    b.Cap("C6", "18 pF*", 720, 1120); b.gnd_pin("C6.2", 10)
    b.Cap("C7", "18 pF*", 800, 1160); b.gnd_pin("C7.2", 10)

    # Abschluss der Leitungspaare (Werte lt. WIZnet-Referenzschaltung pruefen)
    b.R("R9", "49,9 Ω", 1420, 620, vert=True, color=C["usb"])
    b.R("R10", "49,9 Ω", 1480, 620, vert=True, color=C["usb"])
    b.R("R11", "49,9 Ω", 1580, 620, vert=True, color=C["usb"])
    b.R("R12", "49,9 Ω", 1640, 620, vert=True, color=C["usb"])
    b.Cap("C8", "6,8 nF*", 1740, 560); b.gnd_pin("C8.2", 10)
    b.R("R13", "330 Ω", 1420, 950, color=C["led"])
    b.R("R14", "330 Ω", 1560, 990, color=C["led"])
    b.R("R15", "12,4 k 1 %", 1580, 1030, vert=True); b.gnd_pin("R15.2", 10)
    b.Cap("C10", "4,7 µF", 1480, 1070); b.gnd_pin("C10.2", 10)
    b.Cap("C11", "10 nF", 1380, 1100); b.gnd_pin("C11.2", 10)
    b.Cap("C12", "100 nF", 1460, 1260); b.gnd_pin("C12.2", 10)
    b.L("L1", "Ferrit 600 Ω", 1560, 1230)
    b.Cap("C13", "10 µF+100 nF", 1400, 1320); b.gnd_pin("C13.2", 10)

    # ================================================================== RJ45
    b.block(1940, 660, 0, 0, "RJ45 (PoE-fähig) mit Übertrager + LEDs")
    M("J3", 1960, 720, 230, "J3 RJ45", "Magjack, PoE-fähig*",
      [("TDP", "", "TD+", U), ("TDN", "", "TD−", U), ("RDP", "", "RD+", U), ("RDN", "", "RD−", U),
       ("LGK", "", "LED grün K", U), ("LYK", "", "LED gelb K", U), ("TCT", "", "TCT", U), ("RCT", "", "RCT", U),
       ("LGA", "", "LED grün A", U), ("LYA", "", "LED gelb A", U), ("SH", "", "Schirm", U)],
      [("VAP", "", "PoE A (1/2)", U), ("VAN", "", "PoE A (3/6)", U), ("VBP", "", "PoE B (4/5)", U),
       ("VBN", "", "PoE B (7/8)", U)], pitch=40, top=70)
    b.gnd_pin("J3.SH")
    b.Cap("C9", "100 nF*", 1760, 1180); b.gnd_pin("C9.2", 10)
    b.text(1960, 1270, "↔ Netzwerkkabel zum Switch (mit oder ohne PoE) / NR_Com", 15, "#444", weight="bold")

    # ================================================================== PoE
    b.block(2400, 660, 0, 0, "PoE: Strom aus dem Netzwerkkabel")
    M("U5", 2420, 720, 250, "U5 PoE-Modul", "802.3at, 5 V ≥ 2,5 A*",
      [("VA1", "", "VA1", U), ("VA2", "", "VA2", U), ("VB1", "", "VB1", U), ("VB2", "", "VB2", U)],
      [("VP", "", "+5 V", U), ("VN", "", "−VDC", U), ("ADJ", "", "ADJ", "nc")], pitch=40, top=70)
    b.gnd_pin("U5.VN")
    b.Cap("C14", "220 µF", 2760, 820, polar=True); b.gnd_pin("C14.2", 10)
    b.text(2420, 1000, "Gleichrichter, Erkennung, Klasse und", 12, "#555")
    b.text(2420, 1016, "Wandler 48 V → 5 V (galvanisch getrennt)", 12, "#555")
    b.text(2420, 1032, "stecken im Modul.", 12, "#555")

    # ================================================================== NETZE
    N = b.net
    N("J1VB", "J1.VBUS", "F1.1")
    N("+5V_USB", "F1.2", "D1.A", "U6.IN1")
    N("+5V_POE", "U5.VP", "C14.1", "U6.IN2")
    N("POE_A+", "J3.VAP", "U5.VA1"); N("POE_A−", "J3.VAN", "U5.VA2")
    N("POE_B+", "J3.VBP", "U5.VB1"); N("POE_B−", "J3.VBN", "U5.VB2")
    N("CC1", "J1.CC1", "R1.1"); N("CC2", "J1.CC2", "R2.1")
    N("PLED", "R3.2", "D2.A")
    N("ILIM", "U1.ILIM", "R4.1")
    N("FLED", "R5.2", "D3.A")
    N("FAULT", "D3.K", "U1.FLT")
    N("DOCK_5V", "U1.OUT", "C2.1", "J2.V5")
    N("DK_SCK", "J2.SCK", "U4.IO1"); N("DK_MOSI", "J2.MOSI", "U4.IO2")
    N("DK_MISO", "J2.MISO", "U4.IO3"); N("DK_CS", "J2.CS", "U4.IO4")
    N("DOCK_SCK", "U4.O1", "U3.SCLK")
    N("DOCK_MOSI", "U4.O2", "U3.MOSI")
    N("DOCK_MISO", "U4.O3", "R6.1")
    N("W_MISO", "R6.2", "U3.MISO")
    N("DOCK_CS", "U4.O4", "U3.SCS", "R7.2")
    N("RST", "U3.RST", "R8.2", "C5.1")
    N("XI", "U3.XI", "Y1.X1", "C6.1")
    N("XO", "U3.XO", "Y1.X2", "C7.1")
    N("ETH_TXP", "U3.TXP", "J3.TDP", "R9.2")
    N("ETH_TXN", "U3.TXN", "J3.TDN", "R10.2")
    N("ETH_RXP", "U3.RXP", "J3.RDP", "R11.2")
    N("ETH_RXN", "U3.RXN", "J3.RDN", "R12.2")
    N("RX_CT", "R11.1", "R12.1", "C8.1")
    N("J3_RCT", "J3.RCT", "C9.1")
    N("LINKLED", "U3.LINK", "R13.1"); N("LINK_K", "R13.2", "J3.LGK")
    N("ACTLED", "U3.ACT", "R14.1"); N("ACT_K", "R14.2", "J3.LYK")
    N("EXRES", "U3.EXRES", "R15.1")
    N("TOCAP", "U3.TOCAP", "C10.1")
    N("V12", "U3.V12", "C11.1")
    N("AVDD", "U3.AVDD", "L1.1", "C13.1")
    N("+5V", "U6.OUT", "C1.1", "R3.1", "U1.IN", "U1.EN", "R5.1", "U2.VIN", "U2.EN", "C3.1")
    N("+3V3", "U2.VOUT", "C4.1", "R9.1", "R10.1", "J3.TCT", "R7.1", "R8.1", "U3.VDD", "C12.1", "L1.2",
      "J3.LGA", "J3.LYA")

    local_first = [n for n in b.nets if b.nets[n] and n not in ("+5V", "+3V3")]
    local_first.sort(key=lambda n: len(b.nets[n]) * 1000 + spread(b, n))
    order = local_first + ["+5V", "+3V3"]
    order = [n for n in priority if n in order] + [n for n in order if n not in priority]
    b.route_all(order)

    b.note(60, 1420, [
        "So funktioniert die Station:",
        "• Strom: Netzteil 5 V / 3 A an J1 ODER PoE über das Netzwerkkabel (U5). U6 wählt die Quelle,",
        "  beide gleichzeitig sind erlaubt. U1 gibt die 5 V strombegrenzt an den Federstift „5 V“.",
        "  Kurzschluss an den offenen Kontakten (z. B. Schlüssel) → U1 begrenzt, rote LED D3 leuchtet.",
        "• DET liegt in der Station auf GND. Das Bodypack erkennt daran, dass es steckt (U16 P11).",
        "• Der W5500 läuft ständig. /SCS hat einen Pull-up (R7), ohne Bodypack ist der Chip also abgewählt.",
        "• Das Bodypack (C5) spricht den W5500 über SPI an: GP11 SCK, GP12 MOSI, GP1 MISO, GP0 CS.",
        "  /INT bleibt offen (Abfrage-Modus). Reset beim Einschalten über R8/C5, zusätzlich Software-Reset per SPI.",
    ], size=14)
    b.note(1260, 1420, [
        "Vor dem Layout prüfen (*):",
        "• Abschluss TX/RX (R9–R12, C8, C9) und Mittelanzapfungen lt. WIZnet-Referenz",
        "  und Datenblatt der gewählten RJ45-Buchse.",
        "• R_ILIM für ca. 2,5 A (Lader zieht bis 2 A), Quarz-Lastkapazitäten.",
        "• Pogo-Reihenfolge spiegelbildlich zum Bodypack; GND und DET zuerst Kontakt.",
        "• Bodypack ohne Strom an einer Station mit Strom (und umgekehrt): keine",
        "  Rückspeisung über die SPI-Leitungen (ggf. Serienwiderstände erhöhen).",
        "• PoE-Leistung: Laden mit 2 A + Station ≈ 12 W → PoE+ (802.3at) nötig.",
        "  An einem 802.3af-Switch R_ILIM auf ca. 1,5 A setzen: der Lader im Bodypack",
        "  passt seinen Strom dann von selbst an (er regelt bei sinkender Spannung herunter).",
        "• RJ45-Buchse muss PoE-fähig sein (Mittelanzapfungen der Kabelseite herausgeführt).",
    ], size=14)
    return b.render(), b.failed


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
    path = os.path.join(out, "schaltplan-basisstation-linien.svg")
    with open(path, "w", encoding="utf-8") as f:
        f.write(svg)
    print("geschrieben:", path)
    print("nicht geroutet (Verweise):", len(failed))
    for n, p in failed:
        print("  ", n, p)
