# Nachtrag 6 für den NR_Com-Chat: Sitzung fortsetzen statt neu anmelden

Den Block unten in den Chat des Com-Devs kopieren, die aktualisierte DEVICE_GATEWAY_API.md mitschicken
(neue Abschnitte "POST /resume" und "Gnadenfrist und Übernahme").

```text
Wichtige Anforderung aus dem Livebetrieb: Ein Bodypack darf sich bei einem Netzwechsel NIE ab- und wieder
anmelden. Sonst sehen und hören alle anderen bei jedem kurzen Funkloch, AP-Wechsel oder beim Einstecken in
die geplante Basisstation (Wechsel WLAN -> LAN, neue IP) "verlässt Kanal / tritt bei", inkl. Töne.

Bitte im Device-Gateway umsetzen (Details in DEVICE_GATEWAY_API.md):

1) Neuer Endpunkt  POST /device/v1/resume  {token, rtpPort, ssrc}
   - Token bekannt (auch wenn die Sitzung gerade "ruht", siehe 2): neue Geräte-Adresse übernehmen
     (IP aus req.socket.remoteAddress, rtpPort aus dem Body) und das Audio umleiten.
   - Kanäle, Talk-/Mute-Zustand, Lautstärken, Online-Status bleiben unverändert.
     KEINE Events an die Browser (kein leave/join, kein channel-users-update, kein Offline/Online, keine Töne).
   - Antwort 200 wie /login ({token, user, rtp:{port}, pollMs}) plus "resumed": true.
     Token gleich lassen oder neu ausgeben, beides ok. rtp.port darf sich ändern.
   - Token unbekannt / Gnadenfrist vorbei: 401 {"error":"token"} -> Gerät macht normalen /login.
   mediasoup-Hinweis: PlainTransport mit comedia:false kann connect() nur einmal. Bei neuer Adresse also
   intern einen neuen PlainTransport + Producer (gleiche ssrc/rtpParameters) + Consumer für den Mix
   anlegen und die alten erst danach schließen. Den neuen Producer in den Mixern so austauschen, dass
   für die anderen Benutzer KEIN Beitritt erkennbar ist (gleicher Weg wie beim internen Producer-Wechsel,
   ohne producerAddedToChannel-Broadcast an die UI). Bei unveränderter Adresse: nichts neu anlegen.

2) Gnadenfrist statt sofortigem Aufräumen
   - 15 s ohne Gateway-Aufruf: Sitzung "ruht" (RTP darf stehen), Benutzer bleibt online und in den Kanälen,
     keine Events.
   - Erst nach der Gnadenfrist (Standard 60 s, per Umgebungsvariable DEVICE_GRACE_SECONDS einstellbar)
     aufräumen wie bei einem Socket-Disconnect (so wie heute nach 15 s).
   - Optional, schön: in der UI ein kleines "Verbindung unterbrochen"-Symbol am Benutzer, solange die
     Sitzung ruht (ohne Ton).

3) Login mit derselben deviceId übernimmt die bestehende Sitzung an Ort und Stelle
   (z. B. nach einem Neustart oder Update des Geräts, dann ist das Token weg):
   neues Token, neue Adresse, Media wie bei 1) umziehen, Kanäle bleiben, KEINE leave/join/offline-Events.
   Heute wird die alte Sitzung erst aufgeräumt und dann neu angelegt -> das löst genau diese Events aus.

4) /logout bleibt das einzige echte Abmelden (Gerät meldet sich nur ab, wenn der Benutzer am Gerät
   Server/PIN ändert oder den Intercom ausschaltet).

Test: Bodypack im Kanal, Talk an, Browser-User im selben Kanal.
  a) WLAN am Router kurz trennen (10 s) -> im Browser darf nichts passieren, danach geht Audio weiter.
  b) Bodypack neu starten (oder Update über WLAN) -> kein Verlassen/Beitreten im Browser.
  c) Bodypack 70 s ausschalten -> erst dann wird es abgemeldet.
Unser Mock (tools/mock_gateway.py im NR_Intercom-Repo) hat das Verhalten als Referenz eingebaut.

Bitte zurückmelden: Branch/Commit, Ergebnis der Tests a-c.
```
