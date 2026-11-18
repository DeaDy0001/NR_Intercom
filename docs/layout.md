# Bodypack-Platine V0.3: Hinweise fürs Layout

Stand 15.11.2026. Bezeichnungen wie im EasyEDA-Schaltplan.

Annahme: **4-Lagen-Platine, außen 1 oz (35 µm), innen 0,5 oz (17 µm)**, so wie bei den meisten Fertigern Standard.
Lage 2 ist eine durchgehende GND-Fläche, Lage 3 die Versorgungen.

---

## 1. Leiterbahnbreiten (ausführlich, von dick nach dünn)

Gerechnet nach IPC-2221 für 10 °C Erwärmung, mit Reserve. Alle Breiten gelten für die **Außenlagen (35 µm)**.
Auf den Innenlagen (17 µm) braucht man die 4- bis 5-fache Breite. Deshalb alles ab 0,6 mm **außen** führen,
oder innen als Fläche.

### 1,5 mm oder Kupferfläche (2–3 A)

| Netz | Verbindung | Strom |
|---|---|---|
| VBUS | J1 A4B9 + B4A9 → F1 → U15 Pin 6 | 2,5 A |
| Station 5 V | J7 Pin 2 + Pin 8 → C46 → U15 Pin 3 | 2,5 A |
| Lader-Eingang | U15 Pin 2 + Pin 7 → C3 → U3 Pin 1 | 2,5 A |
| PMID | U3 Pin 23 → C4 | 3 A Spitze |
| SW (Lader) | U3 Pin 19 + 20 → L1 (und C6), **so kurz wie möglich** | 3 A Spitze |
| SYS am Lader | L1 → U3 Pin 15 + 16 → C7_1, C7_2 | 3 A |
| Akku plus | BT1 + / BT2 + → C8 → U3 Pin 13 + 14 | 2 A |
| Akku minus | BT1 − / BT2 − → Q1 Pin 1 | 2 A |
| GND hinter dem Schutz | Q1 Pin 3 → GND, **3–4 Vias** | 2 A |

### 1,0 mm (bis 1,5 A)

| Netz | Verbindung | Strom |
|---|---|---|
| VSYS Hauptleitung | von C7_1/C7_2 zu den Abzweigen unten | 1,5 A |

Alternativ als Fläche auf Lage 3.

### 0,8 mm (ca. 1 A)

| Netz | Verbindung | Strom |
|---|---|---|
| VSYS → 3,3-V-Wandler | Hauptleitung → C11_1/C11_2 → U6 Pin 10 + 11 (und Pin 12, 13 dünn) | 1 A |
| Schaltknoten U6 | U6 Pin 8 + 9 → L2 → U6 Pin 6 + 7, kurz | 1,2 A Spitze |
| +3V3 am Wandler | U6 Pin 4 + 5 → C12_1 – C12_3 | 0,8 A |

### 0,6 mm (ca. 0,5 A)

| Netz | Verbindung | Strom |
|---|---|---|
| VSYS → 48 V | Hauptleitung → C70_1/C70_2 → L5 | 0,4 A |
| Schaltknoten 48 V | L5 → Q4 Pin 3 → D7 → C72 | 0,5 A Spitze |
| Strommessung 48 V | Q4 Pin 2 → R81 → GND (2 Vias) | 0,5 A Spitze |
| Verdoppler | D7 → C71 → D8 → C72 → D9 → C73 | Ladestöße bis 0,5 A |
| VSYS → ±6,5 V | Hauptleitung → C61 → U7 Pin 5 + 6, und → L3 → U7 Pin 1 + 24 → D5 | 0,5 A |
| Schaltknoten minus | U7 Pin 13 + 14 → L4 und D6 | 0,5 A Spitze |
| VSYS → S3-Board | Hauptleitung → J3/J4 Pin 14 | 0,5 A |
| +3V3 zum C5 | C12 → C44 → J2 Pin 3 (am Rand, nicht unter dem Analogteil) | 0,4 A |

**48-V-Teil: zusätzlich 0,5 mm Abstand** zu allen anderen Leitungen (P24, DBL, P48_RAW, Schaltknoten L5/Q4/D7).

### 0,4 mm (0,1–0,3 A oder kurze Stromspitzen)

| Netz | Verbindung |
|---|---|
| Gate 48 V | U8 Pin 3 → Q4 Pin 1, kurz (Treiberstrom 1,5 A für wenige ns) |
| BIAS/VCC 48 V | U8 Pin 1 + 2 → C70_1/C70_2 |
| ±6,5 V | D5 → C62 → U17 Pin 8 + 5; D6 → C64 → U18 Pin 8 + 5 |
| +5VA | U17 Pin 1 → C76 → U11 Pin 11, U13 Pin 2, U19 Pin 1 + 3, C28, C32, C33, C78, C85 |
| −5VA | U18 Pin 1 → C77 → U11 Pin 6, U13 Pin 4 + Pad, C34, C35, C79, C80 |
| Kopfhörer | U13 Pin 9 → R31 → J6 Pin 1 + 2; U13 Pin 7 → R32 → J6 Pin 3 + 4 |
| Kopfhörer-Masse | J6 Pin 5 + 6 → GND (2 Vias) |
| +3V3 Verteilung | C12 → U9 Pin 6 / 30, U10, U16, U2-Umgebung |

±5VA kann auch als Fläche auf Lage 3 laufen.

### 0,3 mm (unter 0,1 A)

| Netz | Verbindung |
|---|---|
| VSYS → LEDs | je LED Pin 3 mit C40–C43 |
| VSYS → Wandler-Steuerpins | U6 Pin 12, 13, R10; R74 → U7 Pin 4 |
| 3V3A | U19 Pin 5 → C86 → C18 → U9 Pin 26 |
| P48 | Q2 Pin 3 → R13 → C16 → R17 / R18 (0,5 mm Abstand!) |
| Hub-Versorgung | VBUS → C1 → U2 Pin 19 |
| BAT+ Abzweige | R8 → U4 Pin 5; eigene Leitung zu U5 Pin 2 + 3 |
| BAT− Abzweig | U4 Pin 6, C9 |
| U8 Pin 4/7, U7 Pin 2/3/19 | direkt per Via auf GND |

### 0,2 mm (alles andere)

I2C, I2S, UART, SPI zur Station, Taster, Drehgeber, LED-Daten, alle Steuerleitungen (ANA_EN, P48_EN, HP_EN, ...),
Pull-ups, Rückführungen (FB), Kompensation, RT, SS, VREF, NR/SS, Quarz.

### Durchkontaktierungen

| Strom | Vias (0,3 mm Bohrung) |
|---|---|
| bis 0,5 A | 1 |
| 1 A | 2 |
| 2–3 A | 3–4 |
| GND-Pad eines Leistungs-Kondensators | 2–3, direkt am Pad |
| Thermisches Pad (U3, U6, U7, U8, U9, U17, U18, U13) | 5–9 im Pad |

### Abstände

- 48-V-Teil: **mind. 0,5 mm** zu allem anderen.
- sonst: Minimum des Fertigers (0,15–0,2 mm), bei Leistungsleitungen besser 0,3 mm.

## 1b. Aufbau 1 oz außen / 2 oz innen: welche Leitung auf welcher Lage

Lage 1 (oben) und Lage 4 (unten): 1 oz. Inner1 (Lage 2): GND-Ebene, 2 oz. Inner2 (Lage 3): Signal, 2 oz.

| Netz | Verbindung | Strom | Lage 1 / 4 (1 oz) | Inner2 (2 oz) | Empfohlen |
|---|---|---|---|---|---|
| USB-C 5 V | J1 → F1 → U15 Pin 6 | 2,5 A | 1,5 mm | 1,0 mm | Inner2 |
| Station 5 V | J7 Pin 2 + 8 → C46 → U15 Pin 3 | 2,5 A | 1,5 mm | 1,0 mm | Inner2 |
| Lader-Eingang | U15 Pin 2 + 7 → C3 → U3 Pin 1 | 2,5 A | 1,5 mm | 1,0 mm | Inner2, kurze Stücke außen |
| Akku plus | BT1/BT2 + → C8 → U3 Pin 13 + 14 | 2 A | 1,5 mm | 0,8 mm | Inner2 |
| Akku minus (BAT−) | BT1/BT2 − → Q1 Pin 1 | 2 A | 1,5 mm | 0,8 mm | Inner2, **nicht mit GND verbinden** |
| GND hinter Q1 | Q1 Pin 3 → GND-Ebene | 2 A | Fläche | – | außen, 3–4 Vias in Inner1 |
| PMID | U3 Pin 23 → C4 | 3 A Spitze | Fläche | – | **nur außen**, so kurz wie möglich |
| Schaltknoten Lader | U3 Pin 19 + 20 → L1 → C6 | 3 A Spitze | Fläche | – | **nur außen**, so kurz wie möglich |
| SYS am Lader | L1 → U3 Pin 15 + 16 → C7_1/C7_2 | 3 A | 1,5 mm / Fläche | – | außen, wenige mm |
| VSYS Hauptleitung | C7 → Abzweige | 1,5 A | 1,0 mm | 0,6 mm | Inner2 |
| VSYS → U6 | → C11_1/C11_2 → U6 Pin 10 + 11 | 1 A | 0,8 mm | 0,4 mm | Inner2, am Ende außen an C11 |
| Schaltknoten 3,3 V | U6 Pin 8 + 9 → L2 → Pin 6 + 7 | 1,2 A Spitze | 0,8 mm | – | **nur außen** |
| +3V3 am Wandler | U6 Pin 4 + 5 → C12_1–C12_3 | 0,8 A | 0,8 mm | – | außen |
| VSYS → 48 V | → C70 → L5 | 0,4 A | 0,6 mm | 0,4 mm | Inner2 oder außen |
| Schaltknoten 48 V, Verdoppler | L5 → Q4 → D7 → C71/C72 → D8/D9 → C73 | 0,5 A Spitze | 0,6 mm | – | **nur außen**, 0,5 mm Abstand |
| Strommessung 48 V | Q4 Pin 2 → R81 → GND | 0,5 A Spitze | 0,6 mm | – | außen, 2 Vias in Inner1 |
| VSYS → ±6,5 V | → C61 → U7 Pin 5 + 6, → L3 → U7 Pin 1 + 24 | 0,5 A | 0,6 mm | 0,4 mm | Zuleitung Inner2, Schleifen außen |
| Schaltknoten ±6,5 V | U7 Pin 1/24 → D5, Pin 13/14 → L4/D6 | 0,5 A Spitze | 0,6 mm | – | **nur außen** |
| VSYS → S3-Board | → J3/J4 Pin 14 | 0,5 A | 0,6 mm | 0,4 mm | Inner2 |
| +3V3 → C5 | C12 → C44 → J2 Pin 3 | 0,4 A | 0,6 mm | 0,3 mm | Inner2, nicht unter dem Analogteil |
| Gate 48 V | U8 Pin 3 → Q4 Pin 1 | Spitzen | 0,4 mm | – | außen, kurz |
| ±6,5 V | D5/C62 → U17 Pin 8 + 5; D6/C64 → U18 Pin 8 + 5 | 0,1 A | 0,4 mm | 0,3 mm | beides möglich |
| +5VA / −5VA | U17/U18 Pin 1 → U11, U13, U19 | 0,1 A | 0,4 mm | 0,3 mm | Inner2 oder Fläche |
| Kopfhörer | U13 Pin 9 → R31 → J6; Pin 7 → R32 → J6 | 0,1 A | 0,4 mm | 0,3 mm | außen |
| +3V3 Verteilung, 3V3A, LEDs, P48, Hub | siehe Abschnitt 1 | < 0,1 A | 0,3 mm | 0,3 mm | beliebig |
| alle Signale | | – | 0,2 mm | 0,2 mm | beliebig, nicht auf Inner1 |

An den Pins kleiner ICs (U3, U15, Q1) wird die Leitung auf den letzten 1–2 mm auf Pad-Breite schmal. Kupferflächen
dort direkt anbinden (ohne Wärmefallen), Vias für den Lagenwechsel direkt neben das Pad, 3–4 Stück bei 2–3 A.

## 2. Besondere Leitungen

| Leitung | Was zu beachten ist |
|---|---|
| **USB von J1 zu U2 (D+/D−)** | Kann mit 480 Mbit/s laufen. Als **Paar mit 90 Ω** führen: Breite und Abstand mit dem Impedanz-Rechner des Fertigers für den gewählten Lagenaufbau bestimmen. Beide Leitungen gleich lang (±0,5 mm), möglichst ohne Lagenwechsel, durchgehende GND-Fläche darunter. U1 direkt hinter J1. |
| USB von U2 zum C5 (J2) und zum S3 (J3/J4) | nur 12 Mbit/s, unkritisch. Trotzdem als Paar nebeneinander führen. |
| **R81 (68 mΩ, Strommessung 48 V)** | Die Leitung zu R86 / CS (U8 Pin 5) **direkt am Pad von R81** abgreifen, nicht irgendwo auf der Stromleitung. Die GND-Seite von R81 auf kurzem Weg zu U8 Pin 4 (PGND). |
| **Rückführungen (FB)** | R11/R12 (48 V), R70–R73 (±6,5 V), R75–R78 (±5 V): die Teiler **nahe an den FB-Pin** setzen, die lange Leitung geht zur Ausgangsspannung. Nicht neben Schaltknoten führen. |
| **Mikrofon** | J5 → R17/R18 → C23/C24 → R19/R20 → U11 Pin 4/5 als **symmetrisches Paar**, beide Adern gleich lang, spiegelbildlich bestückt. Weit weg von allen Wandlern und vom C5. |
| **RG1** | direkt zwischen U11 Pin 3 und Pin 12, kurze Leitungen. |
| **Quarz Y2** | höchstens 5 mm von U2 Pin 3/4, keine anderen Leitungen darunter oder dazwischen. |
| **Antenne C5** | kein Kupfer und keine Leitungen unter und neben der Antenne (alle Lagen), Antennenkabel weg vom Mikrofoneingang. |

## 3. Kondensatoren: wie nah an den Pin?

Grundregel für alle Kondensatoren: **die GND-Seite mit eigenem Via direkt am Pad** in die GND-Fläche. Bei den Leistungs-Kondensatoren
2–3 Vias. Der Weg „IC-Pin → Kondensator → Via → GND-Fläche → GND-Pin des IC“ ist die eigentliche Schleife, die klein sein muss.

### Stufe 1: extrem wichtig, direkt am Pin (unter 1–2 mm, auf derselben Lage wie das IC)

Hier fließen schnelle, große Stromspitzen. Zu weit weg heißt: Störungen, Spannungsspitzen, im schlimmsten Fall ein
instabiler Wandler oder ein zerstörtes IC.

| IC | Kondensator | an Pin | Grund |
|---|---|---|---|
| **U3 BQ25895** (Lader) | **C4** 22 µF | 23 PMID ↔ 17/18 PGND | Eingangskondensator des Abwärtswandlers, wichtigster Kondensator am Lader |
| | **C7_1, C7_2** 10 µF | 15/16 SYS ↔ PGND | Ausgang des Wandlers |
| | **C6** 47 nF | 21 BTST ↔ 19/20 SW | Treiber des oberen Schalters |
| | C5 4,7 µF | 22 REGN ↔ GND | interne Versorgung |
| **U6 TPS63021** (3,3 V) | **C11_1, C11_2** 10 µF | 10/11 VIN ↔ 15 PGND | Eingangsseite |
| | **C12_1–C12_3** 22 µF | 4/5 VOUT ↔ PGND | Ausgangsseite, beide Seiten schalten hart |
| | L2 | direkt an Pin 6–9 | |
| **U7 TPS65131** (±6,5 V) | **C61** 4,7 µF | 5/6 INN ↔ 2/3 PGND | Eingang des Minus-Wandlers |
| | **C62** 22 µF | Kathode D5 ↔ PGND | Ausgang des Plus-Wandlers: Schleife Pin 1/24 → D5 → C62 → PGND klein halten |
| | **C64** 22 µF | Anode D6 ↔ GND | Ausgang des Minus-Wandlers: Schleife Pin 13/14 → D6 → C64 → GND klein halten |
| **U8 LM5155** (48 V) | **C70_1, C70_2** | 1 BIAS / 2 VCC ↔ 4 PGND | versorgt den Gate-Treiber, Stromspitzen 1,5 A |
| | **Q4, D7, C71, R81** | als kompakte Gruppe | Schaltschleife Q4 → D7 → C71 → GND → R81 → Q4 so klein wie möglich |
| **U9 Codec** | **C21** 10 µF | 29 DVDD ↔ 28 DVSS | Ausgang des internen Reglers, sonst schwingt er |
| | **C20** 10 µF | 24 AVDD ↔ 17 AVSS | wie oben, Analogteil |
| **U17 / U18** (±5 V) | **C76 / C77** 10 µF | 1 OUT ↔ GND | Stabilität des Reglers |
| **U19 LP5907** | **C85, C86** 1 µF | 1 IN / 5 OUT ↔ 2 GND | Stabilität |
| **U11 INA163** | **C78** 100 nF / **C80** 100 nF | 11 V+ / 6 V− ↔ GND | Verstärker mit hoher Bandbreite |
| **U13 OPA1622** | **C33** 100 nF / **C35** 100 nF | 2 V+ / 4 V− ↔ GND | Verstärker mit hohen Ausgangsströmen |
| **U2 CH334F** | **C2** 1 µF | 20 VDD33 ↔ 25 GND | interner 3,3-V-Regler |
| **U15 TPS2116** (neu) | **C89** 1 µF / **C90** 1 µF | 6 VIN2 / 3 VIN1 ↔ GND-Via | dämpft Spannungsspitzen beim Einstecken (USB, Federkontakte Station). Leitung erst über den Kondensator, dann zum Pin |

### Stufe 2: wichtig, nah am Pin (2–5 mm)

Wenig Strom, aber empfindlich gegen Einstreuung. Weg von Schaltknoten, kurze Leitung zum Pin.

| IC | Kondensator | an Pin |
|---|---|---|
| U9 Codec | **C19** 10 µF | 18 REF: bestimmt das Rauschen des Codecs, möglichst ruhige Stelle |
| | C18 100 nF + C86 1 µF | 26 LDOIN |
| | C17 10 µF | 6 IOVDD |
| U17 / U18 | **C81 / C82** 10 nF | 6 NR/SS: Rauschfilter, Leitung kurz und weg von Wandlern |
| | C83 / C84 10 nF | parallel zu R75 / R77, nahe am FB-Pin 2 |
| | C62 / C64 (zugleich Eingang der LDOs) | 8 IN, wenn U17/U18 nicht direkt neben U7 sitzen: zusätzlich 1 µF direkt an Pin 8 vorsehen |
| U7 TPS65131 | C60 100 nF | 4 VIN |
| | **C63** 220 nF | 17 VREF (Referenz für −6,5 V) |
| | C66 / C67 | 21 CP / 18 CN (Regelung), zu AGND Pin 19 |
| U8 LM5155 | C87 1 nF, R85 + C75 | 6 COMP, zu AGND Pin 7 |
| | C74 47 nF | 9 SS |
| | R80 | 10 RT |
| U6 TPS63021 | C7 (VINA) 100 nF | 1 VINA |
| U3 BQ25895 | C3 1 µF | 1 VBUS |
| | C8 10 µF | 13/14 BAT |
| U11 INA163 | C28 / C79 10 µF | neben C78 / C80 |
| U13 OPA1622 | C32 / C34 10 µF | neben C33 / C35 |
| U2 CH334F | C1 10 µF | 19 V5 |
| | Y2 | 3 XO / 4 XI (siehe oben) |
| U4 DW01A | C9 100 nF, R8 | 5 VCC |
| U15 TPS2116 | C46 / C1 (Eingänge), C3 (Ausgang) | 3 VIN1, 6 VIN2, 2/7 VOUT |
| Mikrofon-Filter | **C25, C26, C27** | direkt an U11 Pin 4/5, symmetrisch: Sie halten WLAN-Störungen vom Eingang fern, das wirkt nur direkt am Pin |

### Stufe 3: unkritisch, irgendwo in der Nähe (5–20 mm reichen)

| Kondensator | Funktion |
|---|---|
| C10 (MAX17048), C22 (U10), C45 (U16) | Entkopplung langsamer Digitalbausteine |
| C40–C43 | je LED, in der Nähe der LED (ca. 5 mm) |
| C44 | 3,3 V am C5-Modul, nahe J2 |
| C16 47 µF | Filter der Phantomspeisung, irgendwo zwischen R13 und R17/R18 |
| C23, C24 47 µF | Koppel-Elkos Mikrofon: Abstand egal, aber **beide gleich und spiegelbildlich** |
| C29, C30, C31 | Koppelkondensatoren Audio |
| C36–C39 | Entprellung Taster |
| C50–C55 | Entprellung Drehgeber |
| C72, C73 | Verdoppler 48 V: kompakt bei D8/D9, aber nicht so kritisch wie C71 |

### Vibrationsalarm (neu)

| Bauteil | Wo |
|---|---|
| Q5, D10, C88, R95 | als Gruppe direkt an den Motor-Pads J8, 0,3-mm-Leitungen reichen (80 mA) |
| R93, R94 | am Gate von Q5. Die Leitung von U10 Pin 10 darf lang sein (nur Schaltsignal). |
| J8 | am Rand, weit weg vom Analogteil (J5, U11, U9, U13), z. B. oben bei Akku/Lader. Motor-GND mit eigenem Via. |

## 4. Aufteilung der Platine

1. **Zone Leistung:** J1, U1, U15, U3 mit L1, Akkuanschluss, U4/Q1, U6 mit L2, U7 mit L3/L4/D5/D6, U8 mit L5/Q4/D7–D9.
2. **Zone Analog:** J5, R17–R24, U11, U17/U18, U19, Codec-Analogseite, U13, J6. Möglichst weit von Zone 1 und vom C5.
3. **Zone Digital:** J2 (C5), J3/J4 (S3), U2, U10, U16, Taster, Drehgeber, LEDs.
4. Die GND-Fläche auf Lage 2 **nicht schlitzen**. Zonen trennt man über den Abstand, nicht über Schlitze in der Masse.
5. Unter die Spulen L1, L2, L3, L4, L5 und unter die Schaltknoten keine Signalleitungen, auch nicht auf den Innenlagen.
6. Thermische Pads (U3, U6, U7, U8, U9, U13, U17, U18) mit mehreren Vias in die GND-Fläche. Ausnahme: **U13 Pad an −5VA**, nicht an GND.
