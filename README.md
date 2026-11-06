# NR_Intercom – ESP32-S3 Bodypack für NR_Com

Drahtloses Intercom-Bodypack (Beltpack) als Client für die bestehende NR_Com-Software
(Repo `DeaDy0001/netrock-com`: Node.js + mediasoup/WebRTC, Mix-Minus pro Benutzer über GStreamer, derzeit Windows).

## Hardware

- Waveshare ESP32-S3 AMOLED-Board (ESP32-S3R8)
- Waveshare WM8960 Audio Board (Codec, Headset und Lautsprecher)
- 2× KY-040 Drehgeber, 4× Taster
- Headset-Anschluss, USB-C

➡️ **Verkabelung und gefundene Fehler: [docs/verkabelung.md](docs/verkabelung.md)**

➡️ **Analyse NR_Com und Anbindung des Bodypacks: [docs/analyse-nr-com.md](docs/analyse-nr-com.md)**

➡️ **Prompt für den Com-Dev (Server-Erweiterung): [docs/prompt-com-dev.md](docs/prompt-com-dev.md)**

## Plan

| Phase | Inhalt | Status |
|---|---|---|
| 0 | Verkabelung prüfen und korrigieren | 🔧 Fehler gefunden, Umlöten nötig |
| 1 | Hardware-Test-Firmware: WM8960 (I2C + I2S), Mikrofon → Kopfhörer, Drehgeber und Tasten, Display, Akku | ⏳ |
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
