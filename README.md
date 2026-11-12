# NR_Intercom – ESP32-S3 Bodypack für NR_Com

Drahtloses Intercom-Bodypack (Beltpack) als Client für die bestehende NR_Com-Software
(Repo `DeaDy0001/netrock-com`: Node.js + mediasoup/WebRTC, Mix-Minus pro Benutzer über GStreamer, derzeit Windows).

## Schnellstart (Windows)

**`NR-Intercom.bat`** doppelklicken: ein Fenster für alles. `help` zeigt die Befehle, z. B.
`c5 update` / `s3 update` (Firmware holen und flashen, Einstellungen bleiben erhalten),
`c5 monitor` / `s3 monitor` (Log, zurück mit Strg+Q), `c5 status` (Befehl am Bodypack ausführen), `status` (alles).
**Ohne Kabel über WLAN:** `devices` (Bodypacks im Netz), `wlan` bzw. `c5 wlan` / `s3 wlan` (Firmware holen und
über WLAN aufspielen). Voraussetzung: einmalig per USB `update` (neue Speicheraufteilung mit zwei Programmbereichen).

## Hardware

Ziel (siehe Hardware-Konzept):
- Waveshare ESP32-C5-Zero (WLAN 5 GHz, Audio, NR_Com-Protokoll), gesteckt
- Waveshare ESP32-S3 AMOLED (Display, Bedienung), gesteckt
- eigene Trägerplatine: TLV320AIC3204, Mic-Vorverstärker mit Phantomspeisung, XLR/6,35-Kombibuchse, 6,35-mm-Kopfhörer, 2× 18650
- 4 frei programmierbare Tasten, 2 Drehgeber

Aktueller Prototyp: ESP32-S3 AMOLED + Waveshare WM8960 Audio Board

➡️ **Hardware-Konzept (Funk, eigene Trägerplatine, Akku, Latenz): [docs/hardware-konzept.md](docs/hardware-konzept.md)**

➡️ **Funknetz-Planung (Access Points, Reichweite, Einstellungen): [docs/funknetz-planung.md](docs/funknetz-planung.md)**

➡️ **Gesamt-Schaltpläne (alles auf einem Blatt):** [Variante A: Testaufbau](docs/img/schaltplan-testaufbau.svg) · [Variante B: Trägerplatine V0.2 als Linien-Plan](docs/img/schaltplan-platine-v1-linien.svg) · [Variante B in Blöcken](docs/img/schaltplan-platine-v1.svg) · [Basisstation als Linien-Plan](docs/img/schaltplan-basisstation-linien.svg) (erzeugt mit `tools/schematics.py`, `tools/schematic_routed.py` und `tools/schematic_station.py`)

➡️ **Verdrahtungspläne Testaufbau (jeder Pin mit eigener Linie):** [C5 + WM8960 + Tasten](docs/img/verkabelung-c5-wm8960.png) · [S3 + Drehgeber](docs/img/verkabelung-s3-drehgeber.png)

➡️ **Verkabelung und gefundene Fehler: [docs/verkabelung.md](docs/verkabelung.md)** *(Prototyp mit WM8960)*

➡️ **Analyse NR_Com und Anbindung des Bodypacks: [docs/analyse-nr-com.md](docs/analyse-nr-com.md)**

➡️ **Prompt für den Com-Dev (Server-Erweiterung): [docs/prompt-com-dev.md](docs/prompt-com-dev.md)**

## Plan

| Phase | Inhalt | Status |
|---|---|---|
| 0 | Verkabelung prüfen und korrigieren | 🔧 Fehler gefunden, Umlöten nötig |
| 1 | Test-Firmware ESP32-C5: WM8960, Mikrofon → Kopfhörer, WLAN 5 GHz, Funk-Messung, RTP-Echo, Tasten → [firmware/bodypack-c5](firmware/bodypack-c5/README.md) | 🔧 bereit zum Testen |
| 1b | Test-Firmware ESP32-S3-Display: Oberfläche, Drehgeber, Akku → [firmware/display-s3](firmware/display-s3/README.md) | ✅ läuft (1,91″-AMOLED) |
| 1c | Intercom mit NR_Com: Kanäle, Talk, Audio in beide Richtungen, Updates über WLAN → [device-gateway-api.md](docs/device-gateway-api.md) | ✅ läuft |
| 2 | Display-Firmware für das **ESP32-S3-Touch-LCD-2.8B** (Hochformat, Touch) | ⏳ nach Lieferung |
| 3 | Trägerplatine V1, Gehäuse, Basisstation → [hardware-konzept.md](docs/hardware-konzept.md) | 📝 geplant |
| 2 | Netzwerk: WLAN, Login (REST), Socket.IO-Signalisierung, Opus über RTP (mediasoup PlainTransport) | ⏳ braucht das neue Server-Event `createPlainTransport` ([Prompt](docs/prompt-com-dev.md)) |
| 3 | Intercom-Funktion: Kanäle (`joinChannel`), Talk/PTT (`setChannelMicMute`), Lautstärke (`setChannelSettings`), Display-Oberfläche (LVGL) | ⏳ |
| 4 | Einrichtung (WLAN, Server, Name), OTA-Update, Akku-Optimierung | ⏳ |

## Software-Grundlage (geplant)

- **ESP-IDF 5.x** (C), kein Arduino: braucht man für I2S-Slave, Echtzeit-Audio, WLAN-Tuning und Opus
- Audio: WM8960 als I2S-Master (eigener 24-MHz-Oszillator → PLL → 12,288 MHz), ESP als I2S-Slave, 48 kHz / 16 Bit
- Netzwerk: Opus 48 kHz (1 Encoder + 1 Decoder, weil der Server mischt), RTP über UDP, Socket.IO v4 über WSS
- Eingaben: Drehgeber über die PCNT-Hardware (prellfrei, keine verlorenen Schritte), Taster entprellt
- Display: RM67162-AMOLED über QSPI (`esp_lcd`) mit LVGL

## Vorschlag Bedienung (zur Diskussion)

| Element | Funktion |
|---|---|
| SW1–SW4 | Talk-Taste Kanal 1–4 (kurz = Talk ein/aus, lang halten = PTT) |
| Drehgeber 1 | Gesamtlautstärke, Drücken = Mikrofon stumm |
| Drehgeber 2 | Kanal wählen, Drücken = Listen ein/aus, dann Drehen = Lautstärke des Kanals |
