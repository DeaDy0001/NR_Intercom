# Bodypack-Platine V0.3: Hinweise fürs Layout

Stand 15.11.2026. Bezeichnungen wie im EasyEDA-Schaltplan.

Annahme: **4-Lagen-Platine, außen 1 oz (35 µm), innen 0,5 oz (17 µm)**, so wie bei den meisten Fertigern Standard.
Lage 2 ist eine durchgehende GND-Fläche, Lage 3 die Versorgungen.

---

## 1. Leiterbahnbreiten

Gerechnet nach IPC-2221 für 10 °C Erwärmung, mit Reserve aufgerundet. Innenlagen brauchen wegen des dünneren Kupfers
etwa die 4- bis 5-fache Breite, deshalb Ströme über 1 A **auf den Außenlagen oder als Fläche** führen.

| Leitung | Strom max. | Außenlage | Innenlage |
|---|---|---|---|
| **VBUS:** J1 → F1 → U1/U15 → U3 Pin 1 | 2,5 A | **1,5 mm** oder Fläche | Fläche |
| **Station 5 V:** J7 Pin 2 + 8 → C46 → U15 Pin 3 | 2,5 A | **1,5 mm** | Fläche |
| **U3 SW** (Pin 19/20) → L1 | 3 A Spitze | **Fläche, ≥ 1,5 mm**, so kurz wie möglich | – |
| **L1 → SYS** (U3 Pin 15/16) und PMID (Pin 23) | 3 A | **1,5 mm** oder Fläche | Fläche |
| **Akku:** BT+ → U3 Pin 13/14 | 2 A laden, 1,5 A entladen | **1,5 mm** oder Fläche | Fläche |
| **Akku-Minus:** BT− → Q1 Pin 1, Q1 Pin 3 → GND | 2 A | **1,5 mm** oder Fläche | Fläche |
| **VSYS Hauptleitung** (von U3 zu den Wandlern) | 1,5 A | **1,0 mm** | 4 mm oder Fläche |
| VSYS → U6 Pin 10/11 (3,3 V) | 1 A | 0,8 mm | 3 mm |
| VSYS → L5 (48 V), VSYS → L3 / U7 (±6,5 V) | je 0,5 A | 0,5 mm | 1,5 mm |
| VSYS → J3/J4 Pin 14 (S3-Board) | 0,5 A | 0,5 mm | 1,5 mm |
| VSYS → LEDs | 0,1 A | 0,3 mm | 0,5 mm |
| +3V3 von U6 | 0,6 A | 0,5 mm | 1,5 mm |
| +3V3 zum C5-Modul (J2 Pin 3) | 0,4 A | 0,4 mm | 1,2 mm |
| ±6,5 V, ±5VA, 3V3A | 0,1 A | 0,3 mm | 0,5 mm |
| 48-V-Teil (Schaltknoten L5/Q4/D7/C72, P24, P48) | 0,5 A | 0,6 mm | – |
| alle Signale (I2C, I2S, UART, SPI, Tasten) | – | 0,2 mm | 0,2 mm |

**Durchkontaktierungen (Vias):** Ein Standard-Via (0,3 mm Bohrung) trägt ca. 1 A. Bei 2–3 A **3–4 Vias** nebeneinander setzen.

**Abstände:**

- 48-V-Teil (P24, DBL, P48_RAW, P48, Mikrofonleitungen hinter R17/R18): **mind. 0,5 mm** zu allem anderen.
- Sonst reicht das Minimum des Fertigers (0,15–0,2 mm).

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

## 4. Aufteilung der Platine

1. **Zone Leistung:** J1, U1, U15, U3 mit L1, Akkuanschluss, U4/Q1, U6 mit L2, U7 mit L3/L4/D5/D6, U8 mit L5/Q4/D7–D9.
2. **Zone Analog:** J5, R17–R24, U11, U17/U18, U19, Codec-Analogseite, U13, J6. Möglichst weit von Zone 1 und vom C5.
3. **Zone Digital:** J2 (C5), J3/J4 (S3), U2, U10, U16, Taster, Drehgeber, LEDs.
4. Die GND-Fläche auf Lage 2 **nicht schlitzen**. Zonen trennt man über den Abstand, nicht über Schlitze in der Masse.
5. Unter die Spulen L1, L2, L3, L4, L5 und unter die Schaltknoten keine Signalleitungen, auch nicht auf den Innenlagen.
6. Thermische Pads (U3, U6, U7, U8, U9, U13, U17, U18) mit mehreren Vias in die GND-Fläche. Ausnahme: **U13 Pad an −5VA**, nicht an GND.
