# NR_Com Device-Gateway – API v1 (Bodypack ↔ Server)

Damit sich ein Hardware-Bodypack wie ein normaler NR_Com-Benutzer verhält (Kanäle sehen, betreten, verlassen,
sprechen, Lautstärke), ohne selbst WebRTC, TLS oder Socket.IO sprechen zu müssen.

**Warum ein Gateway?** Das Bodypack (ESP32-C5, ohne PSRAM, ca. 70 KB freier RAM) kann weder TLS (WSS) noch die
mediasoup-Signalisierung mit großen JSON-Objekten stemmen. Der Server erledigt deshalb die mediasoup-Arbeit intern
(PlainTransport, Producer, Consumer des Mix-Minus) und bietet dem Gerät nur:

- **HTTP/1.1 + JSON** (unverschlüsselt, nur LAN) für Login, Kanalliste und Befehle,
- **RTP/UDP mit Opus** für das Audio in beide Richtungen.

Referenz-Implementierung zum Testen ohne NR_Com: [`tools/mock_gateway.py`](../tools/mock_gateway.py).

---

## 1. Verbindung

| | |
|---|---|
| Basis-URL | `http://<server>:3002/device/v1` (der vorhandene unverschlüsselte HTTP-Server von NR_Com, `HTTP_PORT`) |
| Servername | Standard `nr-com.local` (mDNS, wie heute), alternativ IP-Adresse |
| Format | JSON, UTF-8, `Content-Type: application/json` |
| Token | nach dem Login bei jedem Aufruf: GET als `?token=…`, POST im JSON-Body als `"token"` |
| Lebenszeichen | Das Gerät ruft spätestens alle **5 s** einen Endpunkt auf (normalerweise `state`). Ohne Aufruf für **15 s** gilt das Gerät als getrennt (wie ein Socket-Disconnect). |

---

## 1a. Server finden (UDP-Broadcast, Port 3010)

Damit niemand am Bodypack eine IP eintippen muss:

- Das Gerät schickt `NRCOM_DISCOVER 1\n` (ASCII) per UDP an `255.255.255.255:3010` und an die
  Broadcast-Adresse seines WLANs, alle 500 ms, insgesamt ca. 1,6 s lang.
- Jeder NR_Com-Server antwortet **per Unicast an Absender-IP und -Port** mit einem JSON-Objekt:
  ```json
  { "nrcom": 1, "api": 1, "name": "NR_Com", "project": { "id": 1, "name": "Show 2026" }, "port": 3002 }
  ```
  `port` ist der HTTP-Port, an dem `/device/v1` tatsächlich hängt (bei SSL an `HTTP_PORT` = 3002,
  bei SSL aus der Hauptport). Die Server-IP nimmt das Gerät aus dem Absender der Antwort.
- Gerät mit Server-Einstellung „automatisch“: nimmt den ersten Server, der antwortet. Antwortet keiner,
  versucht es `nr-com.local` (mDNS) mit dem eingestellten Port.

---

## 2. Endpunkte

### `GET /device/v1/hello`
Ohne Login. Dient zum Finden und Prüfen des Servers.
```json
{ "api": 1, "name": "NR_Com", "project": { "id": 1, "name": "Show 2026" } }
```

### `POST /device/v1/login`
```json
{ "pin": "1234", "deviceId": "3cdc758b081c", "deviceName": "Bodypack", "rtpPort": 5004, "ssrc": 305419896 }
```
- `pin`: PIN eines in NR_Com angelegten Benutzers (z. B. „Bodypack 1“), geprüft wie `POST /api/projects/:id/auth/pin`
  für das aktive Projekt.
- `deviceId`: MAC-Adresse ohne Doppelpunkte. Ein neuer Login mit derselben `deviceId` ersetzt die alte Sitzung.
- `rtpPort`: UDP-Port des Geräts (Senden **und** Empfangen). Die Geräte-IP nimmt der Server aus der TCP-Verbindung
  (`req.socket.remoteAddress`, ohne `::ffff:`).
- `ssrc`: SSRC der RTP-Pakete, die das Gerät sendet.

Antwort `200`:
```json
{ "token": "9f2c…", "user": { "id": 7, "name": "Bodypack 1" }, "rtp": { "port": 40123 }, "pollMs": 5000 }
```
- `rtp.port`: UDP-Port des PlainTransports auf dem Server. Das Gerät sendet an `<server-ip>:<rtp.port>`.

Fehler: `401 {"error":"pin"}` (falsche PIN), `503 {"error":"no-project"}` (kein aktives Projekt).

### `GET /device/v1/state?token=…&since=<rev>&wait=<ms>`
Long-Poll. Ist `rev` auf dem Server ungleich `since`, kommt die Antwort sofort. Sonst wartet der Server bis zu
`wait` ms (höchstens 5000) auf eine Änderung und antwortet dann (ggf. mit unveränderter `rev`).
```json
{
  "rev": 17,
  "user": "Bodypack 1",
  "channels": [
    { "id": 3, "name": "Regie", "color": "#ef4444", "joined": true,  "talk": false, "volume": 80, "talkers": 1 },
    { "id": 4, "name": "Ton",   "color": "#3b82f6", "joined": false, "talk": false, "volume": 100, "talkers": 0 }
  ]
}
```
- Alle Kanäle des aktiven Projekts, in der Reihenfolge wie im Browser.
- `joined`: Benutzer ist im Kanal (hört mit). `talk`: Mikrofon des Benutzers in diesem Kanal offen.
- `volume`: Kanal-Lautstärke 0…100 (= `channel_volume` × 100).
- `talkers`: Anzahl der **anderen** Benutzer, die gerade in diesem Kanal sprechen (für eine Aktivitätsanzeige; 0, wenn unbekannt).
- `rev` erhöht sich bei jeder Änderung, die das Gerät betrifft (Kanalliste, eigene Zustände, `talkers`).

Fehler: `401` → Sitzung unbekannt oder abgelaufen, das Gerät meldet sich neu an.

### `POST /device/v1/cmd`
```json
{ "token": "9f2c…", "cmd": "join",   "channel": 3 }
{ "token": "9f2c…", "cmd": "leave",  "channel": 3 }
{ "token": "9f2c…", "cmd": "talk",   "channel": 3, "value": 1 }
{ "token": "9f2c…", "cmd": "volume", "channel": 3, "value": 80 }
{ "token": "9f2c…", "cmd": "speaking", "value": 1 }
```
Antwort `200 {"ok":true,"rev":18}`, Fehler `400/401/404 {"error":"…"}`.

| `cmd` | entspricht im Browser | Hinweis |
|---|---|---|
| `join` | `joinChannel {channelId, producerId}` | danach Mikro im Kanal **zu** (`setChannelMicMute muted:true`), `talk` = false |
| `leave` | `leaveChannel {channelId}` | |
| `talk` | `setChannelMicMute {channelId, muted: !value}` | inkl. `userMicMuteChanged` und Sprech-Anzeige wie beim Browser |
| `volume` | `setChannelSettings {channelId, settings:{channel_volume: value/100}}` | |
| `speaking` | `speakingStatusChanged {isSpeaking: !!value}` | **ohne** `channel`. Das Gerät meldet 1, solange Talk an ist und sein Mikrofon-Gate offen ist (es wird gerade gesprochen), sonst 0. Nur bei Änderung, höchstens ca. 20× pro Sekunde. Für die Sprechanzeige (grünes Licht) im Browser. |

### `POST /device/v1/resume`  (Sitzung fortsetzen, ohne Ab- und Anmelden)
```json
{ "token": "9f2c…", "rtpPort": 5004, "ssrc": 305419896 }
```
Nach einem Funkloch, einem AP-Wechsel mit neuer IP oder dem Wechsel **WLAN ↔ LAN (Basisstation)**.
Der Server übernimmt die neue Geräte-Adresse (IP aus der TCP-Verbindung, `rtpPort` aus dem Body) und leitet das Audio
um. **Kanäle, Talk-Zustand und Online-Status bleiben, es gibt keine Verlassen-/Beitreten-Ereignisse und keine Töne
bei den anderen Benutzern.**
Antwort `200` wie beim Login (`token` ggf. dasselbe, `rtp.port` ggf. neu), zusätzlich `"resumed": true`.
`401` → Sitzung nicht mehr bekannt (Gnadenfrist abgelaufen) → normaler Login.

### Gnadenfrist und Übernahme
- **15 s ohne Aufruf:** Die Sitzung **ruht**. Kein RTP mehr, aber der Benutzer bleibt online und in seinen Kanälen.
  Kein Ereignis an die Browser.
- **Erst nach der Gnadenfrist (Standard 60 s, einstellbar)** wird aufgeräumt wie bei einem Socket-Disconnect.
- **Login mit derselben `deviceId`** (z. B. nach einem Neustart des Geräts, das Token ist dann weg) übernimmt die
  bestehende Sitzung **an Ort und Stelle**: neues Token, neue Adresse, Kanäle bleiben, keine Verlassen-/Beitreten-Ereignisse.

### `POST /device/v1/logout`
`{ "token": "…" }` → `200 {"ok":true}`. Räumt auf wie ein Socket-Disconnect.

---

## 3. Audio (RTP/UDP)

**Gerät → Server**
- Ziel `<server-ip>:<rtp.port>`, Quelle `<geräte-ip>:<rtpPort>`.
- Opus, 48 kHz, mono, Payload-Typ **111**, SSRC aus dem Login, Paketlänge 10 ms (5 und 20 ms ebenfalls möglich).
- Das Gerät sendet nur, solange in mindestens einem Kanal `talk` an ist. Der Server schaltet zusätzlich pro Kanal
  über `setUserMicMuted` frei, wie beim Browser.

**Server → Gerät**
- Der Mix-Minus des Benutzers (`mixedProducerId`) als RTP an `<geräte-ip>:<rtpPort>`.
- Payload-Typ beliebig (vom Router vergeben, z. B. 100). Das Gerät dekodiert jeden PT außer 96 als Opus.
- Nach `mixerRebuilt` darf sich SSRC/Sequenz ändern, das Gerät synchronisiert neu. Es spielt immer nur
  **einen** Strom: Ein neuer SSRC wird übernommen, sobald 3 Pakete in Folge von ihm kommen (ohne Paket des
  alten dazwischen) oder der alte 300 ms still ist. Der Server schickt höchstens einen Strom gleichzeitig.

**Server-intern (mediasoup)**
- `PlainTransport`: `listenIp {ip:'0.0.0.0', announcedIp:<LAN-IP wie bei createWebRtcTransport>}`, `rtcpMux:true`,
  `comedia:false`, danach `transport.connect({ ip: <geräte-ip>, port: rtpPort })`.
- Producer: `kind:'audio'`, `rtpParameters = { codecs:[{ mimeType:'audio/opus', payloadType:111, clockRate:48000,
  channels:2, parameters:{ useinbandfec:1 }, rtcpFeedback:[] }], encodings:[{ ssrc }], rtcp:{ cname:deviceId, reducedSize:true } }`.
- Consumer auf demselben Transport für `mixedProducerId`, `rtpCapabilities = router.rtpCapabilities`, `paused:false`.
  Bei `mixerRebuilt` für diesen Benutzer: alten Consumer schließen, neuen anlegen.

---

## 4. Ablauf im Gerät

```
WLAN verbunden
 → GET  /hello                       (Server gefunden?)
 → POST /login {pin, deviceId, rtpPort, ssrc}  → token, rtp.port
 → RTP-Ziel = server-ip : rtp.port, Audio-Modus "rtp"
 → Schleife: GET /state?since=rev&wait=5000  → Kanalliste anzeigen
 → Bedienung: POST /cmd join | leave | talk | volume
 → 401 → neu einloggen; Netzfehler → nach 2 s erneut versuchen
```
