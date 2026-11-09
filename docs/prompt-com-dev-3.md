# Nachtrag 3 für den NR_Com-Chat: doppelter Mix-Strom zum Bodypack

```text
Erster echter Test Bodypack <-> NR_Com: Login, Kanäle, Talk und Senden funktionieren. Mikrofon kommt im
Browser an. Rückweg: Das Bodypack empfängt RTP, aber laut seiner Statistik kommen ZWEI Ströme
gleichzeitig an (unterschiedliche SSRC, abwechselnde Pakete): 12095 Pakete empfangen, 12089 davon
mit einem Sequenzsprung. Das ergibt ziemlich genau zwei verschränkte Ströme mit gleicher Paketrate.

Vermutung: Auf dem PlainTransport des Geräts gibt es zwei Consumer für den Mix, z. B.
 - pro 'join' wird ein neuer Consumer für mixedProducerId angelegt, obwohl schon einer existiert, oder
 - nach 'mixerRebuilt' wird der alte Consumer nicht geschlossen.

Bitte prüfen (z. B. über GET /device/v1/status, wie viele Consumer pro Sitzung existieren) und
sicherstellen:
 1. Pro Gerätesitzung gibt es zu jedem Zeitpunkt GENAU EINEN Consumer auf dem PlainTransport.
 2. Bei join: Ist der mixedProducerId unverändert und existiert schon ein Consumer -> nichts tun.
    Hat er sich geändert -> erst den alten Consumer schließen, dann den neuen anlegen.
 3. Bei mixerRebuilt: alten Consumer schließen, dann neu anlegen (nicht parallel).
 4. In /device/v1/status pro Sitzung die Consumer-Anzahl und deren SSRC mit ausgeben.

Das Gerät spielt ab sofort nur noch einen Strom (das erste SSRC; ein anderes erst, wenn das erste
300 ms still ist). Der doppelte Strom kostet aber Funkzeit und RAM (bei 20 Bodypacks spürbar)
und sollte deshalb serverseitig weg.

Bitte zurückmelden: Ursache, Branch/Commit, und was /device/v1/status jetzt pro Sitzung zeigt.
```
