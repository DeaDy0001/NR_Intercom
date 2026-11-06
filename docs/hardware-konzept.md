# Hardware-Konzept Bodypack (Version 2)

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
| Display + Bedienung | **Waveshare ESP32-S3 AMOLED**, gesteckt (vorhanden) |
| Aufteilung | **C5 = Funk, Audio, Opus, NR_Com-Protokoll, die 4 Tasten**; **S3 = Display, Drehgeber, Menü, Akku-Anzeige** |
| Mikrofon | Neutrik-**Kombibuchse XLR/6,35 mm** → rauscharmer symmetrischer Mic-Vorverstärker (+20…+60 dB) → Codec |
| Phantomspeisung | per Software **Aus / P48** (optional P12/P24), Boost-Wandler auf der Platine |
| Kopfhörer | **6,35 mm Stereo**, eigener Kopfhörer-Verstärker (treibt 32–600 Ω laut genug) |
| Codec | TI **TLV320AIC3204** (ADC/DAC + DSP für EQ/AGC/Limiter + Mithörton) |
| Akku | **2× 18650 parallel** (ca. 22–24 Wh), ca. **15–22 h** |
| Netz | mind. **2–3 APs** für 20+ Bodypacks, eigene 5-GHz-SSID, Roaming |

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
                         │   Drehgeber 1+2 ──────────────► [ESP32-S3 AMOLED, gesteckt] ◄──────┘
                         │                                  Display, Menü, Akku-Anzeige
                         │   USB-C (Laden) ─► Lader mit Power-Path ─► 2× 18650 parallel
                         │                       ├─► Buck-Boost 3,3 V ─► C5, Codec, Logik
                         │                       ├─► ±5 V (rauscharm) ─► Mic-Vorverstärker, Kopfhörer-Verstärker
                         │                       ├─► Boost 48 V ─► Phantomspeisung
                         │                       └─► VSYS ─► S3-Board
                         │   Akku-Anzeige (MAX17048), Soft-Power-Taster
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

**ESP32-C5-Zero (Funk + Audio):**

| Funktion | Pins |
|---|---|
| I2S zum Codec (BCLK, WS, DOUT, DIN) | 4 |
| I2C (Codec, I/O-Expander für Phantom, Verstärkung, Kopfhörer-Mute) | 2 |
| UART zum S3 | 2 |
| Taste 1–4 | 4 |
| LED-Kette (SK6812) | 1 |
| **Summe** | **13 von 17** (USB bleibt frei zum Flashen) |

**ESP32-S3 AMOLED (Display):** UART zum C5 (2), Reset/Boot des C5 (2), Drehgeber (6), I2C für Akku-Anzeige und Lader (vorhandener Bus GPIO39/40), Soft-Power (2).
Ohne I2S fallen alle bisherigen Pin-Konflikte weg (PSRAM-Pins 33–37, Display-Reset GPIO17 bleiben frei).

---

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
- Buck-Boost 3,3 V für die Logik, rauscharmes ±5 V für den Analogteil, Boost 48 V für die Phantomspeisung
- Das S3-Board wird über VSYS gespeist, sein BAT-Anschluss bleibt leer
- Soft-Power-Taster (lang drücken: Ein/Aus, sauberes Abmelden)

---

## 9. Latenz-Budget (unverändert)

Heute ca. **165–190 ms** Mund → Ohr, davon **100 ms im Server** (Jitterbuffer 60 ms + Mixer 40 ms).
Mit Server-Tuning und 5 GHz sind ca. **75–100 ms** realistisch. Der eigene Mithörton hat ca. 0 ms (im Codec).

---

## 10. Vorgehen

1. **Prototyp auf dem Tisch:**
   - ESP32-C5-Zero + vorhandenes S3-AMOLED-Board
   - zum Start der WM8960 oder ein AIC3204-Testboard
   - einfacher Mic-Vorverstärker für das BPHS1
   - Messen: Opus-Rechenlast und Speicher auf dem C5, Latenz, Paketverlust, Stromverbrauch, Funk-Reichweite
2. **Server:** PlainTransport-Erweiterung und Latenz-Tuning durch den Com-Dev
3. **Trägerplatine V1** in EasyEDA, gefertigt und bestückt bei JLCPCB (Analogteil mit Abschirmung)
4. **Gehäuse:** Antenne oben, Akkufach, Gürtelclip, Buchsen unten oder seitlich
5. **Funknetz** aufbauen und messen (siehe [funknetz-planung.md](funknetz-planung.md)), dann Feldtest mit mehreren Geräten
