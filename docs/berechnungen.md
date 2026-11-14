# Bodypack-Platine V0.3: Berechnungen und Rauschbetrachtung

Stand 14.11.2026 (Komplettprüfung gegen den fertigen EasyEDA-Schaltplan, siehe Abschnitt 15). Alle Werte beziehen sich auf den Linien-Plan (`docs/img/schaltplan-platine-v1-linien.svg`)
und die Datenblätter in `Platine/Datenblätter/`. Werte mit „ca.“ sind Abschätzungen, keine Messungen.

---

## 1. Ergebnis in Kürze

| Bereich | Ergebnis |
|---|---|
| Laden, Akku, Schutz | passt mit **R3 160 Ω** (2,2 A Eingang, weil der TPS2116 nur 2,5 A verträgt), 2 A Laden, Schutz löst bei 2,6–4,4 A aus |
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
- **F1 1812L300:** 3 A Haltestrom. Der Lader begrenzt den Eingang auf 2,2 A, die Sicherung löst also nur bei einem Fehler aus.
- **U15 TPS2116, Vorrang Station:** VIN1 = Station. Umschaltschwelle = 1,0 V × (100 k + 33 k) / 33 k = **4,03 V**
  (mit der Toleranz der Referenz 0,92–1,08 V: 3,7–4,35 V). Liegt die Station an (5 V), lädt sie; sonst USB-C.
- **CH334F:** 12-MHz-Quarz mit CL 12 pF. Der Chip hat ca. 16 pF je Pin eingebaut, in Reihe ca. 8 pF plus Leiterbahn,
  passt. PSELF 10 kΩ → GND = busversorgt.

## 3. Lader BQ25895, Akku, Schutz

- **Eingangsstrom R3 = 160 Ω (geändert, war 130 Ω):** I = K_ILIM / R = 355 / 160 = **2,22 A** (Grenzen 320…390 → 2,0…2,44 A).
  Mit 130 Ω wären bis zu 3,0 A möglich gewesen, der Umschalter TPS2116 davor verträgt aber höchstens 2,5 A.
  Per I2C wird zusätzlich gesetzt: Ladestrom **2 A**, Eingangsstrom an der Station 2,2 A
  (dort erkennt der Lader an D+/D− kein Netzteil und würde sonst nur 500 mA nehmen).
- **Spule L1 2,2 µH, YSPI0530-2R2M:** Sättigung 8 A, Dauerstrom 5 A. Spitze beim Laden ca. 2,5 A + 0,5 A Welligkeit. Passt.
- **TS-Teiler R6 5,23 kΩ / R7 30,1 kΩ + NTC 103AT:** Das sind die Werte aus dem TI-Datenblatt für den
  JEITA-Bereich 0–60 °C. Der NTC muss **thermisch an den Zellen** sitzen.
- **Schutz DW01A + FS8205A:** Die Überstrom-Schwelle des DW01A liegt bei 130–170 mV. Die beiden FETs haben zusammen 39–50 mΩ
  → Abschaltung bei **2,6–4,4 A** (typ. 3,8 A). Die Systemlast aus dem Akku bleibt unter ca. 1,2 A.
- **PMID-Kondensator C4: 22 µF (geändert, war 10 µF).** Das Datenblatt verlangt mind. 8,2 µF wirksam, ein 10-µF-Keramikkondensator hat bei 5 V nur noch ca. 6 µF.
- **MAX17048:** CTG und QSTRT an GND, VDD = CELL. Passt.
- **Laufzeit (Abschätzung, neu gerechnet):** 2× 18650 à 3,4 Ah × 3,6 V = 24,5 Wh, davon ca. 22 Wh nutzbar.
  Verbrauch: C5 mit WLAN im Dauerempfang ca. 0,4 W, S3-Board mit Display ca. 0,65 W, Analogteil mit Wandlerverlusten ca. 0,45 W,
  LEDs und Kleinteile ca. 0,1 W → **ca. 1,6 W → ca. 13–14 h**. Mit Phantom 48 V ca. +0,35 W → **ca. 11 h**.
  Mit gedimmtem Display entsprechend mehr. Das sind Schätzwerte, die Messung am ersten Gerät zählt.

## 4. 3,3 V: TPS63021

- Fest 3,3 V, **FB an VOUT**, **PS/SYNC an VIN** (fester PWM-Betrieb, kein Burst-Modus, damit kein Brummen im Hörbereich).
- Last: C5 bis ca. 0,35 A beim Senden, Expander, Codec-Digitalteil, Pull-ups → **ca. 0,4–0,6 A Spitze**. Der TPS63021 kann 2 A.
- L2 1,5 µH (SLO0420H1R5MTT, Sättigung 6 A, Strombegrenzung des Wandlers 4 A), Eingang 2× 10 µF, Ausgang 3× 22 µF wie im Datenblatt.
- **VINA (Pin 1): R10 100 Ω von VSYS und neu C13 100 nF nach GND.** Das Datenblatt empfiehlt 100 nF (max. 220 nF) für die
  Versorgung der Regelung. Ohne den Kondensator wirkt R10 nicht als Filter.
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
- **REF (Pin 18), AVDD (Pin 24) und DVDD (Pin 29): je 10 µF** direkt an den Pins (C19, C20, C21), so wie in der TI-Beispielschaltung.
  Die internen Regler brauchen laut Datenblatt mind. 1 µF. **C21 war mit 100 nF zu klein** und ist jetzt 10 µF.
  IOVDD (Pin 6): C17 10 µF. LDOIN (Pin 26): C18 100 nF + C86 1 µF.
- **Reihenfolge:** IOVDD liegt immer an, LDOIN nur, wenn die Analogversorgung läuft. Die Firmware hält den Codec im Reset,
  solange ANA_EN aus ist, und richtet ihn nach dem Einschalten neu ein.

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
- **Q4 AO3422 (geändert, war Si2308BDS):** 55 V, max. 0,2 Ω bei 2,5 V Gate-Spannung. Das Gate bekommt nur VSYS (3,3–4,2 V).
  Der Si2308BDS ist erst ab 4,5 V spezifiziert und darf laut Datenblatt eine Schwellspannung bis 3 V haben.

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
- **Schutz (geändert):** R19/R20 **47 Ω** + BAV99 (D3/D4) **nach GND** statt an ±5 V, Erklärung in Abschnitt 15. HF-Filter
  C25 470 pF (zwischen den Adern) und C26/C27 100 pF (gegen GND) gegen WLAN-Einstrahlung.
- **Eingangswiderstand für das Mikrofon:** ca. 8 kΩ (2 × 6,81 kΩ ∥ 2 × 10 kΩ). Üblich sind ≥ 2 kΩ. Passt.
- **Verstärkung INA163:** G = 1 + 6 kΩ / 200 Ω = **31 (+29,8 dB)**.
- **Rauschen INA163 bei G = 31:** e = √(1² + (60 / 31)²) nV/√Hz ≈ **2,2 nV/√Hz** (Eingang 1 nV/√Hz, Ausgangsstufe 60 nV/√Hz).
  R19 + R20 (2 × 47 Ω) rauschen mit 1,25 nV/√Hz dazu.
  Zum Vergleich: Ein 150-Ω-Mikrofon rauscht selbst schon mit 1,6 nV/√Hz.
- **Teiler R23/R24:** 1,2 k / (4,7 k + 1,2 k) = 0,203 (**−13,8 dB**). Er passt den höchsten Pegel des INA163
  (laut Datenblatt mind. ±3,0 V an ±5 V) an den Eingang des Codecs an (0,61 V Spitze, Grenze ca. 0,7 V bei PGA 0 dB).
- **Gesamtrauschen (Abschätzung, 20 Hz–20 kHz):**
  - INA163 mit R19/R20: 2,5 nV/√Hz × √20 kHz ≈ 0,36 µV
  - Codec-PGA (ca. 1,5–2 µV am Codec-Eingang) auf den Mikrofoneingang umgerechnet: ÷ (31 × 0,203) ≈ 0,25–0,32 µV
  - zusammen ≈ **0,46 µV ≈ −124,6 dBu** (mit 150-Ω-Mikrofon −123,7 dBu). Das ist rauscharm. Die 47 Ω kosten ca. 0,6 dB
    gegenüber 10 Ω. Ohne den Teiler wäre es ca. 3 dB besser, dann würde der Codec aber schon bei mittleren Pegeln übersteuern.
- **Aussteuerungsgrenze am Mikrofoneingang:** 3,0 V / 31 ≈ 97 mV Spitze ≈ **68 mV eff. (−21 dBu)**.
  - Dynamisches Headset (BPHS1, ca. 1,8 mV/Pa): sehr laute Sprache 110 dB SPL ≈ 11 mV → **viel Reserve**.
  - Elektret-/Kondensatormikro (ca. 10 mV/Pa): 110 dB SPL ≈ 63 mV → passt knapp. **Wer sehr laut direkt ins
    Kondensatormikro spricht, kann übersteuern.** Abhilfe ohne neue Platine: RG1 auf 680 Ω (+20 dB) tauschen.
- **Pegel im Betrieb (BPHS1):** 2–4 mV × 31 × 0,203 = 13–25 mV am Codec → PGA ca. +30 dB → ca. −12 dBFS.

## 9. Kopfhörer OPA1622

- Codec-LINE-Ausgang ca. 0,5 V eff. → Verstärkung 1 + 1 k / 1 k = **2** → **1 V eff.**
- An 32 Ω (mit den 10 Ω in Reihe): **18 mW**; an 300 Ω: **3,1 mW** (für Broadcast-Headsets laut genug). Mit ±5 V sind bis ca. 2,5 V eff. möglich.
- Koppel-C 2,2 µF mit 47 kΩ: untere Grenzfrequenz **1,5 Hz**.
- Ausgangswiderstand R31/R32 10 Ω: schützt bei Kurzschluss beim Einstecken und hält den Verstärker stabil.
- EN (Pin 8) über den Expander: erst einschalten, wenn der Codec läuft → kein Knacken. **Neu R92 10 kΩ von HP_EN nach GND**,
  damit der Verstärker beim Einschalten aus bleibt (der Expander zieht seine Pins sonst über 100 kΩ auf 3,3 V).
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

- Erledigt: LM5155 gerechnet (Abschnitt 7), 2,8″-Board geprüft (Abschnitt 14), Komplettprüfung (Abschnitt 15).

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

## 15. Komplettprüfung vom 14.11.2026

Geprüft wurde der fertige EasyEDA-Schaltplan (`Platine/Schematic/`, als Textliste in `docs/netzliste-schaltplan-2026-11-14.txt`)
Pin für Pin gegen die Datenblätter in `Platine/Datenblätter/`. Alle Blöcke wurden mit den Werten aus dem Schaltplan neu gerechnet.

### 15.1 Muss geändert werden

| Nr. | Bauteil | Änderung | Grund |
|---|---|---|---|
| 1 | **C21** | 100 nF → **10 µF** | DVDD des Codecs ist der Ausgang eines internen Reglers. Laut Datenblatt mind. 1 µF, TI nimmt 10 µF. Mit 100 nF kann der Regler schwingen. |
| 2 | **R3** | 130 Ω → **160 Ω** | Der TPS2116 verträgt höchstens 2,5 A. Mit 130 Ω darf der Lader bis 3,0 A ziehen, mit 160 Ω max. 2,44 A. |
| 3 | **R90, R91, R92** (neu) | je 10 kΩ von P48_SW (U10 Pin 4), P48_EN (U10 Pin 9) und HP_EN (U10 Pin 6) nach GND | Der TCA9555 zieht nach dem Einschalten alle Pins über 100 kΩ auf 3,3 V, bis die Firmware läuft. Dadurch wären der 48-V-Wandler, der Phantom-Schalter und der Kopfhörerverstärker bei jedem Start kurz an. |
| 4 | **D3, D4** (Verdrahtung) und **R19, R20** | BAV99: Pin 1 und Pin 2 an **GND** statt an ±5 V. R19/R20 10 Ω → **47 Ω** | siehe 15.2 |
| 5 | **C13** (neu) | 100 nF von U6 Pin 1 (VINA) nach GND | Datenblatt TPS63021. Ohne Kondensator ist R10 kein Filter. |

### 15.2 Warum die Schutzdioden am Mikrofoneingang an GND müssen

- Mit Phantomspeisung sind C23/C24 (47 µF) auf 48 V geladen. Wird eine Ader gegen Masse kurzgeschlossen
  (Klinkenstecker einstecken, defektes Kabel), springt die Verstärkerseite des Kondensators auf −48 V.
- Die Dioden leiten diese Ladung ab: 47 µF × 47 V = **2,2 mC**.
- **Bisher** floss sie in die −5-V-Schiene. Die hat nur ca. 25–30 µF und der Regler kann keinen Strom aufnehmen.
  Rechnerisch wäre die Schiene auf ca. **−33 V** gezogen worden. Das liegt über den Grenzwerten von INA163, TPS65131 und OPA1622.
- **Jetzt** fließt sie über die Diode direkt nach GND. Die Versorgung merkt davon nichts.
- R19/R20 begrenzen den Strom: mit 10 Ω wären es 4,7 A Spitze, die BAV99 verträgt 1 A für 1 ms. Mit **47 Ω** sind es 1,0 A Spitze,
  nach 2 ms auf ein Drittel abgeklungen. Das liegt innerhalb der Datenblattangabe.
- Die Dioden begrenzen den Eingang auf ±0,6 V. Das Nutzsignal ist höchstens ±0,1 V, die Dioden leiten dabei nur einige nA.
- Die Phantom-Widerstände R17/R18 bekommen im Kurzschluss je 0,34 W. Deshalb **Bauform mit mind. 0,5 W** (z. B. 2010).

### 15.3 Empfohlen

| Bauteil | Änderung | Grund |
|---|---|---|
| **Q4** | Si2308BDS → **AO3422** (gleiche Bauform SOT-23) | Gate bekommt nur 3,3–4,2 V. Der Si2308BDS ist erst ab 4,5 V spezifiziert, der AO3422 ab 2,5 V. |
| **C4** | 10 µF → **22 µF** (25 V) | BQ25895 verlangt an PMID mind. 8,2 µF wirksam. |
| R70, R72 (optional) | 470 k → 523 k, 590 k → 649 k (ergibt ±6,5 V) | Die LDOs haben dann 1,5 V statt 0,94 V Reserve und unterdrücken den Wandler-Rippel etwas besser. Funktioniert auch ohne. |
| DOCK_DET (optional) | 1 kΩ in Reihe zwischen J7 Pin 5 und U16 Pin 14 | Der Federkontakt liegt offen, der Widerstand schützt den Expander vor statischer Entladung. |

### 15.4 Wo es im Betrieb Probleme geben könnte

| Punkt | Was passieren kann | Abhilfe |
|---|---|---|
| **Akkus falsch herum oder ungleich geladen** | Die zwei Zellen liegen direkt parallel. Wird eine Zelle verpolt eingelegt, schließt sie die andere kurz (mehrere 10 A). Auch eine volle neben einer leeren Zelle gleicht sich mit vielen Ampere aus. Der DW01A sitzt dahinter und schützt davor nicht. | Halter mit mechanischem Verpolschutz nehmen, Polung groß beschriften, immer zwei gleiche, gleich geladene Zellen einlegen. Sicherer: je Zelle eine rückstellbare Sicherung (3 A) in Reihe. |
| **48 V bei fast leerem Akku** | Unter ca. 3,4 V braucht der Wandler über 87 % Tastverhältnis, der LM5155 schafft ca. 88–92 %. Die 48 V können dann absinken. | Firmware schaltet P48 unter 3,4 V ab und meldet das. |
| **LEDs bei leerem Akku** | SK6812MINI-E ist ab 3,7 V spezifiziert, VSYS sinkt bis 3,3 V. Farben werden schwächer, Blau zuerst. | hinnehmen oder Helligkeit in der Firmware begrenzen |
| **Schnelllade-Netzteile** | Der BQ25895 versucht nach dem Einstecken von sich aus, ein Netzteil auf 9/12 V hochzuschalten (MaxCharge). Übliche Netzteile reagieren darauf nicht. Der Eingang (TPS2116, CH334F) verträgt aber nur 5 V. | Firmware löscht beim Start HVDCP_EN und MAXC_EN (Register 02). |
| **Laden an der Station** | Ohne USB-Datenleitungen erkennt der Lader kein Netzteil und nimmt nur 0,5 A. | Firmware setzt bei DOCK_DET den Eingangsstrom auf 2,2 A. |
| **Übersteuern mit lautem Kondensatormikro** | Grenze 68 mV eff. am Eingang. | RG1 auf 680 Ω (+20 dB) bestücken, wenn es stört. |
| **Analogversorgung aus, Codec am I2C** | Ist ANA_EN aus, hat der Codec nur IOVDD. Er kann dann den I2C-Bus stören, an dem auch U10 und der Lader hängen. | Firmware hält den Codec im Reset, solange ANA_EN aus ist. |
| **Wärme** | TPS2116 ca. 0,2 W, BQ25895 ca. 0,8 W beim Laden mit 2 A, FS8205A ca. 0,2 W. | Massefläche und Durchkontaktierungen unter den Pads, sonst unkritisch. |

### 15.5 Geprüft und in Ordnung

| Block | Ergebnis |
|---|---|
| USB-C J1, R1/R2 5,1 kΩ, F1 3 A, U1 USBLC6-2P6 | Pins und Werte stimmen. |
| U2 CH334F | Pinbelegung stimmt (V5 Pin 19, VDD33 Pin 20, PSELF Pin 18 über 10 kΩ an GND). OVCUR# und RESET# dürfen offen bleiben (interne Pull-ups). Quarz ohne externe Kondensatoren ist richtig (16 pF eingebaut). |
| U15 TPS2116 | MODE an VIN1, Teiler 100 k / 33 k → Station hat ab 4,03 V Vorrang. Ohne Station wird automatisch USB genommen. |
| U3 BQ25895 | Alle Pins wie im Datenblatt: CE und OTG an GND, DSEL und INT mit 10 kΩ an 3,3 V, REGN 4,7 µF, BTST 47 nF, TS-Teiler 5,23 k / 30,1 k + NTC 10 k, SYS 2× 10 µF, BAT 10 µF. |
| U4 DW01A, Q1 FS8205A | Pins stimmen (Q1: 1 = S1, 3 = S2, 4 = G2, 6 = G1). R8 100 Ω, C9 100 nF, R9 1 kΩ wie im Datenblatt. Ladeschluss 4,2 V liegt unter der Schutzschwelle 4,25 V. |
| U5 MAX17048 | CTG und QSTRT an GND, VDD = CELL, ALRT an U16 (Pull-up im Expander). |
| U6 TPS63021 | FB an VOUT, PS/SYNC und EN an VIN. L2 6 A Sättigung bei 4 A Begrenzung. |
| U7 TPS65131 | ±5,96 V. VIN über 100 Ω / 100 nF, VREF 220 nF, CP 10 nF, CN 4,7 nF, PSP/PSN an GND, D5/D6 richtig herum. Möglich wären ca. 0,7 A (plus) und 0,4 A (minus), gebraucht werden ca. 70 mA. |
| U17/U18 TPS7A4901/3001 | +5,02 V / −4,99 V. EN an IN ist bei beiden zulässig. NR/SS 10 nF, 10 nF parallel zum oberen Teilerwiderstand, Ausgang 10 µF. |
| U19 LP5907-3.3 | Eingang 5,02 V (erlaubt bis 5,5 V), 1 µF am Ein- und Ausgang. |
| U8 LM5155 + Verdoppler | BIAS = VCC = VSYS ist ab 2,97 V erlaubt. Werte wie in Abschnitt 7. Dioden und Kondensatoren richtig herum, alle Spannungsfestigkeiten reichen. |
| U9 TLV320AIC3204 | I2C-Modus (SPI_SELECT an GND), MCLK an GND (Takt aus BCLK), LDO_SELECT an IOVDD, Reset mit 10 kΩ Pull-up. Bis auf C21 in Ordnung. |
| U11 INA163 | Pins stimmen, SENSE an VO, REF an GND, RG1 200 Ω zwischen Pin 3 und 12, je 10 µF + 100 nF an V+ und V−. |
| U13 OPA1622 | Pin 3 an GND, Pad an V−, Verstärkung 2, je 10 µF + 100 nF. |
| U10/U16 TCA9555 | Adressen 0x20 und 0x27 stimmen. |
| Taster, Drehgeber, LEDs | TS-1187A: Pin 1/2 und 3/4 je verbunden, richtig angeschlossen. LED-Kette mit 33 Ω, 3,3-V-Daten reichen bei VDD = VSYS. |
| J5, J6, J7, U14 | Belegung stimmt. |

### 15.6 Was die Firmware sicherstellen muss

1. Beim Start des C5: U10 einrichten (P48_SW, P48_EN, HP_EN als Ausgang auf 0), danach BQ25895: HVDCP_EN = 0, MAXC_EN = 0, Ladestrom 2 A, Watchdog bedienen oder abschalten.
2. Bei DOCK_DET: Eingangsstrom 2,2 A setzen.
3. Phantom 48 V: nur auf Wunsch einschalten, Codec dabei 0,5 s stumm, unter 3,4 V Akkuspannung abschalten.
4. Reihenfolge Audio: ANA_EN an → 50 ms warten → Codec-Reset lösen und einrichten → 0,5 s warten → HP_EN an.
5. Abschalten bei ca. 3,2 V Akkuspannung (Ship-Mode), der DW01A trennt erst bei 2,5 V.
