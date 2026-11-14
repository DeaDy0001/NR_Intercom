# Funknetz-Planung für 20+ Bodypacks

## 1. Wie weit darf ein Bodypack vom Access Point (AP) weg sein?

Entscheidend ist die **Signalstärke**, nicht die Entfernung. Die Firmware zeigt sie im Funk-Messmodus am Display.

| Signalstärke am Bodypack | Was man hört |
|---|---|
| besser als **−67 dBm** | einwandfrei |
| −67 bis −75 dBm | gut. Einzelne verlorene Pakete werden überdeckt, man hört nichts. |
| −75 bis −82 dBm | kurze Aussetzer möglich, vor allem wenn man sich vom AP wegdreht |
| schlechter als −82 dBm | starke Aussetzer, Verbindung reißt ab |

### Gerechnete Reichweite (5 GHz)

**Sicher** heißt: Auch wenn der eigene Körper zwischen Bodypack und AP steht, bleiben mindestens −75 dBm.
**Nur mit Sicht** heißt: −75 dBm werden nur erreicht, wenn die Antenne freie Sicht zum AP hat. Dreht man sich weg, gibt es Aussetzer.

| Umgebung | sicher | nur mit Sicht zum AP |
|---|---|---|
| draußen, freie Sicht, AP auf 3–4 m Höhe | **ca. 40 m** | bis ca. 150 m |
| Halle / Bühne, freie Sicht | **ca. 30 m** | bis ca. 100 m |
| Halle mit Publikum oder Kulissen dazwischen | **ca. 15–20 m** | bis ca. 50 m |
| Backstage, Gänge, durch 1 Leichtbauwand | **ca. 8–10 m** | bis ca. 20 m |
| durch 1 Ziegelwand | ca. 5 m | bis ca. 14 m |
| durch 1 Betonwand | ca. 3 m | bis ca. 7 m → eigener AP im Raum |

Für die Planung zählt die Spalte **sicher**: Im Radius von 30 m um einen AP (Halle) bzw. 40 m (draußen) funktioniert es,
egal wie man steht. Jede massive Wand ist eine neue Zelle.

### So ist das gerechnet

| Größe | Wert |
|---|---|
| Sendeleistung AP | 17 dBm (auf „mittel“ gestellt) + 4 dBi Antenne = 21 dBm |
| Sendeleistung Bodypack (ESP32-C5, 5 GHz) | 16,5 dBm + 2 dBi externe Antenne |
| Verluste am Bodypack (Antennenumschalter, Kabel, Verstimmung durch Körpernähe) | 4 dB |
| Empfindlichkeit ESP32-C5 bei der langsamsten Datenrate | ca. −92 bis −94 dBm |
| Dämpfung durch den eigenen Körper bei 5 GHz | 0 dB (Sicht) bis ca. 15 dB (Körper dazwischen) |
| Streckendämpfung | 46,9 dB bei 1 m, dann je nach Umgebung 20–30 dB je Faktor 10 in der Entfernung |
| Wände bei 5 GHz | Leichtbau ca. 4 dB, Ziegel ca. 10 dB, Beton ca. 18 dB |
| Reserve für Mehrwegeausbreitung (Schwund) | 17 dB: von −75 dBm bis zur Empfindlichkeit |

- Pegel am Bodypack = 21 + 2 − 4 − Streckendämpfung − Körper = **19 dBm − Streckendämpfung − Körper**.
- Pegel am AP = 16,5 + 2 − 4 + 4 − Streckendämpfung − Körper = **18,5 dBm − Streckendämpfung − Körper**.
  Beide Richtungen sind also etwa gleich stark, solange der AP auf mittlerer Leistung steht.
- Beispiel Halle mit Sicht, 30 m, Körper dazwischen: 46,9 + 22 × log(30) = 79,4 dB → 19 − 79,4 − 15 = **−75 dBm**.
- Die Werte sind gerechnet, nicht gemessen. Antenne, Gehäuse und Publikum verschieben sie leicht um ±5 dB,
  das ändert die Entfernung um den Faktor 1,5. Vor jedem Einsatz mit dem Funk-Messmodus abgehen.

### Was die Reichweite am stärksten beeinflusst

1. **Antenne am Bodypack.** Die externe Antenne muss oben aus dem Gehäuse ragen, weg von Akkus und Platine.
   Mit der eingebauten Keramikantenne im Gehäuse fehlen ca. 5–8 dB, die Reichweite halbiert sich.
2. **Höhe der APs.** 3–4 m hoch montieren, über den Köpfen. Ein AP auf Tischhöhe hinter Publikum verliert 10 dB und mehr.
3. **Sendeleistung des AP.** Mehr als ca. 17 dBm bringt nichts: Das Bodypack hört den AP dann zwar weiter,
   der AP das Bodypack aber nicht. Sprechen fällt dann vor dem Hören aus.
4. **2,4 GHz als Ausweichband.** Reicht rechnerisch etwa doppelt so weit (sicher ca. 80 m in der Halle), weil Strecke und Körper
   weniger dämpfen. Bei Veranstaltungen ist das Band aber voll (Handys, Bluetooth), deshalb nur als Notlösung für große Freiflächen.

### Kapazität je AP

Jedes Bodypack schickt und empfängt 100 Pakete pro Sekunde (Opus, 10 ms). Ein Paket belegt den Funkkanal ca. 0,2–0,35 ms.
Das sind **4–7 % der Funkzeit je Bodypack**. Deshalb **höchstens 10–12 Bodypacks je AP und Kanal**.
Mit 20-ms-Paketen (Einstellung `frame`) halbiert sich die Funkzeit, die Verzögerung steigt um 10 ms.

## 2. Wie viele APs für 20+ Bodypacks?

Jedes Bodypack verschickt viele kleine Sprachpakete (50–100 pro Sekunde und Richtung).
Ein AP-Funkkanal schafft mit 10-ms-Paketen zuverlässig etwa **12–15 Bodypacks** (Rechnung in Abschnitt 1).

| Bodypacks | APs (Minimum) | Empfehlung |
|---|---|---|
| bis 12 | 1 | 2 (Ausfallsicherheit) |
| 20–25 | 2 | **3** |
| 30–40 | 3 | 4 |
| 50+ | 4 | 5–6 |

- **Auch im selben Raum** sollten bei 20+ Geräten **mindestens 2 APs auf verschiedenen Kanälen** laufen. Der Grund ist die Kapazität, nicht die Reichweite.
- Pro AP **ca. 10–12 Bodypacks** einplanen, dann bleibt Reserve.
- Die APs **verteilen** und **hoch montieren** (3–4 m, über den Köpfen des Publikums), mit Sicht auf die Bereiche, in denen sich die Crew bewegt.

## 3. Kanäle (EU / Österreich)

| Band | Kanäle | Hinweis |
|---|---|---|
| 5 GHz UNII-1 | **36, 40, 44, 48** | ohne DFS, **nur drinnen**. **Erste Wahl für Shows in Gebäuden** |
| 5 GHz UNII-2 | 52–64 | DFS, nur drinnen |
| 5 GHz UNII-2e | **100–140** | DFS, **auch draußen erlaubt** → für Open Air |
| 2,4 GHz | – | für das Intercom **nicht verwenden** |

- **Kanalbreite 20 MHz**: Das gibt mehr Kanäle und robustere Verbindungen. Sprache braucht keine Bandbreite.
- **DFS:** Erkennt der AP ein Radar, muss er den Kanal verlassen und zuerst ca. 60 s auf dem neuen Kanal prüfen.
  Wenn jeder Ort von zwei APs abgedeckt ist, wechseln die Bodypacks einfach zum anderen AP.
- Kanäle **fest einstellen**. Automatische Kanalwechsel und „Optimierung“ während Shows ausschalten.

## 4. Empfohlene Hardware

Ziel: Wi-Fi 6 oder 7, zentrale Verwaltung (Controller), **schnelles Roaming (802.11r/k/v)**, PoE, gutes Preis-Leistungs-Verhältnis.

**Variante 1, Ubiquiti UniFi (einfache Bedienung, verbreitet):**
- 3× **UniFi U7 Pro** oder **U6+** (innen); draußen **U6 Mesh** oder **U7 Outdoor**
- 1× **UniFi Cloud Gateway Ultra** (Router, DHCP, Controller, funktioniert ohne Internet)
- 1× PoE-Switch mit 8 Ports, z. B. **UniFi Switch Lite 8 PoE**

**Variante 2, TP-Link Omada (günstiger):**
- 3× **EAP650** oder **EAP670** (innen), Outdoor-Modelle wie EAP610-Outdoor
- **OC200**-Controller (oder Software-Controller auf dem Server-PC), Router **ER605**
- PoE-Switch, z. B. **SG2008P**

**Allgemein:**
- Server-PC per **Kabel** am Switch, niemals per WLAN
- Cat6-Kabel zu den APs, bis 100 m pro AP. Die APs können also weit vom Server weg sein.
- Für Touring ein **Flightcase** mit Switch, Gateway und Kabeltrommeln, dazu AP-Halter für Stative oder Traversen

> Modellnamen sind Beispiele. Vor dem Kauf aktuelle Verfügbarkeit und Preise prüfen.

## 5. Einstellungen im WLAN-Controller

| Einstellung | Wert |
|---|---|
| SSID | eigene, z. B. `NR-Intercom`, **nur 5 GHz** (2,4 und 6 GHz aus) |
| Sicherheit | WPA2-PSK (AES) oder WPA2/WPA3-gemischt |
| Kanalbreite | 20 MHz |
| Kanäle | fest, nicht überlappend (z. B. AP1 = 36, AP2 = 44, AP3 = 40 bzw. 100/108/116 draußen) |
| Sendeleistung | mittel (ca. 14–17 dBm), gleich auf allen APs |
| Roaming | **802.11r (Fast Roaming), 802.11k, 802.11v an** |
| Mindest-Datenrate | 12–24 Mbit/s (alte, langsame Raten aus) |
| WMM / QoS | an (Sprachpakete werden bevorzugt) |
| DTIM | 1 |
| Band Steering | aus (SSID ist ohnehin nur 5 GHz) |
| Multicast / mDNS | mDNS erlauben (für `nr-com.local`) **oder** feste Server-IP verwenden |
| Client-Isolation | aus |
| Internet | nicht nötig. Das Netz kann komplett ohne Internet laufen |
| DHCP | Reservierungen pro Bodypack (feste IP je Gerät, erleichtert die Fehlersuche) |

## 6. Was die Bodypack-Firmware beiträgt

- WLAN-Stromsparmodus **aus** (sonst bis 100 ms zusätzliche Verzögerung)
- Sprachpakete als **Voice** markieren (DSCP EF → WMM AC_VO)
- **Roaming** mit 802.11k/v/r. Die Firmware wechselt rechtzeitig zum besseren AP, nicht erst bei Verbindungsabbruch.
- Opus mit **FEC** und **Paketverlust-Verschleierung**: Einzelne verlorene Pakete hört man nicht.
- **Funk-Messmodus** am Display (Signalstärke, AP, Kanal, Verlust, Latenz)
- Externe Antenne per Software aktivieren (ESP32-C5-Zero: Antennen-Umschalter auf GP26)
