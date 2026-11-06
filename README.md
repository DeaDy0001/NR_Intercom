# NR_Intercom – ESP32-S3 Bodypack für NR_Com

Drahtloses Intercom-Bodypack (Beltpack) als Client für die bestehende NR_Com-Software
(GStreamer-basiert, Multichannel, derzeit Windows).

## Hardware

- Waveshare ESP32-S3 AMOLED-Board (ESP32-S3R8)
- Waveshare WM8960 Audio Board (Codec, Headset und Lautsprecher)
- 2× KY-040 Drehgeber, 4× Taster
- Headset-Anschluss, USB-C

➡️ **Verkabelung und gefundene Fehler: [docs/verkabelung.md](docs/verkabelung.md)**

## Plan

| Phase | Inhalt | Status |
|---|---|---|
| 0 | Verkabelung prüfen und korrigieren | 🔧 Fehler gefunden, Umlöten nötig |
| 1 | Hardware-Test-Firmware: WM8960 (I2C + I2S), Mikrofon → Kopfhörer, Drehgeber und Tasten, Display, Akku | ⏳ |
| 2 | Netzwerk: WLAN und Audio-Streaming kompatibel zu NR_Com (vermutlich RTP + Opus) | ⏳ wartet auf [Antworten vom Com-Dev](docs/fragen-an-com-dev.md) |
| 3 | Intercom-Funktion: Kanäle, Talk/Listen, Lautstärke je Kanal, Display-Oberfläche (LVGL) | ⏳ |
| 4 | Einrichtung (WLAN, Server, Name), OTA-Update, Akku-Optimierung | ⏳ |

## Software-Grundlage (geplant)

- **ESP-IDF 5.x** (C), kein Arduino: braucht man für I2S-Slave, Echtzeit-Audio, WLAN-Tuning und Opus
- Audio: WM8960 als I2S-Master (eigener 24-MHz-Oszillator → PLL → 12,288 MHz), ESP als I2S-Slave, 48 kHz / 16 Bit
- Eingaben: Drehgeber über die PCNT-Hardware (prellfrei, keine verlorenen Schritte), Taster entprellt
- Display: RM67162-AMOLED über QSPI (`esp_lcd`) mit LVGL

## Vorschlag Bedienung (zur Diskussion)

| Element | Funktion |
|---|---|
| SW1–SW4 | Talk-Taste Kanal 1–4 (kurz = Talk ein/aus, lang halten = PTT) |
| Drehgeber 1 | Gesamtlautstärke, Drücken = Mikrofon stumm |
| Drehgeber 2 | Kanal wählen, Drücken = Listen ein/aus, dann Drehen = Lautstärke des Kanals |
