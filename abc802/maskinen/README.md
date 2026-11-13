# ABC 802: maskinen

*(English summary below.)*

En referens för den som programmerar maskinen: kretsarna, klockorna,
minnet, portarna och kontakterna. Källan är Luxors *Dator ABC 802
Servicemanual* (230 8088-15); sidhänvisningarna (s.) gäller den. Det som
inte är utrett är märkt (?). Mycket gäller hela ABC 800-serien, och det
som skiljer står i [`../README.md`](../README.md).

## Kretsarna

Datorkortet sitter i bildskärmen, 200 × 233 mm, fyra lager (s. 6, 9).
Lägena är komponentlistans (s. 29–30).

| Läge | Krets | Uppgift |
|---|---|---|
| 18C | Z80A | processorn, 3 MHz |
| 9A | Z80A CTC | klockorna till de seriella kretsarna, realtidsklockan |
| 7A | Z80A SIO/2 | A = V24 (kontakten CH B), B = kassetten |
| 5A | Z80A DART | A = V24 (kontakten CH A, skrivaren), B = tangentbordet |
| 12G | MC6845P | bildkretsen (CRTC) |
| 9F, 11F, 12F, 14F | ABC02-1, ABC12-1, ABC22-1, ABC32-1 | ROM:en, 4 × 2764 = 32 KB |
| 11A–18A | 8 × HM 4864-3 | 64 KB dynamiskt RAM |
| 5G–8G | 4 × TC5514AP-2 | bildminnet, 2 KB statiskt |
| 3G | ABCT2-1 | teckengeneratorn |
| 2G | ABCP2-1 | PAL16R4 för bilden |
| 7D | 74LS375 | låskretsen som minns var den senaste hämtningen låg; också kassettreläet |
| X01 | kristall 12 MHz | systemklockan |

ROM:en är BASIC II från DIAB på $0000–$5FFF och DOS:en (version 20, byte
för byte densamma som i ABC 800 M) med drivrutinerna för PR:, V24:, CAS:
och terminalen på $6000–$7FFF. Hela ROM:en finns kommenterad i
[`../listning/`](../listning/).

Tangentbordet (ABC 55 eller ABC 77) har en egen processor, en 8035 med
programmet i en 2716, kristall 4,608 MHz, kapacitiva tangenter och en egen
högtalare för pipet (s. 26–27, 84). Det skickar tangentkoderna seriellt
till DART B.

## Klockorna

Kristallen på 12 MHz delas av en 74LS163 (s. 76):

| Frekvens | Till |
|---|---|
| 12 MHz | punktklockan: skiftregistret skiftar ut teckenraden |
| 6 MHz | delas till CPU:ns klocka |
| 3 MHz | Z80A, och CTC:ns systemklocka |
| 2 MHz | teckenklockan till 6845 |
| 1,5 MHz | CTC kanal 0–2 (CLK/TRG) |

128 teckentider à 0,5 µs ger 64 µs per linje (R0 = 127).

## Minnet

ROM:en ligger på $0000–$7FFF och de nedre 32 KB RAM parallellt med den.
De övre 32 KB ($8000–$FFFF) är alltid arbetsminne. Bildminnet, 2 KB,
ligger parallellt med den sista ROM-biten, $7800–$7FFF (s. 9, 18, 46, 48).

Vad CPU:n når beror på två saker: om DART B:s DTR är satt (signalen LRS*)
och var den senaste instruktionshämtningen (M1) låg. Låskretsen 7D är
öppen under varje M1 och minns om hämtningen låg i $7800–$7FFF (s. 49–50).

| Adress | Vanligt läge | Kod körs i $7800–$7FFF | LRS* (DTR B = 1) |
|---|---|---|---|
| $0000–$5FFF | ROM, BASIC II | RAM, nedre | RAM, nedre |
| $6000–$77FF | ROM, DOS och drivrutinerna | RAM, nedre | RAM, nedre |
| $7800–$7FFF | hämtning: ROM; data: bildminnet | ROM (också data) | RAM, nedre |
| $8000–$FFFF | RAM, övre | RAM, övre | RAM, övre |

Mittkolumnen är rutinen LDIR7F på $7FFD, som bara är `LDIR` och `RET`.
`LDIR` hämtar sig själv för varje byte, så under hela flyttningen läses
de nedre 32 KB RAM på $0000–$77FF. Efter `RET` hämtas nästa instruktion
utanför området, och det vanliga läget kommer tillbaka. Så läser och
skriver BASIC:s RAM-skiva MEM: de nedre 32 KB.

Med LRS* är ROM:en och bildminnet bortkopplade och maskinen har 64 KB RAM
från adress 0, vilket är vad CP/M behöver (s. 6, 49). ROM:en själv sätter
aldrig DTR. XM* från ABC-bussen kopplar på samma sätt bort ROM:en och de
nedre 32 KB, för minne på ett kort i bussen (s. 47).

Högst upp i RAM:et ligger BASIC:s och DOS:ens arbetsareor. Med skivminne
(s. 19): DOS:ens buffertar på $F500–$FCFF, systemvariablerna på
$FD00–$FEFF och de enkla variablerna på $FF00. Realtidsklockan står på
$FFEF–$FFF4: år, månad, dag, timme, minut, sekund (s. 55).

Bildminnet är två block à 1 KB, jämna och udda adresser (s. 77, 82). 6845
läser två byte per adress, ett tecken ur varje block.

Öppet (?): vart skrivningar på $0000–$77FF går i det vanliga läget. MEM:
skriver med LDIR7F, vilket tyder på att de inte når RAM:et.

## Portarna

A7 = 0 och A4–A6 väljer en intern krets; ABC-bussens strobar avkodas för
sig (s. 42–43, 54, 60, 64):

| Port | Krets |
|---|---|
| $00–$07 | ABC-bussen: OUT 0–5, INP 0, 1, 2; `IN A,($07)` ger RST på bussen |
| $08–$1F | ABC-bussen, bara XINPSTB/XOUTPSTB |
| $20–$23 | DART: $20/$21 A data/kontroll, $22/$23 B data/kontroll |
| $30–$3F | 6845: $38 register, $39 data, $31 läs. A3 väljer läsning eller skrivning, så bara $38–$3F skriver (s. 66) |
| $40–$43 | SIO/2: $40/$41 A, $42/$43 B |
| $60–$63 | CTC kanal 0–3 |
| $80–$FF | ABC-bussen, bara XINPSTB/XOUTPSTB |

Bussens OUT 0–5 och INP 0–1 är anpassade till Databoard 4680 (s. 42).

Signalerna som DART och SIO styr (s. 50):

| Krets | Signal | Till |
|---|---|---|
| DART A | RI | bygeln S3, 40 eller 80 tecken vid starten |
| DART B | DTR | LRS*: 64 KB RAM |
| DART B | RTS | 40 eller 80 tecken per rad |
| DART B | CTS | bygeln 50/60 Hz |
| DART B | TxD/RxD | tangentbordet |
| SIO B | DTR | reläet till kassettmotorn |
| SIO B | DCD | bygeln S1, skärmsläckaren |

## Avbrotten

Avbrotten går i kedja: CTC först, sedan SIO/2, DART och korten på bussen
(s. 45). NMI kommer bara från bussen; på $0066 står ett hopp som ger
omstart. CTC kanal 3 ger ett avbrott var 10,6:e ms till klockan (`TIME$`).
De andra kanalerna ställs in först när V24: eller PR: öppnas (s. 54–55).

## Kontakterna

Alla sitter på baksidan (s. 20–22).

| Kontakt | Typ | Stift |
|---|---|---|
| CH A | 9-polig D-sub | 1 DTR, 2 TxD, 3 RxD, 4 RTS, 5 CTS, 6 DSR (+12 V), 7 GND, 8 DCD, 9 −12 V |
| CH B | 9-polig D-sub | som CH A |
| Tangentbord | 7-polig DIN | 1 TxD, 2 GND, 3 RxD, 4 TRxC, 5 Key Down*, 6 +12 V, 7 Reset* |
| Kassett | 5-polig DIN | 1 TxD, 2 GND, 3 RxD, 4–5 motorn |
| Video | 6-polig DIN | 2 komposit, 3 GND |
| ABC-bussen | 64-polig Europakontakt | en ledig kortplats |

ABC-bussen (s. 22), rad a: 1 −12 V, 3 RESIN*, 5 INT*, 6–13 D7–D0, 15
RST*, 16–17 INP 1, 0, 18–23 OUT 5, 4, 3, 2, 0, 1, 24 NMI*, 25 INP2*, 26
XINPSTB*, 27 XOUTPSTB*, 28 XM*, 29 RFSH*, 30 RDY, 31 +5 V, 32 +12 V. Rad
b: 1 −12 V, 3 XMEMWR*, 4 XMEMFL*, 5 Φ, 6–13 0 V, 14–29 A15–A0, 30
MEMRQ*, 31 +5 V, 32 +12 V.

---

## English summary

A programmer's reference to the ABC 802, taken from Luxor's service
manual (page numbers refer to it):

- **Chips:** Z80A at 3 MHz, CTC, SIO/2, DART and an MC6845. The ROM is
  32 KB (BASIC II at $0000–$5FFF, the DOS and the drivers at
  $6000–$7FFF). The 64 KB of RAM is dynamic, and the video memory is
  2 KB of static RAM.
- **Memory:** the ROM overlays the lower 32 KB of RAM, and the video
  memory overlays $7800–$7FFF for data. A latch remembers whether the last
  opcode fetch was in $7800–$7FFF. While code runs there, the lower RAM is
  visible: `LDIR` at $7FFD is how BASIC's RAM disk MEM: reaches it. DTR on
  DART B (LRS*) switches out the ROM and gives 64 KB of RAM from 0, for
  CP/M.
- **Ports:** DART at $20–$23, the 6845 at $38/$39 (write) and $31 (read),
  SIO at $40–$43, CTC at $60–$63, and the ABC bus at $00–$07 and $80–$FF.
- **Interrupts:** CTC, SIO, DART, then the bus. CTC channel 3 ticks every
  10.6 ms for the clock.
