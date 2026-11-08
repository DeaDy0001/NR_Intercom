# Nachtrag für den NR_Com-Chat: Server-Suche + Rückmeldung

Den Block unten in den Chat des Com-Devs kopieren. Die aktualisierte
[device-gateway-api.md](device-gateway-api.md) (neuer Abschnitt 1a) bitte mitschicken.

---

```text
Danke, das passt alles sehr gut. Rückmeldung von der Geräteseite:

- Aufräumen wie der echte Disconnect (gespeicherte Kanäle bleiben): genau richtig so.
- Zusätzliche Fehlercodes beim Login (400 deviceId/rtpPort, 503 audio): passt, das Gerät zeigt den
  Text aus "error" jetzt am Display an.
- Port je nach SSL (3002 oder Hauptport): Damit niemand am Bodypack IP und Port mit Drehreglern
  eintippen muss, bitte noch eine kleine Server-Suche einbauen (Abschnitt 1a in DEVICE_GATEWAY_API.md):

Server-Suche per UDP-Broadcast:
 1. Neuer UDP-Socket auf 0.0.0.0:3010 (z. B. in deviceGatewayService beim Start, Port per
    Umgebungsvariable DEVICE_DISCOVERY_PORT überschreibbar, Standard 3010).
 2. Kommt ein Datagramm, das mit "NRCOM_DISCOVER" beginnt, antwortet der Server per Unicast an
    Absender-IP und -Port mit genau diesem JSON (UTF-8, ein Datagramm):
      {"nrcom":1,"api":1,"name":"NR_Com","project":{"id":<id>,"name":"<Projektname>"},"port":<HTTP-Port>}
    "port" = der Port, an dem /device/v1 TATSÄCHLICH erreichbar ist (SSL an: HTTP_PORT, SSL aus: PORT).
    Andere Datagramme ignorieren. Kein aktives Projekt: trotzdem antworten, project = null.
 3. Bei mehreren Netzwerkkarten reicht der eine Socket auf 0.0.0.0, die Antwort geht über die Karte,
    auf der die Anfrage kam.
 4. Windows-Firewall: UDP 3010 eingehend erlauben (zusätzlich zu TCP 3002 und UDP 40000-49999).
    Wenn ihr ein Installer-/Setup-Skript mit Firewall-Regeln habt, bitte dort ergänzen.
 5. Test von einem zweiten PC:
      python -c "import socket as S;s=S.socket(S.AF_INET,S.SOCK_DGRAM);s.setsockopt(S.SOL_SOCKET,S.SO_BROADCAST,1);s.settimeout(2);s.sendto(b'NRCOM_DISCOVER 1',('255.255.255.255',3010));print(s.recvfrom(512))"
    (unter Windows: py statt python) -> muss das JSON mit dem richtigen Port liefern.
 6. Optional in GET /device/v1/status mit ausgeben, dass die Suche läuft und auf welchem Port.

Der mDNS-Name (nr-com.local) bleibt als Rückfallebene, daran bitte nichts ändern.

Am Ende bitte zurückmelden: Branch/Commit, ob der Test aus Punkt 5 funktioniert hat, und auf welchem
Branch das Gateway insgesamt liegt (wir testen dann mit dem echten Bodypack gegen den echten Server).
```
