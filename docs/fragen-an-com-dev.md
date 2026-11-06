# Fragen an den NR_Com-Entwickler

Ziel: Das ESP32-Bodypack soll sich gegenüber NR_Com wie ein normaler (PC-)Client verhalten.
Dafür muss die Firmware dasselbe Audio- und Steuerprotokoll sprechen.

**Am einfachsten:** Zugriff auf das NR_Com-Repository (GitHub), dann kann ich mir das meiste selbst ansehen.
Ansonsten bitte die folgenden Punkte beantworten:

## 1. Audio-Transport
1. Welche **GStreamer-Pipelines** benutzt ihr zum Senden und zum Empfangen? (bitte als Text, z. B. `gst-launch`-Zeilen oder Code)
2. Transport: **RTP über UDP**? Oder etwas anderes (SRT, WebRTC, TCP, eigenes Format)?
3. **Unicast oder Multicast?** Welche IP-Adressen und Ports?
4. **Codec:** Opus, PCM (L16), G.711 oder etwas anderes? Bitrate, **Abtastrate** (48 kHz / 16 kHz?), **Mono/Stereo**, Paketgröße (10/20 ms)?
5. RTP-Details: Payload-Type, SSRC-Vergabe, gibt es RTCP?
6. Jitter-Buffer und Latenz: welche Werte nutzt ihr? Was ist das Ziel (z. B. < 100 ms)?

## 2. Kanäle und Mischen
7. Wie ist ein **„Kanal“** umgesetzt? (eigene Multicast-Gruppe, eigener Port, eigene SSRC, Raum am Server …)
8. **Wer mischt?** Mischt der PC/Server und schickt jedem Client einen fertigen Mix (ein Stream)?
   Oder bekommt jeder Client alle Kanäle einzeln und mischt selbst?
   *(Für den ESP32 ist „Server mischt“ deutlich besser: 1 Decoder statt N.)*
9. Gibt es Mix-Minus (man hört sich selbst nicht zurück)?
10. Wie viele Kanäle gleichzeitig: hören / sprechen?

## 3. Steuerung / Signalisierung
11. Wie meldet sich ein Client an, wie wählt er Kanäle, wie wird **Sprechen (PTT/Talk)** und **Hören (Listen)** signalisiert?
    (WebSocket, REST/HTTP, OSC, MQTT, eigenes UDP-Protokoll, SIP …)
12. Nachrichtenformat (JSON? Beispiele?)
13. Gibt es „Call“-Signale / Ruftasten, Tally, Benutzernamen, Statusanzeigen?
14. Wie findet ein Client den Server: feste IP, mDNS/Bonjour, Broadcast?

## 4. Sonstiges
15. Verschlüsselung/Authentifizierung (SRTP, DTLS, Passwort, Token)?
16. Gibt es einen zentralen Server (PC), oder läuft alles Peer-to-Peer?
17. Läuft NR_Com später auch unter Linux, oder bleibt Windows die Basis?
18. Gibt es eine Testumgebung (z. B. ein Kommandozeilen-Client oder eine Test-Pipeline), die ich nachbauen kann?
