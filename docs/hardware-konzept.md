# Hardware-Konzept Bodypack: Analyse und Empfehlung

Anforderungen: **guter Empfang**, **viele Teilnehmer in einem Netz**, **möglichst geringe Latenz**,
Akku für **einen ganzen Tag** (2–3× 18650), **eigene Trägerplatine**, auf die nur noch das ESP32-Display-Board gesteckt wird.

---

## 1. Kurzfassung / Empfehlung

| Thema | Empfehlung |
|---|---|
| Funk | **WLAN bleibt** (NR_Com ist IP-basiert, kein Gateway nötig), aber **5 GHz und Wi-Fi 6** statt 2,4 GHz |
| Funkchip | **ESP32-C5** (Dual-Band 2,4/5 GHz, Wi-Fi 6) als Modul **ESP32-C5-WROOM-1U-N8R8** (externe Antenne, 8 MB PSRAM) auf der Trägerplatine |
| Display/UI | Das **Waveshare ESP32-S3 AMOLED** bleibt als Steck-Board und macht nur Oberfläche, Drehgeber und Menü |
| Aufteilung | **C5 = Funk + Audio + Opus + NR_Com-Protokoll**, **S3 = Display + Bedienung**, Verbindung über UART |
| Audio-Codec | **TI TLV320AIC3204** (stromsparend, rauscharm, Mic-PGA bis +47,5 dB, interner DSP für EQ/AGC/Mithörton) statt WM8960 |
| Akku | **2× 18650 parallel (1S2P)**, ca. 24 Wh → **ca. 17–22 h** Laufzeit. Ein dritter Akku ist nur für Reserve oder Kälte nötig |
| Laden | USB-C mit Schaltregler-Lader **mit Power-Path** (z. B. TI BQ25606/BQ25895), ca. 2–3 A, voll in ca. 3 h |
| Größter Latenz-Hebel | **Der Server**, nicht das Funknetz: Jitterbuffer 60 ms + Mixer 40 ms lassen sich auf ca. 20 + 10 ms senken |
| Netz | Eigene **Intercom-SSID nur auf 5 GHz**, mehrere APs mit Roaming (802.11k/v/r), WMM-Voice. Etwa **15–20 Bodypacks pro AP-Funkkanal** einplanen |

Fazit: Der ESP32 ist **nicht das Problem**. Das Problem ist **2,4 GHz**. Mit einem ESP32-C5 auf 5 GHz,
guter externer Antenne und einer sauber geplanten WLAN-Infrastruktur ist das ein tragfähiger Kompromiss.
Professionelle DECT-Systeme sind noch etwas robuster, lassen sich aber im Selbstbau praktisch nicht umsetzen (siehe Abschnitt 2).

---

## 2. Funk-Optionen im Vergleich

| Option | Empfang / Störfestigkeit | Viele Teilnehmer | Latenz (Funk) | Anbindung NR_Com | Aufwand | Bewertung |
|---|---|---|---|---|---|---|
| **A: ESP32-S3, WLAN 2,4 GHz (Wi-Fi 4)**, so wie jetzt | ⚠️ 2,4 GHz ist auf Events voll (Publikumshandys, Bluetooth, Funk-DMX, Video-Funk). Nur 3 überlappungsfreie Kanäle | ⚠️ 3 Kanäle × ca. 15 Geräte | 2–20 ms, unter Last mehr Jitter | ✅ direkt | gering | Nur für Proben und kleine Setups |
| **B: ESP32-C5, WLAN 5 GHz (Wi-Fi 6)** ✅ | ✅ deutlich freieres Band, in EU 4 Kanäle ohne DFS (36–48) plus viele DFS-Kanäle | ✅ skaliert über mehrere APs und Kanäle, OFDMA | 1–10 ms | ✅ direkt | mittel | **Empfehlung** |
| C: ESP32-P4 + C5/C6 | wie B bzw. A | wie B | wie B | ✅ | hoch | Mehr Rechenleistung, für Audio unnötig |
| D: DECT NR+ (z. B. Nordic nRF9151, 1,9 GHz) | ✅ eigenes, lizenzfreies Band | ✅ Mesh | niedrig | ❌ braucht eigene Basisstationen und ein Gateway zu NR_Com | sehr hoch | Technologie noch jung, kein fertiger Audio-Stack |
| E: Klassisches DECT (wie Riedel Bolero, Clear-Com FreeSpeak) | ✅✅ | ✅ | ca. 30–50 ms Ende-zu-Ende, sehr konstant | ❌ Gateway nötig | ❌ Module und Protokoll-Stacks sind für Selbstbau kaum erhältlich | Nicht machbar |
| F: Eigenes 2,4-GHz- oder Sub-GHz-Funkprotokoll | ⚠️ / ✅ | ⚠️ Bandbreite begrenzt | niedrig | ❌ eigene Basisstation | sehr hoch | Nicht sinnvoll |
| G: Bluetooth LE Audio | ❌ geringe Reichweite | ❌ | niedrig | ❌ | – | Ungeeignet |

**Warum 5 GHz so viel bringt:** Für „viele Teilnehmer“ zählt die **Anzahl der Funkkanäle**.
Jedes Bodypack verschickt viele kleine Pakete. Ein AP-Funkkanal schafft realistisch etwa 15–25 Sprach-Clients
(Erfahrungswert aus Voice-over-WLAN). 2,4 GHz hat nur 3 nutzbare Kanäle, die zudem mit allem anderen geteilt werden.
5 GHz hat deutlich mehr. Mehr Teilnehmer heißt dann einfach mehr APs auf verschiedenen Kanälen.

**Nachteil von 5 GHz:** Die Reichweite ist etwas geringer und Wände dämpfen stärker. Das gleicht man mit mehreren APs aus,
was bei vielen Teilnehmern ohnehin nötig ist. Wichtig ist außerdem, wie die Antenne am Bodypack sitzt (siehe Abschnitt 5).

---

## 3. Systemaufbau (Empfehlung)

```
                  ┌──────────────── Trägerplatine (eigene Platine, EasyEDA) ───────────────┐
                  │                                                                        │
 Headset  ──XLR-4/TRRS──► Mic-Vorverstärker / Bias ──► TLV320AIC3204 ◄─I2S/I2C─► ESP32-C5-WROOM-1U ──U.FL──► Dual-Band-Antenne
 (Kopfhörer) ◄──────────── Kopfhörer-Ausgang ◄────────┘     (Codec)                 (WLAN 5 GHz, Opus, RTP,
                  │                                                                  Socket.IO zu NR_Com)
                  │   Taster SW1–4 (Talk) ─────────────────────────────────────────►    │ UART
                  │                                                                     ▼
                  │   Drehgeber 1+2 ──────────────► [Buchsenleisten] ◄── Waveshare ESP32-S3 AMOLED (gesteckt)
                  │                                                     Display, Menü, Pegel, Akku-Anzeige
                  │                                                                        │
                  │   USB-C ─► USB-Hub (beide Chips flashbar) ─► Lader mit Power-Path ─► 2× 18650 (1S2P)
                  │                                      └─► Buck-Boost 3,3 V ─► C5 + Codec
                  │                                      └─► VSYS ─► ESP32-S3-Board
                  │   Akku-Anzeige (MAX17048), Ein/Aus-Taster (Soft-Power), Status-LEDs, opt. Vibration
                  └────────────────────────────────────────────────────────────────────────┘
```

**Warum zwei Chips statt einem?**
- Das Audio läuft auf dem C5 völlig ungestört. Im bisherigen Sketch musste I2S während jedes Display-Updates **angehalten** werden.
  Das kann es so nicht mehr geben.
- Das S3-Board kann bleiben, wie es ist: Display, Gehäuse-Ausschnitt und Bedienung sind schon da.
- Zwischen den Boards laufen nur 2–4 Leitungen (UART plus Reset/Boot für den C5). Die Pin-Probleme von heute (PSRAM-Pins, Display-Reset) fallen weg.
- Eine Alternative wäre, den C5 als reines WLAN-Modul für den S3 zu nutzen (ESP-Hosted). Das braucht aber 6–7 schnelle
  Leitungen über die Steckverbindung, und auf dem S3 sind kaum Pins frei. Deshalb ist die Aufteilung nach Aufgaben besser.

**Talk-Tasten direkt an den C5:** Beim Drücken schaltet der C5 das Mikrofon **sofort lokal** frei und meldet es parallel an den Server.
So gibt es keine Verzögerung durch den Umweg über das Display-Board.

---

## 4. Latenz-Budget

| Abschnitt | Heute (NR_Com-Standard) | Optimiert |
|---|---|---|
| Aufnahme-Puffer (I2S/DMA) | 5 ms | 2–5 ms |
| Opus-Frame + Lookahead (Gerät) | 10 + 2,5 ms | 10 + 2,5 ms (Modus „restricted lowdelay“) |
| WLAN Uplink | 2–10 ms | 1–5 ms (5 GHz, WMM-Voice) |
| **Server-Jitterbuffer** | **60 ms** | 20–30 ms |
| **Server-Mixer (audiomixer)** | **40 ms** | 10 ms |
| Server Opus-Encoder | 10 ms | 10 ms |
| WLAN Downlink | 2–10 ms | 1–5 ms |
| Jitterbuffer Gerät | 30–40 ms | 20–30 ms (adaptiv) |
| Wiedergabe-Puffer | 5 ms | 2–5 ms |
| **Summe Mund → Ohr** | **ca. 165–190 ms** | **ca. 75–100 ms** |

- Der größte Hebel liegt **im Server** (Jitterbuffer und Mixer). Das ist ein Thema für den Com-Dev, der Wert ist dort schon einstellbar (`audioMixerService.js:115-166`).
- Der eigene Mithörton kommt **analog** im Codec mit ca. 0 ms. So fühlt sich das Sprechen sofort an.
- Viele Teilnehmer oder 10-ms-Frames: Bei 10 ms sind es 100 Pakete pro Sekunde und Richtung pro Gerät.
  Wird die Funkzelle voll, können Gerät und Server auf **20-ms-Frames** gehen. Das halbiert die Paketlast und kostet 10 ms Latenz.
  Die Frame-Länge wird in der Firmware einstellbar sein.

---

## 5. Empfang: worauf es ankommt

1. **Externe Dual-Band-Antenne** (2,4/5 GHz) über U.FL, oben am Gehäuse und vom Körper weg (wie die Stummelantenne bei Profi-Beltpacks).
   Der Körper dämpft 5 GHz stark. Gürtelclip und Antenne gehören deshalb auf entgegengesetzte Seiten.
2. **Kein Metallgehäuse um die Antenne.** Akkus und Platine stehen zwischen Antenne und Körper, nicht zwischen Antenne und AP.
3. **Option für später: Antennen-Diversity.** Zwei Antennen (vorne und hinten) mit einem HF-Umschalter. ESP-IDF unterstützt externe Antennenumschaltung.
4. **Infrastruktur:**
   - eigene SSID nur für das Intercom, **nur 5 GHz**, 20 MHz Kanalbreite
   - für Shows die DFS-freien Kanäle 36–48 bevorzugen. Bei DFS kann ein Radar-Ereignis den AP für ca. 60 s vom Kanal werfen.
   - mehrere APs mit Controller und Roaming (802.11k/v/r)
   - niedrige Datenraten abschalten (Mindestrate 12–24 Mbit/s), WMM an
   - Server per Kabel ans Netz
5. **Firmware:** WLAN-Stromsparmodus aus (sonst ca. 100 ms zusätzliche Verzögerung). Pakete als Sprache markieren (DSCP EF → WMM AC_VO). Opus mit FEC und Paketverlust-Verschleierung (PLC).

---

## 6. Akku und Laufzeit

**Verbrauch (Schätzung, muss am Prototyp gemessen werden):**

| Verbraucher | ca. |
|---|---|
| ESP32-C5, WLAN aktiv ohne Stromsparen (5 GHz) | 0,35–0,5 W |
| ESP32-S3-Board (WLAN aus) + AMOLED (dunkle Oberfläche, mittlere Helligkeit) | 0,25–0,4 W |
| Codec + Kopfhörer (laut) | 0,05–0,15 W |
| Wandlerverluste (ca. 10 %) | 0,1 W |
| **Summe** | **ca. 0,8–1,2 W** |

**Laufzeit:**

| Akku | Energie (nutzbar) | Laufzeit |
|---|---|---|
| 2× 18650 à 3400 mAh, parallel | ca. 22 Wh | **ca. 18–27 h** |
| 3× 18650 parallel | ca. 33 Wh | ca. 27–40 h |

→ **2 Zellen reichen für einen Tag**, auch mit Reserve. Bei viel Kälte oder sehr hellem Display werden es weniger.

**Warum parallel (1S) und nicht in Reihe (2S):**
- Alles läuft mit 3,3 V. Bei 1S braucht es nur einen Buck-Boost-Wandler.
- Laden über USB-C mit 5 V ist einfach, ohne Spannungserhöhung und ohne Balancing.
- Nur **gleiche Zellen** aus derselben Charge verwenden, entweder als fertiges Pack mit Schutzschaltung oder geschützte Zellen in Haltern.

**Stromversorgung auf der Trägerplatine:**
- **USB-C**, CC-Widerstände für 5 V/3 A, ESD-Schutz
- **Lader mit Power-Path** (Gerät läuft beim Laden weiter), z. B. TI BQ25606 (eigenständig) oder BQ25895 (per I2C auslesbar), Ladestrom 2–3 A, **NTC** an den Zellen (sie liegen am Körper)
- **Akku-Anzeige:** MAX17048 (zeigt % genau an, kaum Eigenverbrauch)
- **3,3 V:** Buck-Boost (z. B. TI TPS63020/TPS63802) für C5 und Codec. Den Analogteil des Codecs zusätzlich filtern.
- **ESP32-S3-Board:** Über den Pin **VSYS** speisen (Systemspannung 3,5–4,2 V). Der eigene Akkuanschluss (BAT) des Boards bleibt **leer**.
  ⚠️ Vor dem Platinen-Layout am Board-Schaltplan prüfen.
- **Ein/Aus:** Soft-Power-Taster (lang drücken). Die Firmware kann dann sauber herunterfahren und sich abmelden.

---

## 7. Audio-Teil

- **Codec TLV320AIC3204** (Alternative TLV320AIC3254 mit größerem DSP; WM8960 als Notlösung).
  - Mikrofon-Verstärkung bis +47,5 dB, differenzielle Eingänge. Mit Vorverstärker geeignet für **dynamische** Headset-Mikrofone, mit Bias für **Elektret**.
  - Der interne DSP macht Hochpass, EQ, AGC und Limiter, sodass der Funkchip nicht damit belastet wird.
  - Analoger Mithörton ohne Verzögerung.
  - Sehr geringer Verbrauch.
- **Headset-Anschluss:** Bei Profi-Intercom-Headsets ist **XLR-4** üblich (1 Mic−, 2 Mic+, 3 Kopfhörer−, 4 Kopfhörer+).
  Zusätzlich **3,5 mm TRRS** für Handy- und PC-Headsets. Mic-Bias umschaltbar (dynamisch/Elektret).
- **Kopfhörer-Verstärker:** Profi-Headsets haben oft 150–600 Ω und brauchen mehr Spannung, als der Codec bei 3,3 V liefert.
  Optional ein kleiner Kopfhörer-Verstärker mit Ladungspumpe (z. B. TPA6132A2).
- **Takt:** Der C5 ist I2S-Master und liefert MCLK, oder der Codec erzeugt den Takt per PLL aus BCLK. Es gibt nur **eine** Taktquelle, sodass das Problem von heute nicht mehr auftreten kann.
- Abtastrate 48 kHz. Opus kann intern auch mit 16 kHz (Wideband) arbeiten. Das spart Rechenzeit, die Sprachqualität ist für Intercom ausreichend, und der Server braucht dafür keine Änderung.

---

## 8. Was ändert sich an der Software-Planung?

- Die bisherige Planung bleibt zum großen Teil gültig (Login, Socket.IO, PlainTransport, Opus/RTP, PTT, Lautstärke).
  Sie läuft jetzt auf dem **ESP32-C5**.
- Neu: eine kleine **UART-Schnittstelle zwischen C5 und S3** (Status, Pegel, Kanalnamen, Tastendrücke, Menübefehle).
- Firmware-Updates: Der C5 holt Updates per WLAN (OTA) und kann den S3 über UART mitflashen. Für die Entwicklung sind beide über einen USB-Hub an der USB-C-Buchse erreichbar.
- Beim Server sind zusätzlich zum bisherigen Prompt die **Latenz-Parameter** wichtig (Jitterbuffer und Mixer-Latenz, optional 20-ms-Frames).

---

## 9. Vorgehen (Vorschlag)

1. **Prototyp ohne eigene Platine:**
   - ESP32-C5-Devkit, z. B. ESP32-C5-DevKitC-1 mit WROOM-1U
   - TLV320AIC3204-Eval- oder Breakout-Board (alternativ vorerst das vorhandene WM8960-Board)
   - vorhandenes S3-AMOLED-Board
   - Damit gleich messen: WLAN-Latenz, Paketverlust, Stromverbrauch, Opus-Rechenlast
2. **Gleichzeitig:** Server-Erweiterung (PlainTransport) und Latenz-Tuning durch den Com-Dev
3. **Trägerplatine V1** in EasyEDA, gefertigt und bestückt bei JLCPCB. Davor die Teile auf Verfügbarkeit bei LCSC prüfen.
4. **Gehäuse** mit Antennenposition, Akkufach und Gürtelclip
5. **Feldtest** mit mehreren Geräten und mehreren APs

## Offene Fragen an dich

1. **Welche Headsets** sollen dran (Hersteller/Modell, dynamisch oder Elektret, Anschluss XLR-4 oder 3,5 mm)?
2. **Wie viele Bodypacks** gleichzeitig (ungefähr 5, 20 oder 50+)? Wie groß ist das Gelände, drinnen oder draußen?
3. **WLAN-Infrastruktur:** Gibt es schon APs (welche)? Oder soll eine Empfehlung für ein Intercom-Funknetz dazu?
4. Braucht es einen **Lautsprecher** im Bodypack (z. B. für Rufsignale), eine **Vibration** oder **Ruf-Tasten (Call)**?
5. Sollen die Talk-Tasten **beleuchtet** sein (Talk-/Ruf-Anzeige)?
