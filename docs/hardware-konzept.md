# Hardware-Konzept Bodypack (Stand 4)

> **Berechnungen aller Blöcke inkl. Rauschbetrachtung:** [berechnungen.md](berechnungen.md)
>
> **Stand 4 (November 2026):** Bauteile gegen die Datenblätter geprüft (Ordner `Datenblätter/`), Spannungsversorgung
> überarbeitet (±5 V mit TPS65131 + LDOs, 48 V mit Verdoppler), Schaltplan V0.3.
>
> **Stand 3:** Display wird das **Waveshare ESP32-S3-Touch-LCD-2.8B** (2,8″, 480 × 640, Touch).
> Akku bleibt **2× 18650** (flacher LiPo erst in einer späteren Version). Neu: Messwerte vom Prototyp (Abschnitt 1a),
> Pin-Plan für das 2,8″-Board (Abschnitt 7) und die **Basisstation** mit LAN und Laden (Abschnitt 11).

Anforderungen:
- guter Empfang, **20+ Bodypacks**, möglichst geringe Latenz
- **normale Headsets** inkl. Broadcast-Headsets: Mikrofon über **XLR/6,35-mm-Kombibuchse**, Kopfhörer über **6,35 mm Stereo**, **Phantomspeisung einstellbar**
- erstes Test-Headset: **Audio-Technica BPHS1**
- **4 frei programmierbare Tasten** + **2 Drehgeber** für die Bedienung am Display
- Akku für **einen ganzen Tag** (2–3× 18650)
- **eigene Trägerplatine**, Module werden nur gesteckt
- WLAN-Netz wird mitgeplant → [funknetz-planung.md](funknetz-planung.md)

---

## 1. Ergebnis in Kürze

| Thema | Entscheidung |
|---|---|
| Funk | WLAN **5 GHz, Wi-Fi 6** mit **ESP32-C5** |
| Funk-Modul | **Waveshare ESP32-C5-Zero(-M)**, gesteckt, mit **IPEX-Anschluss für externe Antenne** ✅ |
| Display + Bedienung | **Waveshare ESP32-S3-Touch-LCD-2.8B** (2,8″ IPS 480 × 640, Touch), gesteckt. Bisheriges 1,91″-AMOLED nur noch zum Testen |
| Aufteilung | **C5 = Funk, Audio, Opus, NR_Com-Protokoll, die 4 Tasten**; **S3 = Display, Drehgeber, Menü, Akku-Anzeige** |
| Mikrofon | Neutrik-**Kombibuchse XLR/6,35 mm** → rauscharmer symmetrischer Mic-Vorverstärker (+20…+60 dB) → Codec |
| Phantomspeisung | per Software **Aus / P48** (optional P12/P24), Boost-Wandler auf der Platine |
| Kopfhörer | **6,35 mm Stereo**, eigener Kopfhörer-Verstärker (treibt 32–600 Ω laut genug) |
| Codec | TI **TLV320AIC3204** (ADC/DAC + DSP für EQ/AGC/Limiter + Mithörton) |
| Akku | **2× 18650 parallel** (ca. 22–24 Wh), ca. **15–22 h** |
| Netz | mind. **2–3 APs** für 20+ Bodypacks, eigene 5-GHz-SSID, Roaming |
| Server-Anbindung | **NR_Com Device-Gateway** (HTTP + RTP/Opus) → [device-gateway-api.md](device-gateway-api.md) |
| Updates | **über WLAN** (OTA, zwei Programmbereiche, automatischer Rücksprung) mit `NR-Intercom.bat` |
| Später | **Basisstation**: Bodypack einstecken → Audio über LAN statt WLAN, gleichzeitig laden (Abschnitt 11) |

---

## 1a. Gemessen am Prototyp (C5-Zero + WM8960 + S3-AMOLED, gegen NR_Com)

| Messung | Wert | Bewertung |
|---|---|---|
| Mikrofon → eigener Kopfhörer (Mithören) | ca. 5–6 ms | ✅ klingt natürlich |
| CPU C5 ohne Talk / mit Talk (Opus 48 kbit/s, Komplexität 2) | **20 % / 65 %** | ⚠️ ausreichend, aber nur ca. 35 % Reserve |
| Opus-Encoder / Decoder pro 10-ms-Paket | ca. 3,9 ms / 2,3 ms | Engpass ist der Encoder (kein DSP im C5) |
| Freier RAM C5 im Betrieb | ca. 32 KB (Spitzen bis nahe 0, behoben durch kleinere Puffer) | ⚠️ knapp, wird weiter beobachtet |
| Jitter im WLAN (5 GHz, RSSI −65…−80 dBm) | 5–10 ms | ✅ |
| Flash-Belegung C5 | 1,46 MB von 1,875 MB je Programmbereich | ✅ |

Weitere CPU-Reserve ist per Software möglich (Opus-Komplexität 0, häufig genutzter Code im internen RAM).
Falls einmal deutlich mehr Rechenleistung nötig wird (z. B. Rauschunterdrückung mit zwei Mikrofonen), ist die
saubere Lösung eine spätere Version mit **ESP32-P4 + C5** (P4 für Display, Audio und Signalverarbeitung, C5 bleibt 5-GHz-Funk).

---

## 2. Das ESP32-C5-Zero: passt das?

**Ja, für die Aufgabe „Funk + Audio“ passt es sehr gut.**

| | ESP32-C5-Zero |
|---|---|
| Chip | ESP32-C5HF4 (RISC-V 240 MHz, 384 KB SRAM, 4 MB Flash, **kein PSRAM**) |
| Funk | 2,4 + **5 GHz Wi-Fi 6**, Bluetooth 5 |
| Antenne | Keramikantenne **und IPEX**, umschaltbar per Software (GP26) ✅ |
| Größe | 28 × 18 mm, Version „-M“ mit Stiftleisten zum Stecken |
| Freie Pins | GP0–GP12 + Lötpads GP23/24/25/28 (GP13/14 = USB, GP26 = Antennen-Umschalter, GP27 = RGB-LED) |

**Vorteile:**
- Die gesamte Funktechnik sitzt fertig und geprüft auf dem Modul. Auf der Trägerplatine ist **kein HF-Layout** nötig, das ist beim Selbstbau der heikelste Teil.
- Der IPEX-Anschluss erlaubt eine richtige externe Antenne oben am Gehäuse. Das ist für den Empfang der wichtigste Punkt.
- Klein, günstig, steckbar und austauschbar.

**Einschränkung:** **Kein PSRAM.** Für WLAN, TLS, Opus-Encoder und -Decoder, RTP und Jitterbuffer reichen die 384 KB voraussichtlich
(Schätzung ca. 200–280 KB). Für zusätzlich eine Display-Oberfläche auf demselben Chip wird es aber eng (siehe Abschnitt 3).
Das wird als erste Messung am Prototyp geprüft.

---

## 3. Braucht es wirklich einen zweiten ESP32 nur für das Display?

**Technisch nicht zwingend. Für Version 1 ist es aber die bessere Wahl.**

| | **A: C5 + S3-Display (empfohlen für V1)** | B: nur ein C5 + einfaches SPI-Display |
|---|---|---|
| Audio-Sicherheit | ✅ Audio und Funk laufen allein auf dem C5. Display-Updates können keine Aussetzer verursachen | ⚠️ ein Rechenkern für Audio, Funk und Oberfläche gleichzeitig |
| Speicher | ✅ Die Oberfläche läuft auf dem S3 mit 8 MB PSRAM | ⚠️ C5-Zero ohne PSRAM ist für eine Oberfläche zu knapp. Man bräuchte ein C5-Board mit PSRAM |
| Pins | ✅ reichlich | ⚠️ 17 Pins für I2S, I2C, Display, 4 Tasten, 2 Drehgeber → I/O-Expander nötig |
| Display | ✅ schönes AMOLED, schon vorhanden | ein SPI-TFT-Modul (z. B. 1,9–2″), eigenes Gehäuse-Fenster |
| „Nur stecken“ | ✅ beide Module gesteckt | ✅ |
| Strom | ca. +0,2–0,3 W | ✅ etwas sparsamer |
| Kosten | ca. +25 € | ✅ |
| Software | zwei Firmwares + einfache UART-Verbindung | eine Firmware |

**Empfehlung:** Version 1 baut auf **A**. Wenn die Messungen am Prototyp zeigen, dass der C5 genug Reserve hat
(Rechenlast Opus und WLAN), kann eine spätere, günstigere **Version 2 mit nur einem Chip** kommen.
Die Firmware wird von Anfang an so aufgebaut, dass die Oberfläche später auf den C5 umziehen kann.

---

## 4. Systemaufbau

```
                         ┌───────────────────────── Trägerplatine (EasyEDA) ─────────────────────────┐
 Kombibuchse XLR/6,35 ──►│ Schutz + Phantom (Aus/P48) ─► Mic-Vorverstärker ─► TLV320AIC3204 ◄─I2S/I2C─┐│
 (Mikrofon)              │                                          (Codec)                         ││
 6,35 mm Stereo ◄────────│◄── Kopfhörer-Verstärker ◄─────────────────┘                               ││
 (Kopfhörer)             │                                                                           ▼│
                         │   Taster 1–4 (frei programmierbar, mit RGB-LED) ──────────────► [ESP32-C5-Zero]──IPEX──► Antenne
                         │                                                                    │ UART
                         │   Drehgeber 1+2 ─► I/O-Expander ─I2C─► [ESP32-S3-Touch-LCD-2.8B] ◄──┘
                         │                                  Display + Touch, Menü, Akku-Anzeige
                         │   USB-C (Laden) ─► Lader mit Power-Path ─► 2× 18650 parallel
                         │                       ├─► Buck-Boost 3,3 V ─► C5, Codec, Logik
                         │                       ├─► ±5 V (rauscharm) ─► Mic-Vorverstärker, Kopfhörer-Verstärker
                         │                       ├─► Boost 48 V ─► Phantomspeisung
                         │                       └─► VSYS ─► S3-Board
                         │   Akku-Anzeige (MAX17048), Soft-Power-Taster
                         │   Andock-Kontakte (später): 5 V Laden + LAN zur Basisstation
                         └────────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Audio-Teil (normale Headsets und Broadcast-Headsets)

### Test-Headset Audio-Technica BPHS1
- Mikrofon: **dynamisch**, Niere → **keine Phantomspeisung**, braucht viel Verstärkung (ca. +40…+55 dB)
- Kopfhörer: **65 Ω**, ca. 100 dB/mW, Stereo
- Kabel BPCB1: Mikrofon auf **XLR-Stecker**, Kopfhörer auf **6,35-mm-Klinke**. Das passt direkt an Kombibuchse + 6,35-mm-Buchse ✅

### Mikrofon-Eingang
- **Neutrik-Kombibuchse** (XLR-3 + 6,35 mm TRS, symmetrisch, Printmontage)
- **Phantomspeisung** (IEC 61938): per Software **Aus / P48** über 6,81-kΩ-Präzisionswiderstände.
  Optional **P12/P24** über einen zweiten Widerstandssatz, der umgeschaltet wird. Langsames Hoch- und Runterfahren gegen Knacken.
  Der Ausgang ist während des Umschaltens stumm. Die Spannung kommt aus einem kleinen Boost-Wandler (max. 10 mA).
- **Schutz:** 63-V-Koppelkondensatoren, Klemmdioden (gegen den Entladestoß beim Ein- und Ausstecken mit Phantom), ESD-Schutz
- **Vorverstärker:** rauscharmer Instrumentenverstärker für Mikrofone (Klasse THAT1512 / INA163),
  Verstärkung in Stufen umschaltbar (+20 / +40 / +60 dB). Die Feineinstellung macht der Codec digital.
  Damit sind dynamische Broadcast-Mikrofone **und** Kondensator-Mikrofone möglich.
- **Wichtig, WLAN-Brummen:** Bei hoher Mikrofonverstärkung kann das WLAN-Senden als Brummen oder Knattern einstreuen.
  Gegenmittel: symmetrischer Eingang, HF-Filter direkt an der Buchse, Abschirmhaube über dem Vorverstärker, durchgehende Massefläche und die Antenne weit weg vom Analogteil.

### Kopfhörer-Ausgang
- **6,35 mm Stereo** (Neutrik-Printbuchse)
- Kopfhörer-Verstärker mit ±5-V-Versorgung (z. B. **OPA1622**): ca. 3 Veff, laut genug auch an 300–600 Ω.
  Der BPHS1 (65 Ω) braucht für laute Umgebungen ca. 1,5 Veff.
- Stereo-Nutzung als Option: z. B. **Kanal A links / Kanal B rechts** oder überall Mono-Mix. Das ist in der Firmware einstellbar.
- Begrenzer (Limiter) im Codec-DSP als Gehörschutz gegen laute Knackser

### Codec TLV320AIC3204
- 48 kHz, I2S. Der **C5 ist Takt-Master**, es gibt nur eine Taktquelle.
- DSP: Hochpass (Trittschall, Popp-Geräusche), EQ, AGC/Kompressor fürs Mikrofon, Limiter für den Kopfhörer
- **Mithörton** direkt im Codec ohne Verzögerung (Pegel einstellbar)

---

## 6. Bedienung

| Element | Angeschlossen an | Funktion |
|---|---|---|
| **Taste 1–4** | **C5** (direkt, schnellste Reaktion) | **frei programmierbar**: Talk Kanal X (Tasten- oder Halte-Modus), Ruf (Call), Mikro stumm, Antworten usw. Je Taste eine **RGB-LED** (Talk/Ruf/Status), alle zusammen über 1 Pin (SK6812-LED-Kette) |
| **Drehgeber 1 + 2** (mit Druck) | **S3** | Bedienung am Display: Menü, Kanal wählen, Lautstärke pro Kanal, Einstellungen |

Die Tastenbelegung wird im Gerät gespeichert. Sie wird am Display über die Drehgeber eingestellt, später optional auch über NR_Com.

---

## 7. Pin-Belegung (Planung)

**ESP32-C5-Zero (Funk + Audio).** Prototyp = so wie heute auf dem Tisch verdrahtet, Platine = Schaltplan Variante B:

| Funktion | Prototyp | Platine (V0.2) |
|---|---|---|
| I2S zum Codec: BCLK, WS, DIN (Mic), DOUT (Kopfhörer) | GP4, GP5, GP6, GP8 | gleich |
| I2C (Codec, I/O-Expander U10) | GP9 (SDA), GP10 (SCL) | gleich |
| UART zum S3 | GP2 (TX) → S3 RX, GP3 (RX) ← S3 TX | gleich (TX/RX gekreuzt) |
| Taste 1–4 | GP0, GP1, GP12, GP11 | **über U10 TCA9555** (P10–P13, 0x20, alle 5 ms abgefragt) |
| Basisstation (SPI zum W5500) | – | **GP11 SCK, GP12 MOSI, GP1 MISO, GP0 CS** |
| Antennen-Umschalter / RGB-LED-Kette | GP26 / GP27 | gleich |
| frei | GP7, GP23, GP24, GP25 | gleich (Strapping-Pins GP7/GP25 bleiben unbeschaltet) |

Die Tasten wandern auf der Platine an den I/O-Expander (der hat interne Pull-ups, Abfrage ohne Interrupt reicht
für Tasten völlig). Dadurch werden vier „normale“ Pins frei, und die Basisstation braucht keine Strapping-Pins.

**ESP32-S3-Touch-LCD-2.8B (Display + Bedienung).** Das RGB-Display belegt fast alle Pins des S3, auf den
Stiftleisten sind nur noch wenige frei. Deshalb laufen die Drehgeber über einen I/O-Expander am I2C-Bus:

| Funktion | Pin am 2,8″-Board | Hinweis |
|---|---|---|
| UART zum C5 | GPIO43 (TX) → C5 GP3, GPIO44 (RX) ← C5 GP2 | Konsole des S3 läuft über USB, UART0-Pins sind frei |
| I2C (gemeinsam mit Touch GT911 und den Bausteinen des Boards) | GPIO15 (SDA), GPIO7 (SCL) | dazu auf der Trägerplatine: **U16 TCA9555** (0x27) für Drehgeber, Ein/Aus-Taste, Lader-/Akku-Alarm und Station-Erkennung, **MAX17048** (0x36), **BQ25895** (0x6A) |
| Interrupt vom TCA9555 (Drehgeber bewegt) | GPIO16 | Drehgeber werden bei jeder Änderung über I2C gelesen |
| Akku-Messung des Boards | GPIO4 | nicht genutzt, die Akku-Anzeige kommt vom MAX17048 |
| PSRAM | GPIO33–37 | belegt, nicht verwenden |

**Vor dem Platinen-Layout mit dem Schaltplan des 2,8″-Boards prüfen:**
- I2C-Adressen der Bausteine auf dem Board (Touch, I/O-Expander, ggf. Lagesensor/RTC). Der BQ25895 (0x6A) darf
  nicht mit einem Lagesensor (oft 0x6A/0x6B) kollidieren, sonst einen zweiten Bus oder einen anderen Lader nehmen.
- **Stromversorgung des Boards:** Hat das Board einen eigenen Laderegler am BAT-Anschluss, das Board **nicht** über
  BAT speisen (der Lader würde sonst gegen unsere Zellen arbeiten), sondern über den 5-V- bzw. VCC-Pin.
- Wie das Display initialisiert wird (ST7701 über SPI, evtl. über den I/O-Expander des Boards) → Firmware.

**Firmware für das 2,8″-Board (nach dem Eintreffen):** RGB-Display-Treiber (ST7701), Touch (GT911),
Oberfläche im Hochformat 480 × 640 (mehr Platz für Kanäle), Drehgeber über den TCA9555 (U16).

## 8. Akku und Laufzeit

| Verbraucher (Schätzung, wird gemessen) | ca. |
|---|---|
| ESP32-C5, WLAN aktiv ohne Stromsparen | 0,35–0,5 W |
| ESP32-S3-Board + AMOLED (dunkle Oberfläche) | 0,25–0,4 W |
| Codec + Vorverstärker + Kopfhörer-Verstärker | 0,1–0,2 W |
| Phantomspeisung (nur bei Kondensator-Mikrofon) | 0–0,3 W |
| Wandlerverluste | ca. 0,1–0,15 W |
| **Summe** | **ca. 0,9–1,4 W** |

| Akku | nutzbar | Laufzeit |
|---|---|---|
| **2× 18650 (à ca. 3400 mAh) parallel** | ca. 22 Wh | **ca. 15–24 h** |
| 3× 18650 parallel | ca. 33 Wh | ca. 23–36 h |

→ 2 Zellen reichen für einen Tag. Mit dauerhaft eingeschalteter Phantomspeisung und Kälte wird es knapper.
Der dritte Zellen-Platz kann auf der Platine **vorgesehen** werden, eine Bestückung ist dann optional.

Stromversorgung:
- USB-C (5 V/3 A, ESD)
- Lader mit Power-Path, z. B. TI BQ25895 (per I2C, 2–3 A)
- NTC an den Zellen
- MAX17048 für die Prozentanzeige
- Buck-Boost 3,3 V für die Logik: **TPS63021** (fest 3,3 V, PS/SYNC an VIN = fester PWM-Betrieb, rauscharm)
- ±5 V für den Analogteil: **TPS65131** erzeugt ±6 V (Boost + Inverter, PWM), danach glätten die rauscharmen LDOs
  **TPS7A4901** (+5 V) und **TPS7A3001** (−5 V). Der ursprünglich geplante LM27762 geht nicht: Er macht +OUT mit
  einem Linearregler direkt aus VIN, aus 3,5–4,2 V Akku also nie +5 V.
- 48 V Phantom: Ein einstufiger Boost von 3,3 V auf 48 V bräuchte ca. 93 % Tastverhältnis, der **LM5155** kann laut
  Datenblatt 90 % (min) bis 93 % (typ). Deshalb boostet er nur auf **ca. 24 V**, ein **Spannungsverdoppler**
  (2 Dioden + 2 Kondensatoren, 100 V) macht daraus 48 V. FB misst direkt an den 48 V (470 k / 10 k, Referenz 1,0 V).
  Geschaltet wird mit **Q2 ZXMP10A13F** (P-FET −100 V) und **Q3 MMBTA42** (NPN 300 V), Gate geschützt mit 15-V-Zener.
- Ladestrom-Grenze am BQ25895: **R_ILIM 160 Ω** (ca. 2,2 A, weil der TPS2116 max. 2,5 A verträgt). In der Basisstation erkennt der Lader an D+/D− kein
  Netzteil (nur 500 mA), deshalb setzt die Firmware beim Andocken den Eingangsstrom per I2C hoch.
- Das S3-Board wird über VSYS gespeist, sein BAT-Anschluss bleibt leer
- Soft-Power-Taster (lang drücken: Ein/Aus, sauberes Abmelden)

---

## 9. Latenz-Budget (unverändert)

Heute ca. **165–190 ms** Mund → Ohr, davon **100 ms im Server** (Jitterbuffer 60 ms + Mixer 40 ms).
Mit Server-Tuning und 5 GHz sind ca. **75–100 ms** realistisch. Der eigene Mithörton hat ca. 0 ms (im Codec).

---

## 10. Vorgehen (Stand)

1. ✅ **Prototyp auf dem Tisch:** C5-Zero + WM8960 + S3-AMOLED: Audio, WLAN, Opus, Intercom mit NR_Com in beide Richtungen,
   Bedienung am Display, Updates über WLAN, Werkzeug `NR-Intercom.bat`
2. ✅ **Server:** Device-Gateway in NR_Com (Branch des Com-Devs), offen: grüner Sprech-Punkt im Browser
3. **2,8″-Board:** bestellen, Schaltplan prüfen (Abschnitt 7), Display-Firmware portieren
4. **Mikrofon-Eingang testen:** AIC3204-Testboard + Mic-Vorverstärker für das BPHS1 (das WM8960-Board hat keinen Headset-Eingang)
5. **Trägerplatine V1** in EasyEDA (Werte mit * gegen die Datenblätter prüfen), gefertigt und bestückt bei JLCPCB,
   Andock-Kontakte gleich mit vorsehen (Abschnitt 11)
6. **Gehäuse:** Antenne oben, Akkufach 2× 18650, Gürtelclip, Buchsen unten oder seitlich, Kontaktfläche für die Basisstation
7. **Funknetz** aufbauen und messen (siehe [funknetz-planung.md](funknetz-planung.md)), dann Feldtest mit mehreren Geräten
8. **Basisstation** (Abschnitt 11)

---

## 11. Basisstation: LAN statt WLAN und gleichzeitig laden

Idee: Bei Produktion und Livebetrieb steckt das Bodypack in einer Station am Platz (Regie, Tonpult, Kamera).
Dann läuft das Audio **über ein Netzwerkkabel** statt über WLAN, und der Akku wird **gleichzeitig geladen**.
Das entlastet auch das Funknetz für die mobilen Bodypacks.

### Warum nicht einfach ein USB-Netzwerkadapter?
- Der **C5 kann kein USB-Host** sein. Er hat nur USB für Konsole und Flashen, einen USB-Netzwerkadapter kann er nicht betreiben.
- Der **S3** könnte zwar USB-Host sein, aber sein USB-Anschluss ist auf dem 2,8″-Board die Buchse fürs Flashen.
  Host und Laden über dieselbe Buchse ist schwierig (die Stromrichtung auf VBUS wäre falsch herum), und das Audio
  müsste dann vom S3 über die UART zum C5. Das geht, ist aber kompliziert und fehleranfällig.

### Empfohlen: Andock-Kontakte mit einem kleinen Netzwerk-Chip
```
 Bodypack (Trägerplatine)                          Basisstation
 ┌───────────────────────────┐   Federkontakte   ┌────────────────────────────────┐
 │ C5  SCK  ─────────────────┼── ● ●  ──────────┼─ W5500 (Netzwerk-Chip mit PHY) ─┼─ RJ45 ─► Switch / NR_Com
 │     MOSI ─────────────────┼── ●              │                                │
 │     MISO ─────────────────┼── ●              │                                │
 │     CS   ─────────────────┼── ●              │                                │
 │ Erkennung (gesteckt?) ────┼── ●  ──────────┼─ GND                           │
 │ Lader-Eingang ◄───────────┼── ● 5 V ◄──────┼─ Netzteil 5 V / 3 A            │
 │ GND ──────────────────────┼── ● ●  ──────────┼─ GND                           │
 └───────────────────────────┘                  └────────────────────────────────┘
```
- **W5500** (bewährter Netzwerk-Chip mit eingebautem 100-Mbit-Ethernet, wird von ESP-IDF direkt unterstützt) sitzt
  **in der Station**, am Bodypack sind nur 4 SPI-Leitungen + Erkennung + 5 V + GND auf Kontaktflächen.
  So bleibt das Bodypack leicht, und ohne Station verbraucht der Netzwerk-Teil keinen Strom.
- Am C5-Zero laufen die SPI-Leitungen über **GP11 (SCK), GP12 (MOSI), GP1 (MISO), GP0 (CS)**. Die waren im Prototyp
  die Tasten; auf der Platine hängen die Tasten am I/O-Expander U10. Der W5500 läuft ohne Interrupt-Leitung
  (Abfrage-Modus, das kann der ESP-IDF-Treiber).
- **Erkennung:** Der DET-Kontakt liegt in der Station auf GND und geht im Bodypack an U16 (P11, interner Pull-up).
- **USB-C und Station funktionieren beide.** Ein Strom-Umschalter **U15 TPS2116** wählt zwischen den 5 V der Station
  (VIN1, **Vorrang**: MODE an VIN1, PR1 über 100 k / 33 k → schaltet ab ca. 4 V) und USB-C-VBUS (VIN2) und gibt das
  Ergebnis (VBUS_CHG) an den Lader. Die Daten von USB-C (Hub → Flashen
  und Konsole beider ESP32) sind davon unabhängig, man kann also auch in der Station per USB flashen.
  Beide 5-V-Quellen gleichzeitig sind erlaubt, der TPS2116 verhindert Rückströme zwischen ihnen.
- **Kontakte:** vergoldete Flächen am Bodypack, Federstifte (Pogo-Pins) in der Station, mechanisch geführt.
  Serienwiderstände und ESD-Schutz an den SPI-Leitungen im Bodypack.

### Schaltplan der Station
[`img/schaltplan-basisstation-linien.svg`](img/schaltplan-basisstation-linien.svg) (Linien-Plan, erzeugt mit
`tools/schematic_station.py`):
- **Strom: USB-C oder PoE.** Entweder ein USB-C-Netzteil 5 V / 3 A (nur Strom, mit Sicherung und TVS-Diode) oder
  **PoE über das Netzwerkkabel**: Ein fertiges PoE-Modul (U5, IEEE 802.3at, 5 V ≥ 2,5 A, galvanisch getrennt)
  macht aus den 48 V am Kabel 5 V. Ein Umschalter **TPS2116** (U6, derselbe Baustein wie im Bodypack) wählt die
  Quelle, beide gleichzeitig sind erlaubt. Dann reicht **ein Kabel für Strom, Laden und LAN**.
- **Leistung bei PoE:** Laden mit 2 A plus Station sind ca. 12 W, das braucht **PoE+ (802.3at)** am Switch.
  An einem älteren 802.3af-Switch (ca. 13 W am Kabel, nach dem Wandler weniger) den Strom an den Kontakten auf
  ca. 1,5 A begrenzen (R_ILIM). Der Lader im Bodypack regelt bei sinkender Spannung selbst herunter, er lädt
  dann nur langsamer.
- **RJ45:** Die Buchse muss PoE-fähig sein (Mittelanzapfungen der Kabelseite herausgeführt, für Modus A und B).
- Die 5 V gehen über einen strombegrenzten Lastschalter **TPS2557** an den Federstift, damit ein Kurzschluss an den
  offenen Kontakten nichts zerstört (rote LED = Fehler).
- **3,3 V:** AP2112K-3.3 nur für den W5500.
- **LAN:** W5500 mit 25-MHz-Quarz, Abschluss der Leitungspaare lt. WIZnet-Referenz, RJ45-Buchse mit Übertrager und LEDs.
  /SCS hat einen Pull-up, ohne Bodypack ist der Chip abgewählt. /INT bleibt offen (Abfrage-Modus).
- **Federstifte:** SCK, MOSI, MISO, CS, 5 V, DET (in der Station auf GND), GND, mit ESD-Schutz TPD4E05U06.

### Was die Firmware macht (ohne Ab- und Anmelden)
- Station erkannt → Netzwerk über den W5500 starten (DHCP). Danach **setzt das Bodypack seine Sitzung fort**
  (`POST /device/v1/resume` mit dem bisherigen Token und der neuen Adresse), es meldet sich **nicht** ab und wieder an.
  Für die anderen Benutzer passiert nichts: kein Verlassen/Beitreten, kein Ton, der Benutzer bleibt online und in seinen
  Kanälen. Nur das Audio läuft ab dann über die neue Adresse. Die Umschaltpause liegt geschätzt unter 1 s.
- Aus der Station genommen → zurück auf WLAN, wieder fortsetzen (resume).
- Der Server hält eine **Gnadenfrist**: Meldet sich ein Gerät 15 s nicht, ruht die Sitzung nur (kein Ereignis an die
  Browser). Erst nach 60 s (einstellbar) wird es wirklich abgemeldet. Ein Funkloch, ein AP-Wechsel oder ein Neustart
  des Bodypacks (Login mit derselben Geräte-ID übernimmt die Sitzung) bleibt so für alle unsichtbar.
- Auf dem Bodypack ist das schon eingebaut (auch für Funklöcher und neue IP im WLAN). **Am Server muss der Com-Dev
  das umsetzen:** [`docs/prompt-com-dev-6.md`](prompt-com-dev-6.md), API in [`device-gateway-api.md`](device-gateway-api.md).
