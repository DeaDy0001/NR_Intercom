# Prompt für den NR_Com-Chat (netrock-com)

Den Block unten komplett kopieren und in den Chat des Com-Devs einfügen.

---

```text
Kontext: Wir bauen ein Hardware-Bodypack (ESP32-S3 + WM8960-Codec, 4 Taster, 2 Drehgeber),
das sich in NR_Com wie ein normaler Benutzer verhalten soll (eigener User, z. B. "Bodypack 1").
Ein ESP32 kann kein vollständiges WebRTC (ICE + DTLS-SRTP) vernünftig umsetzen. Deshalb soll das Gerät
über einen mediasoup-PlainTransport einfaches RTP (Opus 48 kHz) senden und empfangen.
Alles andere bleibt, wie es ist: Login über /api/projects/:id/auth/pin, Socket.IO, register-user,
client-heartbeat, produce, joinChannel (mixedProducerId), consume, setChannelMicMute,
setChannelSettings und mixerRebuilt werden vom Gerät unverändert benutzt.
Alle Transporte liegen in mediaService in derselben Map. produce/consume funktionieren also auch
mit einer PlainTransport-ID.

Aufgabe (bitte minimal-invasiv, der Browser-Flow darf sich NICHT ändern):

1. Neues Socket.IO-Event mit Ack in server/src/services/socketService.js:
     socket.on('createPlainTransport', async (data, callback) => { ... })
   - Erzeugt über mediaService.createPlainTransport() einen Transport mit
       listenIp: { ip: '0.0.0.0', announcedIp: <dieselbe LAN-IP wie bei createWebRtcTransport (getLocalIP)> }
       rtcpMux: true, comedia: true, enableSrtp: false
     Port automatisch aus dem Worker-Bereich (WEBRTC_MIN_PORT..MAX_PORT, 40000-49999), KEIN fester Port.
   - Antwort: { id, ip: <announcedIp>, port: transport.tuple.localPort }
     Bei Fehler: { error: message } (wie bei den anderen mediasoup-Events).
   - Der Transport wird dem Socket bzw. User zugeordnet und beim Disconnect bzw. Heartbeat-Timeout
     geschlossen, genauso wie die WebRTC-Transports.
   - Achtung: mediaService.createPlainTransport hat 127.0.0.1 als Default. Das darf für den internen
     Mixer so bleiben. Nur das neue Event übergibt 0.0.0.0 + announcedIp.

2. Optional, zusätzlich: Event 'connectPlainTransport' { transportId, ip, port } -> transport.connect({ ip, port })
   -> Ack { success: true }. Das ist für den Fall comedia:false gedacht. Mit comedia:true lernt mediasoup die
   Geräteadresse aus dem ersten RTP-Paket, das das Gerät sendet.

3. Prüfen, dass
   - 'produce' mit transportId eines PlainTransports funktioniert (rtpParameters vom Gerät:
     codecs:[{mimeType:'audio/opus', payloadType:111, clockRate:48000, channels:2,
     parameters:{useinbandfec:1}, rtcpFeedback:[]}], encodings:[{ssrc:<zufällig>}],
     rtcp:{cname:'esp32-xx', reducedSize:true}) und channelIds wie beim Browser registriert werden.
   - 'consume' auf dem PlainTransport funktioniert und im Ack die vollständigen rtpParameters
     zurückgibt (das Gerät braucht codecs[0].payloadType und encodings[0].ssrc).
   - Der mixedProducerId-Stream per RTP an die per comedia gelernte Geräteadresse geht.

4. Test ohne ESP (von einem zweiten PC aus, simuliert das Gerät), nach createPlainTransport/produce/consume
   z. B. über ein kleines Node-Skript mit socket.io-client:
     Senden:    gst-launch-1.0 audiotestsrc is-live=true wave=sine freq=440 ! audioconvert ! audioresample !
                opusenc bitrate=64000 frame-size=20 ! rtpopuspay pt=111 ssrc=12345 !
                udpsink host=<SERVER-IP> port=<port aus createPlainTransport> bind-port=5004
     Empfangen: Wegen comedia kommt der Mix an genau dieser Quelladresse zurück (Port 5004 vom Sender).
                Am einfachsten im Testskript beide Richtungen über denselben UDP-Socket abwickeln.
   Erwartung: Der Ton ist im Browser bei Usern im selben Kanal hörbar, und der Mix der anderen kommt per RTP zurück.

5. Kurze Doku als docs/DEVICE_CLIENT_API.md: Ablauf (REST-Login -> Socket.IO -> register-user ->
   client-heartbeat -> createPlainTransport -> produce -> joinChannel -> setChannelMicMute ->
   consume mixedProducerId -> mixerRebuilt-Handling) mit den genauen Payloads.

6. Windows-Firewall: Eingehendes UDP 40000-49999 muss erlaubt sein. Das ist für WebRTC wahrscheinlich schon so,
   bitte bestätigen.

Bitte NICHT ändern: Opus-Parameter des Routers, Mixer-Logik, Browser-Client.

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
 - Name und Payload der neuen Events (final)
 - auf welchem Branch/Commit das liegt
 - ob der Test aus Punkt 4 funktioniert hat
```
