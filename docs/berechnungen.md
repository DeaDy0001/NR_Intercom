# Bodypack-Platine V0.3: Berechnungen und Rauschbetrachtung

Stand 13.11.2026. Alle Werte beziehen sich auf den Linien-Plan (`docs/img/schaltplan-platine-v1-linien.svg`)
und die Datenblätter in `Platine/Datenblätter/`. Werte mit „ca.“ sind Abschätzungen, keine Messungen.

---

## 1. Ergebnis in Kürze

| Bereich | Ergebnis |
|---|---|
| Laden, Akku, Schutz | passt (2,7 A Eingang, 2 A Laden, Schutz löst ab ca. 3 A aus) |
| 3,3 V (TPS63021) | passt, große Reserve (2 A möglich, ca. 0,6 A Spitze nötig) |
| ±6 V → ±5 V | passt, LDOs mit ca. 0,9 V Spannungsreserve, **zusätzlich C83/C84 für weniger Rauschen** |
| 48 V Phantom | **geändert:** 500 kHz (R80 44,2 k), **neu R86 2 k** (Flankenausgleich), Kompensation R85 1 k / C75 150 nF / **neu C87 1 nF**. Vorher drohte Schwingen bei halber Taktfrequenz |
| Mikrofon-Kette | **Eingangsrauschen ca. −125 dBu** (20 Hz–20 kHz, unbewertet). Studio-Vorverstärker liegen bei ca. −128 dBu |
| Kopfhörer | 1 V eff. an 32 Ω (ca. 30 mW), sehr rauscharm |
| ESP32-S3-Board | Speisung über **Pin 14 VCC** (nicht 5V), BQ25895 an den I2C-Bus des C5 (Adresse 0x6A ist am S3-Bus schon belegt), /INT von U16 an **GP4** |
| **Neu für weniger Rauschen** | **U19 LP5907-3.3**: eigene, saubere 3,3 V für den Codec-Analogteil (statt aus dem Schaltwandler) |

---

## 2. USB-C, Umschalter, Hub

- **CC1/CC2 5,1 kΩ:** Ein USB-C-Netzteil liefert damit 5 V (bis 3 A je nach Netzteil).
- **F1 1812L300:** 3 A Haltestrom. Der Lader begrenzt den Eingang auf 2,7 A, die Sicherung löst also nur bei einem Fehler aus.
- **U15 TPS2116, Vorrang Station:** VIN1 = Station. Umschaltschwelle = 1,0 V × (100 k + 33 k) / 33 k = **4,03 V**
  (mit der Toleranz der Referenz 0,92–1,08 V: 3,7–4,35 V). Liegt die Station an (5 V), lädt sie; sonst USB-C.
- **CH334F:** 12-MHz-Quarz mit CL 12 pF. Der Chip hat ca. 16 pF je Pin eingebaut, in Reihe ca. 8 pF plus Leiterbahn,
  passt. PSELF 10 kΩ → GND = busversorgt.

## 3. Lader BQ25895, Akku, Schutz

- **Eingangsstrom R3 = 130 Ω:** I = K_ILIM / R = 355 / 130 = **2,73 A** (Grenzen 320…390 → 2,46…3,0 A).
  Per I2C wird zusätzlich gesetzt: Ladestrom **2 A**, Eingangsstrom an der Station ≥ 2,5 A
  (dort erkennt der Lader an D+/D− kein Netzteil und würde sonst nur 500 mA nehmen).
- **Spule L1 2,2 µH, YSPI0530-2R2M:** Sättigung 8 A, Dauerstrom 5 A. Spitze beim Laden ca. 2,5 A + 0,5 A Welligkeit. Passt.
- **TS-Teiler R6 5,23 kΩ / R7 30,1 kΩ + NTC 103AT:** Das sind die Werte aus dem TI-Datenblatt für den
  JEITA-Bereich 0–60 °C. Der NTC muss **thermisch an den Zellen** sitzen.
- **Schutz DW01A + FS8205A:** Die Überstrom-Schwelle des DW01A liegt bei ca. 150 mV. Die beiden FETs haben zusammen ca. 40–50 mΩ
  → Abschaltung bei **ca. 3–3,7 A**. Laden 2 A und Systemlast bis ca. 1,5 A liegen darunter.
- **MAX17048:** CTG und QSTRT an GND, VDD = CELL. Passt.
- **Laufzeit (Abschätzung):** 2× 18650 à 3,5 Ah = 7 Ah × 3,7 V ≈ 26 Wh.
  Verbrauch ohne Phantom ca. 1,3–1,6 W (Display 0,4–0,6 W, C5 mit WLAN 0,3–0,5 W, Analog 0,15 W, Verluste) →
  **ca. 16–20 h**. Mit Phantom 48 V (Kondensatormikro, ca. 7 mA) ca. +0,5 W → ca. 12–15 h.

## 4. 3,3 V: TPS63021

- Fest 3,3 V, **FB an VOUT**, **PS/SYNC an VIN** (fester PWM-Betrieb, kein Burst-Modus, damit kein Brummen im Hörbereich).
- Last: C5 bis ca. 0,35 A beim Senden, Expander, Codec-Digitalteil, Pull-ups → **ca. 0,4–0,6 A Spitze**. Der TPS63021 kann 2 A.
- L2 1,5 µH (SLO0420H1R5MTT, Sättigung 6 A), Eingang 2× 10 µF, Ausgang 3× 22 µF wie im Datenblatt.
- **Wichtig:** Diese 3,3 V sind „digital“ und tragen das Schaltrauschen (2,4 MHz) und die Stromstöße vom WLAN.
  Deshalb bekommt der **Analogteil des Codecs ab jetzt eigene 3,3 V** (siehe Abschnitt 6).

## 5. ±6 V und ±5 V (Analog)

- **TPS65131:** +6 V = 1,213 V × (1 + 470 k / 120 k) = **+5,96 V**; −6 V = −1,213 V × 590 k / 120 k = **−5,96 V**.
  Teiler-Strom ca. 10 µA (wie im Datenblatt empfohlen ≥ 5 µA).
- Takt ca. 1,38 MHz, PSP/PSN an GND = fester PWM-Betrieb.
- Spulenwelligkeit (plus): ΔI = V_IN × D / (f × L) = 3,7 V × 0,41 / (1,38 MHz × 4,7 µH) ≈ **0,23 A**. Spule
  SLO0420H4R7MTT (3 A Sättigung). Passt.
- **Last:** INA163 ca. 10 mA, OPA1622 ca. 5 mA Ruhe + bis ca. 45 mA Spitze an 32 Ω, Codec-LDO U19 ca. 15 mA →
  **ca. 70 mA Spitze je Seite**. Der TPS65131 schafft mehrere 100 mA.
- **LDOs TPS7A4901 / TPS7A3001:**
  - Ausgang +5 V: 1,185 V × (1 + 32,4 k / 10 k) = **5,02 V**; −5 V: −1,179 V × (1 + 32,4 k / 10 k) = **−5,00 V**.
  - Spannungsreserve 5,96 − 5,0 = **0,96 V** (Dropout typ. 0,26 V bei 100 mA). Passt.
  - **Rauschen:** NR/SS 10 nF (C81/C82) **und neu 10 nF parallel zum oberen Teilerwiderstand (C83 ∥ R75, C84 ∥ R77)**.
    Laut Datenblatt sinkt das Ausgangsrauschen damit auf ca. 15–20 µV eff. (10 Hz–100 kHz).
  - Unterdrückung des 1,38-MHz-Rippels des TPS65131: ca. 40 dB im LDO + LC-Wirkung der Ausgangskondensatoren.
- ADG1201 ist entfallen (V1 ohne umschaltbare Verstärkung).

## 6. Codec TLV320AIC3204

- I2C-Adresse 0x18, SPI_SELECT an GND (I2C), MCLK an GND. Der Takt kommt aus BCLK (C5 ist I2S-Master,
  48 kHz × 64 = 3,072 MHz) über die PLL im Codec.
- **Versorgung neu:**
  - IOVDD (Pin 6) = digitale 3,3 V.
  - **LDOIN (Pin 26) = 3V3A aus U19 LP5907-3.3**, gespeist aus den rauscharmen +5 V.
    Der LP5907 rauscht nur ca. 6,5 µV eff. und hängt nicht am Schaltwandler, an dem auch das WLAN-Funkmodul zieht.
  - Aus LDOIN erzeugt der Codec intern AVDD und DVDD (1,8 V). LDO_SELECT (Pin 30) an IOVDD = internen LDO für DVDD an.
- REF (Pin 18) 10 µF, AVDD (24) und DVDD (29) je 10 µF + 100 nF direkt an den Pins.

## 7. 48 V Phantom

- **Boost LM5155 auf ca. 24 V, dann Spannungsverdoppler auf 48 V.** Der Teiler R11/R12 misst direkt die 48 V.
- **Takt (geändert): R80 44,2 kΩ** → f ≈ 2,2·10¹⁰ / 44,2 k ≈ **500 kHz** (vorher 86,6 k / 254 kHz).
- Tastverhältnis bei VSYS 3,5 V: D = 1 − 3,5 / (24,4 + 0,4) ≈ **0,86**. Grenze des LM5155 bei 500 kHz:
  1 − 100 ns × 500 kHz = 0,95 bzw. ca. 0,90 (interne Grenze). Passt, die Reserve ist klein. Der BQ25895 hält VSYS
  auch bei leerem Akku auf mind. 3,5 V. Firmware: 48 V unter 3,4 V Akkuspannung abschalten.
- Last: P48 nach Norm max. 10 mA je Mikrofon, mit 2 × 6,81 kΩ Kurzschlussstrom max. 14 mA → **ca. 0,7 W**.
  Eingangsstrom ca. 0,27 A Mittelwert.
- Spule L5 22 µH: ΔI = 3,5 V × 0,86 / (500 kHz × 22 µH) ≈ **0,27 A** → Spitze ca. 0,41 A. Sättigung 2,0 A. Passt.

### Flankenausgleich (Slope Compensation), warum R86 neu ist

Bei Tastverhältnis über 50 % schwingt ein Stromregler bei halber Taktfrequenz, wenn die interne Rampe zu klein ist
(Datenblatt Gl. 8):

- Nötig: 0,5 × (V_LOAD + V_F − V_SUPPLY) / L × R_S × 1,2 = 0,5 × 21,3 V / 22 µH × 68 mΩ × 1,2 ≈ **39,5 kV/s**
- Vorher (254 kHz, ohne RSL): 40 mV × 254 kHz = **10,2 kV/s → zu wenig, Schwinggefahr.**
- Jetzt (500 kHz, R86 = RSL 2,0 kΩ, Maximum laut Datenblatt): 40 mV × 500 kHz + 30 µA × 2 kΩ × 500 kHz
  = 20 + 30 = **50 kV/s** → passt (optimal laut Gl. 9 wären ca. 54 kV/s).
- R86 sitzt zwischen CS (Pin 5) und dem Knoten R81 / Q4-Source. Der LM5155 schickt einen Rampenstrom aus CS, der an
  R86 die zusätzliche Rampe erzeugt.

### Strombegrenzung

- I_PEAK = (100 mV − 30 µA × 2 kΩ × 0,86) / 68 mΩ ≈ **0,71 A** (mit Toleranz min. ca. 0,6 A). Nötig sind ca. 0,41 A. Passt.
- Q4 Si2308BDS: 60 V, 0,19 Ω bei 4,5 V. Bei 500 kHz und 0,27 A ca. 30 mW Verluste.

### Regelschleife (Kompensation)

Stromgeregelter Boost, Rechnung nach Datenblatt (Fehlerverstärker Gm = 2 mA/V, R_O = 10 MΩ):

- Rückführung vom 48-V-Ausgang: 1 V / 48 V, der Verdoppler verdoppelt die 24 V → wirksam 1 / 24,4 am Boost-Knoten.
- Ausgangskapazität am Boost-Knoten (DC-Bias abgezogen): C71 ≈ 0,8 µF + C73 über den Verdoppler ×4 ≈ 2,4 µF → **ca. 3,2 µF**.
- Durchtrittsfrequenz (oberhalb des Lastpols, unabhängig von der Last):
  f_c = Gm × R85 × (1/24,4) × (1 − D) / (2π × R_S × C_out) = 2 mA/V × 1 kΩ × 0,041 × 0,14 / (2π × 68 mΩ × 3,2 µF)
  ≈ **8–9 kHz**.
- Rechter Halbebenen-Nullpunkt: f_RHP = R_Last (1 − D)² / (2π L) ≈ 840 Ω × 0,02 / (2π × 22 µH) ≈ **120 kHz** →
  f_c liegt weit darunter (Faustregel ≤ f_RHP / 5 = 24 kHz und ≤ f_SW / 10 = 50 kHz).
- **R85 1,0 kΩ + C75 150 nF:** Nullstelle 1 / (2π × 1 k × 150 n) ≈ **1,1 kHz** (f_c / 8).
- **C87 1 nF C0G** von COMP nach GND: Pol bei 1 / (2π × 1 k × 1 n) ≈ **160 kHz**, filtert Schaltstörungen.
- Phasenreserve ca. **75–80°**. Auch bei doppelter oder halber Verstärkung (Toleranzen, Kapazität) stabil.
- Ohne Mikrofon arbeitet der Wandler lückend (DCM), dort ist die Schleife von sich aus stabil.
- Die bisherigen Startwerte 10 k / 10 nF hätten f_c ≈ 85 kHz ergeben, nahe am RHP-Nullpunkt → wären instabil gewesen.

### Verdoppler, Teiler, Sanftanlauf

- **Verdoppler C71–C73 1 µF / 100 V (1206 X7R):** Bei 24–48 V bleiben ca. 0,4–0,8 µF übrig.
  Welligkeit an C73: ΔU = I / (f × C) = 14 mA / (500 kHz × 0,45 µF) ≈ **0,06 V**.
- Teiler R11 470 k / R12 10 k: 1,0 V × 48 = **48 V** (Toleranz ±1 % → 47,5–48,5 V; Norm 48 ± 4 V).
- Sanftanlauf C74 47 nF: t = 47 nF × 1 V / 10 µA ≈ **5 ms**.
- **WEBENCH wird nicht gebraucht**, die Werte oben sind nach Datenblatt gerechnet. Beim ersten Einschalten die 48 V
  mit dem Oszilloskop ansehen (Lastsprung Mikrofon an/aus): kein Klingeln, keine Schwingung bei 250 kHz.
- **Schalter Q2/Q3:** Gate von Q2 auf −15 V begrenzt (DZ1, max. ±20 V erlaubt). Strom durch R83:
  (48 − 15) V / 47 kΩ ≈ 0,7 mA. Q3 bekommt 0,26 mA Basisstrom (aus 3,3 V über 10 kΩ), schaltet sicher durch.
- **Filter R13 100 Ω + C16 47 µF:** Grenzfrequenz 34 Hz. Der 500-kHz-Rippel wird um ca. 80 dB gedämpft.
  Er wirkt außerdem als **Gleichtakt** auf das Mikrofon (beide Adern gleich), der INA163 unterdrückt ihn zusätzlich.

## 8. Mikrofon-Eingang und Rauschen

- **Phantomspeisung:** R17/R18 6,81 kΩ **0,1 %**. Die Toleranz ist wichtig: Je gleicher die Widerstände, desto weniger
  Störungen auf der 48-V-Leitung werden zu einem Nutzsignal. Bei 0,1 % ca. 60 dB Unterdrückung.
- **Koppel-Elkos C23/C24 47 µF / 63 V** mit R21/R22 10 kΩ: untere Grenzfrequenz **0,34 Hz**.
  Beim Ein-/Ausschalten der Phantomspeisung entsteht ein Knacken. Die Firmware schaltet den Codec dabei stumm (ca. 500 ms).
- **Schutz:** R19/R20 10 Ω + BAV99 an ±5 V gegen die Spitzen beim Einstecken mit Phantomspeisung. HF-Filter
  C25 470 pF (zwischen den Adern) und C26/C27 100 pF (gegen GND) gegen WLAN-Einstrahlung.
- **Eingangswiderstand für das Mikrofon:** ca. 8 kΩ (2 × 6,81 kΩ ∥ 2 × 10 kΩ). Üblich sind ≥ 2 kΩ. Passt.
- **Verstärkung INA163:** G = 1 + 6 kΩ / 200 Ω = **31 (+29,8 dB)**.
- **Rauschen INA163 bei G = 31:** e = √(1² + (60 / 31)²) nV/√Hz ≈ **2,2 nV/√Hz** (Eingang 1 nV/√Hz, Ausgangsstufe 60 nV/√Hz).
  Zum Vergleich: Ein 150-Ω-Mikrofon rauscht selbst schon mit 1,6 nV/√Hz.
- **Teiler R23/R24:** 1,2 k / (4,7 k + 1,2 k) = 0,203 (**−13,8 dB**). Er passt den höchsten Pegel des INA163
  (ca. ±3,5 V an ±5 V) an den Eingang des Codecs (ca. 0,7 V Spitze bei PGA 0 dB) an.
- **Gesamtrauschen (Abschätzung, 20 Hz–20 kHz):**
  - INA163: 2,2 nV/√Hz × √20 kHz ≈ 0,31 µV
  - Codec-PGA (ca. 1,5–2 µV am Codec-Eingang) auf den Mikrofoneingang umgerechnet: ÷ (31 × 0,203) ≈ 0,25–0,32 µV
  - zusammen ≈ **0,44 µV ≈ −125 dBu**. Das ist rauscharm. Ohne den Teiler wäre es ca. 3 dB besser, dann würde der
    Codec aber schon bei mittleren Pegeln übersteuern.
- **Aussteuerungsgrenze am Mikrofoneingang:** 3,5 V / 31 ≈ 113 mV Spitze ≈ **80 mV eff. (−20 dBu)**.
  - Dynamisches Headset (BPHS1, ca. 1,8 mV/Pa): sehr laute Sprache 110 dB SPL ≈ 11 mV → **viel Reserve**.
  - Elektret-/Kondensatormikro (ca. 10 mV/Pa): 110 dB SPL ≈ 63 mV → passt knapp. **Wer sehr laut direkt ins
    Kondensatormikro spricht, kann übersteuern.** Abhilfe ohne neue Platine: RG1 auf 680 Ω (+20 dB) tauschen.
- **Pegel im Betrieb (BPHS1):** 2–4 mV × 31 × 0,203 = 13–25 mV am Codec → PGA ca. +30 dB → ca. −12 dBFS.

## 9. Kopfhörer OPA1622

- Codec-LINE-Ausgang ca. 0,5 V eff. → Verstärkung 1 + 1 k / 1 k = **2** → **1 V eff.**
- An 32 Ω: 31 mW; an 300 Ω: 3,3 mW (für Broadcast-Headsets laut genug). Mit ±5 V sind bis ca. 2,7 V eff. möglich.
- Koppel-C 2,2 µF mit 47 kΩ: untere Grenzfrequenz **1,5 Hz**.
- Ausgangswiderstand R31/R32 10 Ω: schützt bei Kurzschluss beim Einstecken und hält den Verstärker stabil.
- EN (Pin 8) über den Expander: erst einschalten, wenn der Codec läuft → kein Knacken.
- **Pin 3 (GND) an GND, Pad (Pin 11) an V− (−5 V)!** So steht es im Datenblatt.

## 10. Bedienung, I2C, Station

- **Tasten:** 1 kΩ + 100 nF + Pull-up im TCA9555 (ca. 100 kΩ). Loslassen dauert ca. 10 ms → bei 5 ms Abfrage passt das.
- **Drehgeber:** 10 kΩ / 10 nF → 0,1 ms Entprellung, Rest in der Firmware.
- **LEDs SK6812MINI-E:** 33 Ω in der Datenleitung. Datenblatt ab 3,7 V, VSYS kann bis ca. 3,5 V sinken → Farben bei
  fast leerem Akku evtl. schwächer.
- **I2C am C5:** 4,7 kΩ Pull-ups, ca. 50 pF Leitung → Anstieg ca. 0,25 µs → 400 kHz möglich.
- **I2C am S3:** Pull-ups sitzen vermutlich auf dem 2,8″-Board (Touch). **Am Board-Schaltplan prüfen**, sonst
  2× 4,7 kΩ ergänzen.
- **Station (J7, 9-polig):** 2× 5 V und 2× GND (je 2 A je Pin) → 4 A Reserve bei 2,5 A. SPI über 33 Ω und die Federstifte
  bis ca. 10–20 MHz. Symmetrische Belegung: verdreht angesteckt passiert nichts.

---

## 11. Was sich durch diese Prüfung geändert hat

| Änderung | Grund |
|---|---|
| **U19 LP5907-3.3 + C85/C86 (1 µF)**, neu | Codec-Analogteil bekommt eigene, saubere 3,3 V aus +5VA statt aus dem Schaltwandler |
| **C83/C84 10 nF** parallel zu R75/R77, neu | weniger Rauschen der ±5-V-LDOs (laut Datenblatt) |
| **C81/C82 10 nF** an NR/SS | waren als „NR“/„SS“ ohne Nummer gezeichnet. Pro LDO **ein** NR/SS-Kondensator, die anderen zwei werden C83/C84 |
| **INA163 Pin 8 SENSE an Pin 9 VO** | ohne diese Verbindung arbeitet der Ausgang nicht korrekt |
| **OPA1622 Pin 3 GND an GND, Pin 11 (Pad) an V−** | laut Datenblatt |
| **BQ25895 STAT (Pin 4) offen** | Lade-LED R5/D1 entfällt, die Anzeige läuft über das Display |
| **J7 9-polig**, symmetrisch | 2× 5 V, 2× GND, Schutz gegen verdreht Anstecken |
| **TPD4E05U06:** nur Pins 1, 2, 4, 5 + GND 3/8 | 6, 7, 9, 10 sind NC (nur zum geraden Durchführen der Leitung im Layout) |

## 12. Hinweise fürs Layout (rauscharm)

1. **4-Lagen-Platine:** Oben Signale, Lage 2 durchgehende **GND-Fläche ohne Schlitze**, Lage 3 Versorgungen, unten Signale.
2. **Zonen trennen:** Schaltwandler (BQ25895, TPS63021, TPS65131, LM5155 + Verdoppler) auf eine Seite der Platine,
   Mikrofon-Eingang, INA163, Codec-Analogteil und Kopfhörer auf die andere. Abschirmhaube über Mic/Kopfhörer vorsehen.
3. **Schaltknoten klein halten:** Spule, Schalter, Diode und Ausgangskondensator jedes Wandlers dicht beieinander.
   Keine Analogleitungen unter oder neben SW-Knoten.
4. **Mikrofonleitungen** als Paar, symmetrisch und kurz. R17/R18, C23/C24, R19–R22 spiegelbildlich platzieren.
   RG1 direkt an Pin 3/12, SENSE direkt an VO.
5. **Entkoppel-Kondensatoren** (100 nF) direkt an die Pins, 10 µF daneben.
6. **Antenne des C5** weit weg vom Mikrofoneingang. Kein Kupfer unter der Antenne.
7. **48-V-Teil** mit ausreichend Abstand (≥ 0,5 mm) zu anderen Leitungen.
8. **Taktfrequenzen:** BQ25895 1,5 MHz, TPS63021 2,4 MHz, TPS65131 1,38 MHz, LM5155 500 kHz. Alles weit über
   dem Hörbereich, Mischprodukte entstehen nur bei schlechtem Layout. Deshalb Punkte 1–3 beachten.

## 13. Noch offen

- Erledigt: LM5155 gerechnet (Abschnitt 7), 2,8″-Board geprüft (Abschnitt 14).
- Kopfhörerbuchse J6: Typ wählst du selbst.

## 14. ESP32-S3-Touch-LCD-2.8B (Board-Schaltplan geprüft)

Quelle: `Platine/ESP32-S3_2.8-Touch-LCD/` (Schaltplan, Maßzeichnung, Datenblätter).

| Punkt | Ergebnis |
|---|---|
| **Versorgung** | Die 5V-Pins (12, 13) gehen auf den Lader ETA6098 des Boards und über eine Diode auf VCC. **Pin 14 VCC** ist direkt der Eingang des 3,3-V-Wandlers MP1605 (2,3–5,5 V). → **VSYS an Pin 14 VCC**, 5V, BAT, 3V3 offen. |
| USB-C am Board | Im Gerät nicht anstecken (würde über die Diode in VCC/VSYS zurückspeisen). Programmieren über unseren Hub (D+/D− am Stecker). |
| **I2C-Adressen am S3-Bus** (GP15/GP7, 10 k Pull-ups am Board) | TCA9554 0x20, GT911 0x5D/0x14, PCF85063 0x51, **QMI8658 0x6A**. Der BQ25895 hat ebenfalls 0x6A → **BQ25895 an den I2C-Bus des C5** (dort: Codec 0x18, U10 0x20). Keine weiteren Pull-ups nötig. |
| U5 MAX17048 (0x36), U16 TCA9555 (0x27) | bleiben am S3-Bus, keine Überschneidung. |
| **GP16** | ist am Board der Touch-Interrupt (TP_INT) → nicht benutzen. **/INT von U16 an GP4 (Pin 19).** GP4 hat am Board einen Teiler 200 k / 100 k (BAT_ADC), mit R60 10 k Pull-up unkritisch. |
| GP0 | Boot-Pin, frei lassen. GP33–37 = PSRAM, nicht benutzen. |
| TXD/RXD | GP43/GP44 (U0TXD/U0RXD). Das Boot-Protokoll des S3 erscheint beim Start am C5, die Firmware ignoriert es. |
| BQ_INT | jetzt an U10 P01 (Expander des C5). Der C5 schickt Ladezustand und Laderstatus per UART an den S3. |

**Maße für den Footprint (Draufsicht Trägerplatine, Ursprung = linke obere Ecke des Boards):**

- Board 47,90 × 66,70 mm, 4 Löcher M2 je 3,50 mm vom Rand (Abstand 40,90 × 59,70 mm).
- Stiftleiste 2 × 12, Raster 2,54 mm, Mitte der Leiste 6,30 mm vom Rand (2,80 mm neben der Lochmitte):
  Spalte Pins 1–12 (TXD-Seite) bei **x = 5,03 mm**, Spalte Pins 13–24 (SDA-Seite) bei **x = 7,57 mm**.
- Erste Reihe (Pin 1 GND / Pin 24 GND) bei **y = 10,00 mm**, letzte Reihe (Pin 12 / 13, 5V) bei y = 37,94 mm.
- Höhe gesamt 11,1 mm, Stiftleiste ca. 6 mm unter dem Board.
- Skizze: `docs/img/footprint-s3-2.8b.svg`.
