# Funknetz-Planung für 20+ Bodypacks

## 1. Wie weit darf ein Bodypack vom Access Point (AP) weg sein?

Entscheidend ist nicht eine feste Entfernung, sondern die **Signalstärke am Bodypack**.
Für Sprache über WLAN gilt die übliche Planungsregel:

| Wert | Ziel |
|---|---|
| Signalstärke (RSSI) am Bodypack | **besser als −67 dBm** |
| Signal-Rausch-Abstand | **mind. 25 dB** |
| Überlappung zum nächsten AP | jeder Ort sieht einen **zweiten AP mit mind. −75 dBm** (für Roaming und Ausfallsicherheit) |

**Richtwerte für 5 GHz mit am Körper getragenem Bodypack und externer Antenne:**

| Umgebung | typische Reichweite pro AP |
|---|---|
| Halle / Bühne mit Sichtverbindung | **ca. 30–50 m** |
| durch 1–2 Leichtbauwände | ca. 15–25 m |
| Backstage mit Beton- oder Ziegelwänden | ca. 8–15 m, **jede massive Wand ist eine „neue Zelle“** |
| draußen mit Sichtverbindung, AP erhöht montiert | ca. 60–100 m (mit Richtantennen mehr) |

Das sind Erfahrungswerte, keine Garantie. Wie gut es im Gebäude wirklich klappt, zeigt nur eine **Messung**.
Dafür bekommt die Firmware einen **Funk-Messmodus**: Das Display zeigt Signalstärke, AP-Name, Kanal, Paketverlust und Latenz.
Man geht damit einmal die Location ab.

**Warum die Richtung vom Bodypack zum AP die Grenze ist:**
- Das Bodypack sendet mit kleiner Antenne und wenig Leistung, und der Körper dämpft zusätzlich 5–15 dB.
- Der AP ist meist deutlich stärker. Deshalb die **Sendeleistung der APs auf mittel stellen** (ca. 14–17 dBm), damit die Verbindung in beide Richtungen etwa gleich gut ist.
- Sonst „hört“ das Bodypack den AP noch, aber der AP das Bodypack nicht mehr. Das Bodypack wechselt dann zu spät zum besseren AP.

## 2. Wie viele APs für 20+ Bodypacks?

Jedes Bodypack verschickt viele kleine Sprachpakete (50–100 pro Sekunde und Richtung).
Ein AP-Funkkanal schafft zuverlässig etwa **15–20 Sprach-Clients**.

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
