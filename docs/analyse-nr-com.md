# Analyse NR_Com (netrock-com) aus Sicht des ESP32-Bodypacks

Stand: `DeaDy0001/netrock-com` main @ `0c61917`. Das Repository wurde **nur gelesen**.
Pfade beziehen sich auf das netrock-com-Repo.

## Kurzfassung

- NR_Com ist **kein reines GStreamer-System**, sondern läuft mit **WebRTC über mediasoup** (Node.js-Server, Browser-Client).
  GStreamer wird nur **auf dem Server** benutzt, um für jeden Benutzer einen eigenen Mix zu bauen.
- **Der Server mischt.** Jeder Benutzer bekommt genau **einen** Opus-Stream mit allen anderen Teilnehmern seiner Kanäle
  (Mix-Minus, die eigene Stimme ist nicht drin).
  → Das ESP32 braucht nur **1 Encoder + 1 Decoder**. Das ist ideal.
- Audioformat: **Opus, 48 kHz**, Mixer-Ausgang 64 kbit/s mit 10-ms-Frames und In-Band-FEC.
- Das Problem: Ein Client muss heute **vollständiges WebRTC** sprechen (ICE, DTLS-SRTP).
  Das ist auf dem ESP32 zwar möglich, aber aufwendig und fehleranfällig.
- **Lösung mit minimaler Server-Änderung:** mediasoup unterstützt auch einfaches RTP (**PlainTransport**).
  Der Server nutzt das intern schon. Es fehlt nur **ein neues Socket-Event**, mit dem ein Gerät so einen Transport anlegen kann.
  Alles andere (Login, Kanäle, Produce/Consume, PTT, Lautstärke) bleibt **unverändert** und wird wiederverwendet.
  → Prompt für den Com-Dev: [prompt-com-dev.md](prompt-com-dev.md)

## Wie ein Client heute funktioniert

| Schritt | Wie | Quelle |
|---|---|---|
| Projekt finden | `GET /api/projects/active` (ohne Login) | `server/src/routes/projectRoutes.js:174` |
| Login | `POST /api/projects/:id/auth/pin` `{pin, password?, rememberMe}` → `{user, token}` | `projectRoutes.js:917` |
| Signalisierung | **Socket.IO v4** über WSS auf Port 3000 (selbstsigniertes Zertifikat) | `server/src/server.js:178-209` |
| Anmelden am Socket | `register-user {userId, username, userRole}` | `socketService.js:50` |
| Heartbeat | `client-heartbeat` alle 3 s, sonst nach 10 s offline | `socketService.js:696`, `:28` |
| Medien | `getRouterRtpCapabilities` → `createWebRtcTransport` → `connectTransport` → `produce {…, channelIds}` | `socketService.js:862-950` |
| Kanal beitreten | `joinChannel {channelId, producerId}` → Antwort enthält **`mixedProducerId`** | `socketService.js:377-543` |
| Hören | `consume {transportId, producerId: mixedProducerId, rtpCapabilities}` | `socketService.js` |
| Mixer neu gebaut | Server sendet `mixerRebuilt {mixedProducerId}`, der Client muss neu `consume` aufrufen | `audioMixerService.js:1666` |
| Sprechen (PTT) | `setChannelMicMute {channelId, muted}`. **Nach dem Join ist das Mikro offen**, also sofort `muted:true` senden | `socketService.js:811` |
| Lautstärke pro Kanal | `setChannelSettings {channelId, settings:{channel_volume:0..1}}` (wirkt live im Mixer) | `socketService.js:1530` |
| Lautstärke pro Sprecher | `setUserVolume {targetUserId, channelId, volume}` | `socketService.js:1455` |
| Kanalliste | `GET /api/projects/:id/channels` | `projectRoutes.js:541` |
| Server finden | mDNS `nr-com.local` | `mdnsService.js` |

mediasoup-Codec (Router): `audio/opus 48000/2`, `useinbandfec:1, usedtx:0`.
Alle Transporte liegen in **einer** gemeinsamen Map. Deshalb funktionieren die bestehenden
Events `produce` und `consume` auch mit einem PlainTransport (`mediaService.js:383`, `:230`, `:255`).

## Geplanter Ablauf im Bodypack (nach der Server-Erweiterung)

> **Überholt:** Der C5 (ohne PSRAM) kann weder TLS noch Socket.IO. Der aktuelle Plan ist ein schlankes
> **Device-Gateway** im Server (HTTP + RTP): [device-gateway-api.md](device-gateway-api.md),
> Prompt: [prompt-com-dev.md](prompt-com-dev.md). Der Abschnitt unten bleibt als Hintergrund stehen.

```
WLAN → GET /api/projects/active → POST /auth/pin (eigener Benutzer "Bodypack 1")
     → WSS Socket.IO → register-user → Heartbeat alle 3 s
     → createPlainTransport (NEU)  → {id, ip, port}
     → produce (Opus 48 kHz mono, PT 111, eigene SSRC, channelIds)
     → joinChannel für jeden Kanal → setChannelMicMute muted:true
     → consume mixedProducerId → RTP empfangen auf demselben UDP-Port
     → bei mixerRebuilt: neu consume
Taster SW1–4  → setChannelMicMute (Talk/PTT pro Kanal)
Drehgeber     → setChannelSettings channel_volume / lokale Gesamtlautstärke
```

- **Eigene Stimme (Mithörton):** Sie ist nicht im Mix enthalten. Der WM8960 kann sie analog und ohne Verzögerung direkt auf den Kopfhörer legen.
- **Latenz (Schätzung):** Server-Jitterbuffer 60 ms + Mixer 40 ms + ESP-Jitterbuffer ca. 40 ms + Frames ≈ **150–200 ms** Ende-zu-Ende.
- **Takt:** Der WM8960 läuft mit seinem eigenen 24-MHz-Takt. Der ESP-Jitterbuffer muss kleine Taktabweichungen zum Server ausgleichen (gelegentlich ein Sample einfügen oder weglassen).
- **TLS:** Das Server-Zertifikat ist selbstsigniert. Das ESP kann es fest hinterlegen (`server/src/certs/cert.pem`).
- **Speicher:** TLS, WLAN und Opus brauchen den PSRAM. Das ist ein weiterer Grund, warum GPIO33–37 frei bleiben müssen.

## Erkenntnisse aus den vorhandenen ESP32-Versuchen (`wm8960/`)

`wm8960/wm8960_full_loopback/wm8960_full_loopback.ino` (Arduino):

- Pins im Code: BCLK 35, LRCLK 36, DOUT 37, DIN 38, **MCLK 34**, I2C 40/39.
  **Drehgeber 2 SW = GPIO15**, laut Kommentar „rewired“, also anders als im Schaltplan (GPIO17).
- Der ESP ist I2S-Master und gibt 12,288 MHz MCLK aus. Der WM8960 ist auf „SYSCLK = MCLK, keine PLL“ eingestellt.
- **Problem:** Das WM8960-Board hat einen eigenen 24-MHz-Oszillator, der fest auf den MCLK des Codecs geht.
  Der ESP-Takt kämpft dagegen (Jumper P1). Je nachdem, welcher „gewinnt“, läuft der Codec mit dem falschen Takt.
  Das passt genau zu dem „Noise Debug“, das dort gemacht wurde (Software-Noise-Gate, MCLK-Off-Test).
- Dazu kommt: I2S läuft auf GPIO34–37 (Octal-PSRAM). Das geht nur, solange PSRAM in der Arduino-IDE ausgeschaltet ist.
- Fazit: In der neuen Firmware ist der **WM8960 der Master** (PLL aus den 24 MHz) und der ESP der Slave, **ohne MCLK-Leitung**.
  Siehe [verkabelung.md](verkabelung.md).

## Gefundene Fehler im NR_Com-Server (zur Info an den Com-Dev)

Diese Punkte sind beim Lesen aufgefallen. Sie betreffen **alle** Clients, nicht nur das Bodypack:

1. `server/src/services/audioMixerService.js:461`: `getAllUserVolumes()` gibt ein **Array** zurück
   (`databaseService.js:2314`). Der Code ruft darauf aber `.get()` auf, was einen TypeError wirft.
   Folge: Im Python-Mixer-Modus wird ein neuer Sprecher nicht live in laufende Mixer eingefügt.
2. `server/src/services/socketService.js:2070`: `channelMemberships.get(userId)`. Die Map ist aber nach
   **channelId** aufgebaut (`:32`). Folge: Beim Disconnect wird der Mixer des Benutzers nie abgebaut.
3. `server/src/services/audioOutputGStreamerService.js:328`: `payload=96` ist fest eingetragen. Der Router vergibt aber PT 100,
   und in den Caps fehlt `clock-rate=48000`.
4. Sicherheit: `register-user` übernimmt die userId ohne Token-Prüfung. Das JWT-Secret hat einen Default-Wert (`jwtService.js`).
5. `server/src/config/mediasoup.js` wird nirgends geladen. Wirksam ist nur die Konfiguration in `mediaService.js`.
