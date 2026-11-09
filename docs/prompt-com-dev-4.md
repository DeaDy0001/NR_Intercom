# Nachtrag 4 für den NR_Com-Chat: Rückweg ist still + Sprechanzeige

Den Block unten in den Chat des Com-Devs kopieren (aktualisierte DEVICE_GATEWAY_API.md mitschicken).

```text
Test mit deinem Branch claude/inspiring-mccarthy-0eujmm: Danke, der doppelte Consumer ist weg
(am Gerät: 0 fremde Pakete, 0 Stromwechsel). Bodypack -> Browser funktioniert jetzt.

Zwei offene Punkte:

1) Rückweg Server -> Bodypack ist STILL.
   Das Gerät empfängt den Mix lückenlos (7709 Pakete, ein Strom, Jitter 0,3 ms, keine Verluste),
   die Pakete sind im Mittel 56 Byte groß, dekodieren aber zu ca. -84 dBFS, also Stille.
   Gleichzeitig spricht ein Browser-Benutzer (anderer Benutzer als das Bodypack) im selben Kanal mit
   offenem Mikrofon. Der Decoder im Gerät ist geprüft (Mock-Server-Echo kommt hörbar an).
   Bitte prüfen, während der Browser-User spricht:
    - GET /device/v1/status: enthält audio.mixer.inputs des Bodypack-Users den Producer des
      Browser-Users? Ist der dort stummgeschaltet (Volume 0, setUserMicMuted, channel_volume)?
    - Ist es wirklich der Mix-Minus DES BODYPACK-USERS, der konsumiert wird (nicht z. B. ein leerer
      Mixer eines anderen Users, oder ein Mixer, der vor dem join des Browser-Users gebaut wurde und
      dessen Producer nicht live hinzugefügt bekommt)?
    - Hört ein ZWEITER Browser-User im selben Kanal den ersten? (Dann ist der Mixer an sich ok und es
      liegt am Geräte-Pfad.)
    - Kommt beim Gerät nach einem neuen join (mixerRebuilt) Ton an?
   Hinweis: In Fehler a) aus dem ersten Prompt ging es genau um "neuer Sprecher wird nicht live in
   laufende Mixer eingefügt". Falls das im Python-Mixer-Modus noch irgendwo hakt, würde das passen.

2) Sprechanzeige (grünes Licht) für das Gerät.
   Der Browser meldet 'speakingStatusChanged' aus seinem eigenen Gate, das Gerät konnte das bisher nicht.
   Neu in der API (DEVICE_GATEWAY_API.md, Tabelle /cmd):
      POST /device/v1/cmd {"token":"…","cmd":"speaking","value":1|0}      (ohne "channel")
   Das Gerät schickt 1, solange Talk an ist und sein Mikrofon-Gate offen ist, sonst 0, nur bei Änderung.
   Bitte genauso behandeln wie 'speakingStatusChanged {isSpeaking}' (gleiche Broadcasts
   user-speaking-in-channel / user-stopped-speaking-in-channel, gleiche Wirkung auf 'talkers' bei anderen
   Geräten). Bei Logout/Timeout und wenn Talk in allen Kanälen aus ist: automatisch "spricht nicht".
   Antwort wie bei den anderen Befehlen: {"ok":true,"rev":…}.

Bitte zurückmelden: Ursache für 1), Branch/Commit, und die Zeile audio.mixer aus /device/v1/status
während der Browser-User spricht.
```
