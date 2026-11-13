# Kassetten i ABC-datorerna: formaten

Arbetsunderlag för verktygen och PDF:en. (?) markerar det som inte är
belagt än. Källorna står i slutet av varje avsnitt.

Maskiner: ABC80, ABC 800 C/M, ABC 802 och ABC 806. ABC 1600 är inte med.

En fil på bandet beskrivs i fyra lager:

| Lager | Vad | Skiljer sig mellan |
|-------|-----|--------------------|
| 1 Signal | ljudet på bandet, bitcellerna | ABC80 (programvara) och ABC 800-serien (SIO + ABC 820) |
| 2 Block | synk, STX, 256 byte, ETX, summa | små skillnader i inledning och avslutning |
| 3 Fil | filhuvud + datablock med nummer | ABC 800 har tidkonstanten i filhuvudet |
| 4 Innehåll | BAS-text, BAC, data; teckenkod | ABC80-BASIC och BASIC II; 7 bitar, DOS, UTF-8 |

---

## 1 ABC80

**Hårdvaran.** PIO B, port $3A:

- bit 5 ut: motorreläet ($20 = på, $00 = av)
- bit 6 ut: skrivsignalen. Vid läsning nollställer den låskretsen.
- bit 7 in: läst signal (låskretsen)

**Signalen.** FM (bifas), skriven av programvaran (`CASWBI`/`CASHLF`).
Signalen vänds i slutet av varje bitcell, och en etta vänds en gång till
mitt i cellen. En nolla tar 3993 T och en etta 4065 T. Vid 2,9952 MHz blir
det 1,333 ms och 1,357 ms, eller 750 och 737 bit/s. Luxor anger 700 baud.
I version 9913 är en etta 8 T kortare. Minst signifikanta biten skrivs först.

**Läsningen.** Den sker med avbrott, ett per bitcell (`CASINT`). Flanken
i cellens början ger avbrottet, och bit 7 läses ungefär 3/4 cell senare.
Har en flank till kommit däremellan är biten en etta.

**Blocket** (`CASWBL`):

| Del | Längd |
|-----|-------|
| nollbitar | 256 bitar |
| SYN $16 | 3 byte |
| STX $02 | 1 byte |
| data | 256 byte |
| ETX $03 | 1 byte |
| summa | 2 byte, låg byte först: 3 + summan av de 256 databytena, 16 bitar |

Ett block är 2368 bitar och tar ungefär 3,2 s.

**Filen:**

- Namnblock: `FF FF FF`, sedan namnet i 11 tecken (8 + 3, med blanksteg
  efter), och resten nollor.
- Datablock: `00`, blocknumret (låg, hög; det första är 0), sedan
  253 byte data.
- Före namnblocket väntar `CASPRE` cirka 6,8 s på att bandet ska komma upp
  i fart.
- Motorn går hela tiden som filen är öppen. Pauser blir tystnad på bandet.

**Felen vid läsning:**

| Fel | Orsak |
|-----|-------|
| 42 | ingen signal inom cirka 8,5 s (`$AA`) |
| 37 | fel blocknummer eller fel början på blocket |
| formatfel | ETX saknas (`$A5`) |
| summafel | summan stämmer inte (`$A3`) |

**Två buffertar.** Läsningen växlar mellan $FB00 och $FC00. Läser
programmet långsamt går block förlorade, och nästa block ger fel 37.

Källor: `~/Documents/abc80/dis/abc80old.asm` (rutinerna CASPRE, CASWBL,
CASOPN, CASGET, CASRBL och CASINT); ABC80-boken, band 1, kapitlet
Hårdvaran, avsnittet Kassetten; Markesjö 1978, s. 112–124, 164–167 och
193–194.

---

## 2 ABC 800-serien (800 C/M, 802, 806)

**Hårdvaran.** SIO/2 kanal B ($42/$43) i teckensynkront läge (bisynk med
16 bitars synk, WR6 = $16 och WR7 = $02). DTR på SIO B drar reläet RE1
till motorn. CTC kanal 0 ger bitklockan. På ABC 802 går den femstiftiga
DIN-kontakten så här: 1 TxD, 2 GND, 3 RxD, 4–5 motorn. ABC 800 ansluter
kassettminnet ABC 820 till kontakten märkt TAPE.

**Hastigheten.** Enligt bruksanvisningen för ABC 800:

> Filhuvudet ("programrubriken") skrivs och läses alltid i 700 baud,
> och det anger själv hastigheten för resten av filen, 700 eller 2400
> baud. Därför läser ABC 800 band som är inspelade på ABC80.

ROM:en i ABC 802 gör just så:

- Filhuvudet skrivs och läses med CTC-tidkonstanten 129.
- Datablocken använder tidkonstanten i filhuvudet. Det är VFF99,
  som är 40 efter RAMINI.

Med 3 MHz / 16 / tk ger 129 en klocka på 1 453 Hz och 40 en klocka på
4 688 Hz. Det är nära två gånger 700 respektive 2 400 baud. Därför tror
jag att CTC:n ger dubbla bittakten, och att FM-kodningen till ABC80:s
format görs utanför SIO:n, i ABC 820 eller i gränssnittet (?). Detta
måste beläggas i schemat (ABC800-schema.pdf, servicemanualernas blad 1)
innan lager 1 för ABC 800 skrivs. Emulatorn räknar i dag med att bandet
är tecken, inte ljud (`kassett802.h`).

**Blocket** (`CASWR` i ABC 802):

| Del | Längd |
|-----|-------|
| $00 | 32 byte (= 256 nollbitar) |
| SYN $16 | 1 byte (ABC80 har 3) |
| STX $02 | 1 byte |
| data | 256 byte |
| ETX $03 | 1 byte |
| summa | 2 byte, som hos ABC80 |
| $FF | 2 byte (finns inte hos ABC80) |

Vid läsning letar SIO:n själv efter `$16 $02`, så ett block från ABC80
duger.

**Filen:**

- Filhuvud: `FF FF FF`, namnet i 11 tecken, tidkonstanten i
  1 byte (position 14) och resten nollor.
- Ett band från ABC80 har en nolla på position 14 (?). Det bör betyda
  700 baud. Kontrollera i `CASCTC`, där A = 0 lämnar CTC:n orörd.
- Datablock: `00`, blocknumret och 253 byte, som hos ABC80.
- Pauserna: 12 × 65 536 varv före filhuvudet. Mellan blocken 1 eller 3
  beroende på bit 5 i FCB+3.
- Felen: 37 (fel blocknummer), 35 (mottagningsfel eller fel summa),
  41 och 52.

**Var koden ligger:**

| Maskin | Plats | Källa |
|--------|-------|-------|
| ABC 802 | CAS: på $736D–$79C0 | `dis/build/abc802rom.asm` |
| ABC 800 C/M | i ROM 0 ("CAS" på $064A i `ABC800-basic.C-1.bin`, $0679 i `M-12`) | inte disassemblerad än (?) |
| ABC 806 | ROM:en finns inte här | bara manualerna (`ABC806-*.pdf`) (?) |

Källor: `dis/build/abc802rom.asm` (CASRD, CASRX, CASWR, CASINI, CASCTC och
CASDRV); `GitHub/ABC80/abc802/emulator/karna/kassett802.h`;
`maskin/MASKINEN.md`; `~/Documents/abc80/dis/doc/ref/arkiv/abc800/ABC800-dator-bruksanvisning.pdf`
(avsnitten om ABC 820, s. 36 och kapitel 4).

---

## 3 Innehållet

| Typ | Skrivs med | Form |
|-----|------------|------|
| BAS, text | `LIST`, `PRINT #` | rader avslutade med CR. I blocket ger `$09 n` n blanksteg, och ETX avslutar texten. Se 3.1 |
| BAC, ABC80 | `SAVE` | 256-bytes sektorer, första byten $82. Rad: `[längd][nr lo][hi][kod …][$0D]`. Samma form på band och skiva |
| BAC, BASIC II | `SAVE` | första byten $8F (sett i `L.BAC`). Formatet skiljer sig från ABC80:s (?) |
| BIN, data | `PUT` | rå data i datablocken (?) |

BASIC II finns i ABC 800-serien, i BASIC II för ABC80 (`basicii80`) och i
BASIC II för PC (`basic-ii-pc`).

### 3.1 Text i blocken

Ur ROM:erna (ABC80: `TXTPRT` på $0912 och `TXTEND`, `abc80old.lst`;
ABC 802: `BPRINT` på $4A07, `BSPACE` på $49E9 och funktion 0 på $4991,
`dis/build/abc802rom.lst`) och provat i emulatorerna 2026-11-13:

- Texten står i datablockets 253 byte: högst 252 tecken och sedan ETX.
- LF skrivs inte. Programmet skickar CR LF, och ROM:en tar bort LF.
- Ett blanksteg som följs av ett blanksteg i samma utskrift börjar en
  följd, som blir `$09 n` (n räknas mod 256). I BASIC II blir varje del
  av en `PRINT #` en egen utskrift, och blanksteg i två delar packas inte
  ihop: `PRINT #1,"RAD";I;"  "` ger `RAD 1 $09 02`, där blanksteget efter
  talet står för sig. ABC80 ger `RAD 1$09 03`, som om raden kom i ett
  stycke.
- Blanksteg före CR tas bort bara om CR kommer i samma utskrift. `LIST`
  och `PRINT #` skickar CR för sig, så blanksteg sist på raden blir kvar.
- Vid blockets slut skiljer ROM:erna sig:
  - ABC80 skriver blocket först när två eller färre platser är lediga
    före `$09 n`.
  - BASIC II packar så länge två platser är lediga. På den sista platsen
    skriver den ett vanligt blanksteg, och resten av följden kommer i
    nästa block.
- Vid stängning skrivs blocket (ETX efter texten), och sist ett block med
  sex nollor och ETX. Står blocket tomt, får det själv nollorna.
- Bufferten skrivs hel och nollas inte mellan blocken, så efter ETX står
  det som var kvar från blocket före. Det kommer med i summan.
- Vid läsning är ETX slut på blocket, $00 slut på filen och en byte över
  $7F fel 58. Det som läses ut är rader med CR (och LF i ABC80).

`kas packa` följer ABC80:s regel, och BASIC II:s när `--tk` ges. Ett
program (`LIST`) blir då samma block som ROM:en skriver, byte för byte.
En datafil från `PRINT #` blir också samma block i ABC80. I BASIC II
blir den samma text, men blocken kan skilja där programmet delade raden
på flera utskrifter, eftersom det inte syns i texten.

---

## 4 Teckenkoderna

| Kod | Var | ÄÖÅ |
|-----|-----|-----|
| Svensk 7-bitarskod (den svenska varianten av ISO 646) | ABC80, ABC 800-serien, filerna på band och skiva | `@`=É `[`=Ä `\`=Ö `]`=Å `^`=Ü `` ` ``=é `{`=ä `|`=ö `}`=å `~`=ü `$`=¤ |
| DOS | (a) ABC:s egna DOS (UFD-DOS, DOS v.20): samma filer på skiva. (b) MS-DOS på PC: kodsidorna 437/850, Ä = $8E (?) | se frågan nedan |
| UTF-8 | i dag | |

Radsluten är CR på ABC, CRLF i DOS och LF i dag.

I BASIC II för PC förekommer `[\]{|}` ofta i BAC-filerna. Den
svenska 7-bitarskoden levde alltså kvar där (?).

---

## Öppna frågor

1. Hur ser signalen ut på bandet från ABC 800/ABC 820? Gör ABC 820 FM-kodningen?
   Läs schemat.
2. Disassemblera CAS: i ABC 800:s ROM 0 och jämför med ABC 802.
3. Har ABC 806 samma CAS: som 800? Det behövs en ROM.
4. Vilken byte på position 14 lämnar ABC80, och hur tolkar ABC 800 den?
5. Hur är BASIC II:s BAC-format uppbyggt? Ta det ur ROM:en (LIST).
6. Vilken DOS menas: ABC:s egna DOS eller MS-DOS på PC, eller båda?
