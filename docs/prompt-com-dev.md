# Prompt für den NR_Com-Chat (netrock-com)

Den Block unten komplett kopieren und in den Chat des Com-Devs einfügen. Die Datei
[device-gateway-api.md](device-gateway-api.md) bitte zusätzlich mitschicken (oder in netrock-com nach
`docs/DEVICE_GATEWAY_API.md` kopieren lassen). Sie ist die verbindliche Schnittstelle.

> Dieser Prompt **ersetzt** den früheren Vorschlag mit `createPlainTransport` über Socket.IO.
> Grund: Das Bodypack läuft auf einem ESP32-C5 ohne PSRAM. TLS (WSS), Socket.IO und mediasoup-JSON passen
> nicht in den Speicher. Deshalb übernimmt der Server die mediasoup-Arbeit, und das Gerät spricht nur HTTP + RTP.

---

```text
Kontext: Wir bauen ein Hardware-Bodypack (ESP32-C5 für Funk/Audio + ESP32-S3 mit Display und 2 Drehgebern).
Es soll sich in NR_Com wie ein normaler Benutzer verhalten: eigener User mit PIN (z. B. "Bodypack 1"),
alle Kanäle des aktiven Projekts sehen, Kanäle betreten und verlassen, pro Kanal sprechen (Talk) und
die Kanal-Lautstärke ändern. Audio genau wie beim Browser: Das Gerät schickt sein Mikrofon, der Server
schickt den fertigen Mix-Minus des Users (mixedProducerId) zurück.

Das Gerät kann KEIN TLS, kein Socket.IO und kein WebRTC (zu wenig RAM). Bitte deshalb ein schlankes
"Device-Gateway" im Server bauen: HTTP/JSON (unverschlüsselt, nur LAN) + RTP/UDP mit Opus.
Die genaue Schnittstelle steht in der beigelegten Datei DEVICE_GATEWAY_API.md (API v1). Bitte exakt so
umsetzen (Pfade, Feldnamen, Statuscodes), weil die Gerätefirmware schon danach gebaut ist.

Kurzfassung der API (Details in der Datei):
  Basis: http://<server>:3002/device/v1  -> am vorhandenen unverschlüsselten httpApp (HTTP_PORT, server.js),
         dort, wo heute schon /json und /api/companion-timeline hängen.
  GET  /hello                      -> {api:1, name:"NR_Com", project:{id,name}}
  POST /login  {pin, deviceId, deviceName, rtpPort, ssrc}
                                   -> {token, user:{id,name}, rtp:{port}, pollMs:5000} | 401 {error:"pin"}
  GET  /state?token&since&wait     -> Long-Poll (max 5000 ms): {rev, user, channels:[{id,name,color,joined,talk,volume,talkers}]}
  POST /cmd    {token, cmd:"join"|"leave"|"talk"|"volume", channel, value}  -> {ok:true, rev}
  POST /logout {token}
  Lebenszeichen: jeder Aufruf zählt; 15 s ohne Aufruf = getrennt (wie Socket-Disconnect).

Was der Server pro Gerät intern machen soll (minimal-invasiv, Browser-Flow NICHT ändern):
 1. Login: PIN prüfen wie POST /api/projects/:id/auth/pin (aktives Projekt), User wie bei 'register-user'
    als online führen (Online-Anzeige im Browser), Heartbeat-Zeitstempel bei jedem Gateway-Aufruf setzen.
    Kanäle, in denen der User laut DB schon ist, beim Login wieder betreten (wie beim Browser-Reload), Talk aus.
 2. mediasoup: PlainTransport mit listenIp {ip:'0.0.0.0', announcedIp:<dieselbe LAN-IP wie createWebRtcTransport>},
    rtcpMux:true, comedia:false, Port automatisch aus 40000-49999; dann transport.connect({ip:<Geräte-IP aus
    req.socket.remoteAddress ohne ::ffff:>, port:rtpPort}).
    Producer: kind 'audio', rtpParameters {codecs:[{mimeType:'audio/opus', payloadType:111, clockRate:48000,
    channels:2, parameters:{useinbandfec:1}, rtcpFeedback:[]}], encodings:[{ssrc}], rtcp:{cname:deviceId, reducedSize:true}}.
    Consumer des mixedProducerId auf demselben Transport (rtpCapabilities = router.rtpCapabilities, paused:false).
    Bei 'mixerRebuilt' für diesen User: Consumer neu anlegen. Das Gerät nimmt jeden Payload-Typ außer 96 als Opus.
 3. Befehle auf die vorhandene Logik abbilden (gleiche Wirkung und gleiche Broadcasts wie beim Browser):
      join   -> joinChannel {channelId, producerId}  + danach Mic in diesem Kanal stumm (setUserMicMuted ... true)
      leave  -> leaveChannel {channelId}
      talk   -> setChannelMicMute {channelId, muted: !value}  (inkl. userMicMuteChanged + Sprech-Anzeige)
      volume -> setChannelSettings {channelId, settings:{channel_volume: value/100}}
    Empfehlung: die Rümpfe dieser Socket-Handler in Funktionen auslagern, die einen "virtuellen Client"
    {userId, username} bekommen, und sie aus socketService UND aus dem neuen deviceGatewayService aufrufen.
    So bleibt eine einzige Logik, und der Browser verhält sich unverändert.
 4. state.rev erhöhen bei: Kanal angelegt/gelöscht/umbenannt, eigener join/leave/talk/volume, Änderung von
    'talkers' (Anzahl anderer User, die gerade in dem Kanal sprechen, aus speakingStatusChanged bzw.
    userMicMuteChanged). Long-Poll-Anfragen dann sofort beantworten.
 5. Timeout/Logout/neuer Login mit gleicher deviceId: aufräumen wie beim Socket-Disconnect (Producer, Consumer,
    Transport schließen, aus Mixern entfernen, User offline).
 6. Neue Dateien z. B. server/src/services/deviceGatewayService.js und server/src/routes/deviceGatewayRoutes.js,
    in server.js nur: httpApp.use(express.json()) (falls nötig) und httpApp.use('/device/v1', deviceGatewayRoutes).
 7. Windows-Firewall: TCP 3002 und UDP 40000-49999 eingehend erlauben (bitte prüfen/bestätigen).

Test ohne Gerät: In unserem Repo NR_Intercom liegt tools/mock_gateway.py (Python-Referenz derselben API, ohne
mediasoup). Damit sieht man das erwartete Verhalten. Ein echter Test geht mit einem kleinen Skript:
  1) curl http://<server>:3002/device/v1/hello
  2) curl -X POST -H "Content-Type: application/json" -d "{\"pin\":\"1234\",\"deviceId\":\"test01\",\"deviceName\":\"Test\",\"rtpPort\":5004,\"ssrc\":12345}" http://<server>:3002/device/v1/login
  3) curl "http://<server>:3002/device/v1/state?token=<token>&since=0&wait=0"
  4) POST /cmd join + talk 1, dann von einem zweiten PC Opus-RTP an <server>:<rtp.port> schicken:
     gst-launch-1.0 audiotestsrc is-live=true wave=sine freq=440 ! audioconvert ! audioresample ! opusenc bitrate=48000 frame-size=10 ! rtpopuspay pt=111 ssrc=12345 ! udpsink host=<SERVER-IP> port=<rtp.port> bind-port=5004
     -> Der 440-Hz-Ton muss im Browser bei Usern im selben Kanal hörbar sein.
     Rückweg prüfen: gst-launch-1.0 udpsrc port=5004 caps="application/x-rtp,media=audio,encoding-name=OPUS,clock-rate=48000,payload=100" ! rtpjitterbuffer ! rtpopusdepay ! opusdec ! autoaudiosink
     (payload= an den tatsächlichen PT anpassen) -> man hört den Mix der anderen.

Bitte NICHT ändern: Opus-Parameter des Routers, Mixer-Logik, Browser-Client, bestehende Socket-Events.

Zusätzlich sind beim Lesen des Codes folgende Fehler aufgefallen. Bitte prüfen, gerne als separater Commit/PR:
 a) server/src/services/audioMixerService.js:461: databaseService.getAllUserVolumes() liefert ein Array
    (databaseService.js:2314), dort wird aber .get() aufgerufen -> TypeError. Neue Sprecher werden im
    Python-Mixer-Modus daher nicht live hinzugefügt.
 b) server/src/services/socketService.js:2070: channelMemberships.get(userId). Die Map ist aber
    channelId -> Set<userId> (Zeile 32). Mixer werden beim Disconnect deshalb nie aufgeräumt.
 c) server/src/services/audioOutputGStreamerService.js:328: payload=96 ist fest eingetragen, der Router vergibt
    aber dynamisch (meist 100), und clock-rate=48000 fehlt in den Caps.
 d) Hinweis Sicherheit: register-user übernimmt die userId ohne Token-Prüfung. Das JWT-Secret hat einen
    Default-Wert in jwtService.js.

Am Ende bitte zurückmelden:
 - auf welchem Branch/Commit das liegt und welcher Port tatsächlich benutzt wird
 - ob die curl- und GStreamer-Tests funktioniert haben
 - ob 'talkers' umgesetzt ist oder vorerst immer 0 liefert
 - Abweichungen von DEVICE_GATEWAY_API.md (am besten keine)
```
