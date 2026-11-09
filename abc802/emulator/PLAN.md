# Emulatorn för ABC 802 (steg 5 i FARDPLAN.md)

En ABC 802 på samma Z80-kärna som ABC80-emulatorn (`karna/z80.c`), med
egen kod för maskinen. Uppgifterna om maskinen står i
`../maskin/MASKINEN.md` och `../dis/tools/abc802_notes.py`.

## Delsteg

1. **Kärnan och batchläget.** Minnet med låskretsen 7D och LRS*, DART,
   SIO/2, CTC, 6845, tangentbordet på DART B, avbrottskedjan, bilden ur
   teckengeneratorn och skärmen som text. `vard/abc802.c` kör och
   skriver skärmen (och en PNG).
2. **Fönstret.** SDL3 som `abc80/emulator/sdl/`, med tangentbordet och
   pipet.
3. **Webbsidan.** WebAssembly som `abc80/emulator/webb/abc80.html`, en
   fristående fil.
4. **Skivminnet.** DOS v.20 och ett skivkort på ABC-bussen
   (`karna/skivkort.c` ur ABC80-emulatorn som grund).
5. **CP/M.** 64 KB RAM med DTR på DART B; vilken CP/M som fanns till
   ABC 802 (`../prov/CPM.md`).
6. **Kassetten, V24 och skrivaren.** SIO B med CTC kanal 0, CH A och CH B.

## Läget

- 2026-10-13: delsteg 1 klart. `make` bygger `abc802`. Nya filer:
  `karna/sio.c` (SIO/2 och DART, asynkront: WR0-WR7, RR0-RR2, kön på tre
  tecken, avbrotten i sex nivåer och vektorn som status ändrar),
  `karna/ctc.c` (timer- och räknarläge, förskalning, avbrott),
  `karna/abc802.c` (minnet, portarna, tangentkön, kedjan CTC, SIO, DART
  och RETI), `karna/skarm802.c` (bilden 480 x 250 och texten),
  `vard/abc802.c` (flaggorna -r -c -40 -60 -k -f -K -t -p -i) och
  `vard/png.c` (kopia).
- ROM:en startar och skriver `ABC802` efter 0,5 s. BASIC-program går att
  skriva och köra, med åäö. 40 tecken (-40) fungerar.
- 2026-10-13: delsteg 2 klart. `sdl/fonster.c` och `make` i `sdl/` bygger
  `abc802f`: bilden 480 x 250 i 4:3 (punkterna 2 x 3 förstorade,
  bärnsten med sken och efterlysning på 20 ms; -skarp utan),
  tangenterna som i ABC80-fönstret, F12 RESET, inklistring, -k och -f.
  Pipet (kod 7 till tangentbordet) är en ton på 1 kHz i 100 ms (?);
  tangentbordets 2716 (65-02486) finns inte, så tonen är inte känd.
  Öppet: tangentbordets funktionstangenter (PF1-PF8?) och vilka koder
  ABC 55 ger för dem är inte utrett.
- 2026-10-13: delsteg 3 klart. `make webb` bygger `webb/abc802.html`, en
  fristående fil (WebAssembly, ROM:en och teckengeneratorn som base64)
  som ABC80:s sida: `webb/webb.c` (värden), `webb/abc802.js`,
  `webb/mall.html` och `webb/bygg.sh`; `webb/libc.c` är ABC80:s. Samma
  bärnstensfosfor som fönstret (efterlysningen på canvasen, skenet som
  en suddig canvas ovanpå med CSS blur), pipet som en ton på 1 kHz i
  100 ms (?), byglarna 40/80 och 50/60 Hz (startar om), skarp bild,
  Kopiera texten, Skriv in en fil (också släppt på sidan eller
  inklistrat), svenska och engelska, Kom igång, `?k=`, `?tecken=40`,
  `?hz=60`, `?skarp=pa`, `?ljud=av`, `?sprak=`. Provat i Chromium,
  WebKit och Firefox (Playwright): program, åäö, BREAK, pip, 40 tecken.
- 2026-10-13: delsteg 4 klart. ABC-bussen i kärnan (`karna/bussen.c` och
  `karna/skivkort.c`, kopior ur ABC80-emulatorn): OUT 0-5 och IN 0-2 går
  till korten (MASKINEN.md), resten läser $FF. DOS v.20 i ROM:en hittar
  kort 44 och läser, skriver, tar bort (KILL) och ger fel 43 vid
  skrivskydd och fel 21 när HD0 saknas, som BASIC II:s DOS v.19 i
  ABC80-emulatorn. Värden och fönstret: `-D fil[@kort[.enhet]]` (standard
  kort 44 enhet 0, MF0/DR0), `-DP` skrivskyddad, `-DS` skriver tillbaka
  avbilderna. `-k` har fått `\r`, och `-f` en paus på 200 ms efter varje
  rad (som webbsidan; utan den tappas tangenter). Webbsidan har panelen
  Skivminnet: Tidsproven (från början), Tom skiva, Sätt i (.img, .dsk,
  också släppt på sidan), Spara, och katalogen ur avbilden (BASIC II har
  ingen; LIB fanns på Luxors systemskiva), där ett klick ger `LOAD DR0:`.
  Skivorna (`../prov/skivor/gor.py`): `tom.img` är en ABC 800-skiva där
  DOS:en har tagit bort allt utom SYSDIR.SYS, och `tidsprov.img` har 33
  av tidsproven, inskrivna med `-f` och sparade med SAVE (utan AHLS,
  AHLS1 och SCRUSS, som README.md säger inte är anpassade). Provat i
  Chromium, WebKit och Firefox (Playwright).
- 2026-10-13: Luxors systemprogram på `tidsprov.img`
  (`../prov/skivor/luxor/`, ur ABC-klubbens arkiv): BASICINI.SYS ger
  felen som text på svenska (autostarten bortkopplad), BYE kör CMDINT.SYS
  (DOS:ens kommandotolk, `LIB` och `¤BAS` tillbaka), LIB.ABS och LIB.BAC
  visar katalogen. De binära filerna skrivs av ett BASIC-program med PUT
  (253 bytes per post), LIB.BAS som tangenter. Värden har fått `-L ms`,
  pausen efter varje rad med `-f`: LIB (17 KB) tappar tangenter med 200 ms,
  eftersom BASIC II tar längre tid på sig att lägga in en rad ju större
  programmet är (4560 blev 560); 1000 ms räcker. Värden skriver bara den
  sista skärmen, så ett fel under inskrivningen syns inte där.
- 2026-10-13, delsteg 5 (början): den CP/M som fanns till ABC 802 är
  MyAB:s CP/M Plus 3.0 (BIOS 1.2C, "ABC802 C version"; disk001.img i
  abc80.net:s arkiv, CPM3802A/B/C.SYS). Den startar genom DOS:ens krok:
  OPNINI öppnar BASICINI.SYS, och börjar sektorn med JP körs den i
  sektorbufferten ($F500). Den går i emulatorn (DIR, SHOW, DATE, TYPE)
  efter två rättelser mot maskinen, båda prövade med BASIC och DOS
  (tidsproven, LIB, SAVE, LOAD, KILL, 40 tecken, webbsidan i Chromium och
  WebKit):
  - 6845: R/W kommer från A3 (s. 66), så OUT till $30-$37 skriver
    inget. MyAB:s BIOS gör OUT ($35); förut flyttade det bilden 48
    kolumner (R12 = $02).
  - Kort 44: IN 1 har bitarna som på FD2 (bit 7 status, bit 3 = 0 efter
    fel, bit 2 = 0 under data, bit 1 = 0 medan kommandot tas emot), och
    samma kort valt igen behåller sitt läge. CP/M:s laddare väntade
    förut för evigt.
  Avbilderna ligger inte i förrådet (privatpersoners program). Nästa: en
  egen CP/M 2.2 som i ABC80-emulatorn (DR:s fria CP/M, egen BIOS med
  drivrutinen för kort 44, start genom en egen BASICINI.SYS; BIOS:en
  slår av DTR för att skriva i bildminnet och läser DART B utan
  avbrott).
- 2026-10-13, delsteg 5 klart: en egen CP/M 2.2 i `cpm/`, utan något ur
  MyAB. CCP och BDOS är DR:s (`cpm/dr/`, `cpm22.py` som i ABC80, för
  $E000); `cpm/start.asm` är laddaren, post 0 i BASICINI.SYS (från
  $F503): den läser sektor 40-71 på MF0 till $8000, slår på DTR och
  flyttar systemet till $E000. `cpm/bios.asm` ($F600) har ABC80-BIOS:ens
  skärmkoder (ADM-3A och ESC T/Y/*/E/R) på 24 x 80 med 6845:s markör,
  tangentbordet på DART B utan avbrott (BOOT stänger av dem i DART, SIO
  och CTC), och A: B: på kort 44 enhet 0 och 1: 80 spår om 32 sektorer,
  5 systemspår, 150 block om 2 KB, 128 katalogposter. KONUT byter till
  en stack över $8000 och slår av DTR medan den skriver. `cpm/skiva.py`
  gör skivorna: DOS:en själv skriver BASICINI.SYS (som `gor.py`),
  bitkartan sätts full så att DOS:en aldrig skriver i CP/M:s del, och
  filerna läggs in med EXM = 1. `make` bygger `cpm/a.img` (systemet och
  DR:s program) och `cpm/b.img` (tom); `make cpmprov` (`cpm/prov.sh`)
  provar starten, DIR, STAT, skärmkoderna (`prov/skarm.asm`), ASM, LOAD,
  ^C, och en fil på 70 KB (tre extent) med PIP [V] tillbaka till A:,
  jämförd byte för byte. ED och DDT är prövade för hand. Webbsidan har
  knappen *CP/M* och `?cpm=pa`, och visar då CP/M-katalogen (prövad i
  Chromium, WebKit och Firefox). Assemblern behöver `-g ORG`, annars
  fylls utfilen från 0.
- 2026-10-13: editorn SKRIV ur ABC80-emulatorn (`cpm/skriv/`, källan och
  bruksanvisningen som de är) finns på `cpm/a.img` och webbsidan. Den
  läser kolumnerna ur minnet, så BIOS:en har fått byten BREDDEN (80)
  efter hopptabellen, och Makefile sätter KOLUMNER till dess adress ur
  `cpm/bios.lst` (listningen kommer på standard ut med dis:s assembler).
  `make cpmprov` provar att flytta en rad och spara; webbsidan är
  prövad med ESC K X i Chromium, WebKit och Firefox. Bruksanvisningen
  talar om ABC80 (40 kolumner, ESC som Ctrl-Ä).
- 2026-10-13: BBC BASIC (`cpm/bbc/`, Russells BBCBASIC.COM med vår sida
  `abc802.asm` på $0100-$01FF) och Zork I med tolken ZTOLK (`cpm/zork/`,
  ztolk.asm oförändrad) ur ABC80-emulatorn finns på `cpm/a.img` och
  webbsidan. Båda assembleras med bokens assembler (verktyg/z80asm i
  ABC80/inifran), som de är skrivna för: dis:s assembler tar inte
  `CP (uttr)&255`. Sidans RAD och KOL är EQU i BIOS:en och står inte i
  listningen; Makefile tar dem ur dis:s symboltabell (`-v`). TICKS väntar
  1 154 varv om 26 T, 10 ms vid 3 MHz. `*BYE` (eller QUIT) går tillbaka
  till CP/M. `make cpmprov` provar räkning, TIME under INKEY(200), POS och
  VPOS, Zorks statusrad och save/restore (20 prov); webbsidan är prövad
  med BBC BASIC och ett drag i Zork i Chromium, WebKit och Firefox.
- 2026-10-13, delsteg 6 klart: kassetten, CH A (PR:) och CH B (V24:).
  `karna/sio.c` har fått det teckensynkrona läget (mono- och bisynk:
  jakten efter WR7 eller WR6+WR7, RR0 bit 4, fyllnad med WR6 när
  bufferten är tom), och teckentiden räknas ur WR3-WR5 och CTC:n
  (`klockor()` i `karna/abc802.c`): CH A klockas av kanal 2, CH B av
  kanal 0 (sändaren) och 1 (mottagaren), kassetten av kanal 0 (?).
  `karna/kassett802.c` är bandet, ett tecken per byte; motorn följer DTR
  på SIO B. Huvudet skrivs med tidkonstanten 129 (182 tecken/s) och
  blocken med $FF99 = 40. Det som går ut på CH A och CH B lämnas till
  värden, och in kommer ett tecken per teckentid när mottagaren är på;
  XOFF ($13) från maskinen håller inmatningen tills XON ($11). Flaggorna
  `-T`, `-U`, `-A`, `-B`, `-a` och `-b` i `abc802` och `abc802f`, och
  panelerna Kassetten och CH A och CH B på webbsidan. Prövat: `SAVE
  CAS:PROV`, spola, `LOAD CAS:PROV` och `RUN` (`LOAD CAS:` visar
  namnet), `LIST PR:`, `PRINT #1` och `INPUT #1` på V24: och `LOAD V24:`
  av text (BREAK slutar); webbsidan i Chromium, WebKit och Firefox.
  `make cpmprov` ger fortfarande 20 rätt.
- Grafiken: SET DOT ger mosaiken; raden måste ha grafikkoden. PRINT tar
  bort koder under $20 (SCRCHR) och bit 7 (CHRMSK), men $97 (bit 7 + $17)
  med POKE i bildminnet ($7800 + 80 x rad) slår på grafiken, eftersom
  bit 7 bara inverterar och koden är $17. Manualen gör det med PRINT GWHT
  (= CHR$(151)): SCRCHR prövar koden innan bit 7 tas bort, så $97 går
  igenom och $17 hamnar i bildminnet. Prövat; se bokens kapitel 3.
- MEM: fungerar: `SAVE MEM:16`, `LOAD MEM:16` (namnen är tal 0-255, annat
  ger fel 21). Det prövar låskretsen (LDIR7F på $7FFD).
- Teckengeneratorns fil är 8 KB, men bara de första 4 KB är tecken;
  resten är programkod (en 2732 läst som 2764?).
- Klockan går bara när dagen ($FFF1) är ställd (CLKTIK). Proven ställer
  den med POKE 65521%,1%.
- 2026-10-21: de oanvända bildfilerna från ABC80 (`bild.c`, `bild.h`,
  `tecken.c`, `tecken.h`, `teckensnitt.h`) är borttagna (f752f78); 802:an
  ritar ur teckengeneratorn. Kvar i `karna/` men inte i Makefile:
  `ieckort.c`, `kassett.c`, `ljud.c`, `p40.c`, `skrivarkort.c` och
  `uni80.c`. De behövs möjligen inte, men det beror på vad som gick att
  koppla till ABC 802 (IEC-buss, skrivarkort, UNI-80 m.m.), och det vet vi
  ännu för lite om (?). Avgör när det är utrett.
- 2026-11-06: skrivarkortet. PR: i ROM:en ($7000) har typerna C
  (Centronics), P och U, alla kort 60 på ABC-bussen (OUT 1 $BC, IN 1
  statusen, OUT 0 tecknet), som 9704 i ABC80. GETDEF väljer kortet av sig
  själv när det svarar, vid första OPEN efter RESET. `karna/skrivarkort.c`
  är omskriven för 802:an: Luxors Centronicskort 55 20502-01 med statusen
  $07 (bit 0-2 ej kopplade, läses som 1 (?); BUSY, PE, FAULT och låset
  0), så att typen blir P; skrivaren är alltid klar. Flaggan `-P fil` i
  `abc802` och `abc802f`, och på webbsidan kryssrutan *kortet i* och
  "Skrivarkortet (kort 60)" i panelen CH A och CH B. Prövat: `LIST PR:`
  med och utan kortet, `LIST "PR:C"` och `"PR:V"`, och på webbsidan
  kortet i, RESET och `LIST PR:`. V24-skrivaren på PIO B (ABC80) är
  borttagen ur filen; 802:an har ingen PIO.
- 2026-11-06: Datavision på CH B, en rekonstruktion (sidorna är nya).
  `webb/datavision.js` svarar på det maskinen sänder på V24: och skickar
  sidor som options-PROM:ets terminal typ W visar (TERM och DVTAB i
  ROM:en): FF, ^^, ^J, ^K, ^M, ^T och ESC x för attributen, mosaiken som
  bytes, och STX följt av en rad som INPUT #1 läser. Navigeringen som i
  Prestel (*nn#, *#, **, en siffra, #). På webbsidan väljs den i panelen
  CH A och CH B (*Datavision*, *Ring upp*); F1-F8 ger PF-tangenterna
  $C0-$C7 (?), så att terminalen går att lämna (fel 53). Prövat i
  webbläsaren med 40 tecken och `LOAD V24:WEA10K24.40A`: sidorna 0, 1, 4,
  7, *# tillbaka, en sida som inte finns, F1 ut, och programmet på sidan
  9 som fick raden efter STX. Färgerna syns inte, eftersom skärmen är
  svartvit; attributen blir tomma celler. Inte utrett: `LOAD "V24:W"` med
  ett program efter STX (varje rad går först genom terminalen, och BREAK
  går ut till motparten), och split speed 1200/75 i emulatorn.
- 2026-11-06: Bytesboden, en BBS på CH B, också en rekonstruktion (den
  har inte funnits; inlägg, alias och program är påhittade).
  `webb/bbs.js` är en ADM-3A-motpart för terminal typ T: ^Z tömmer
  skärmen, raderna slutar med CR LF, och BBS:en skickar tillbaka det man
  skriver (* för lösenordet, BS mellanslag BS för att sudda). Inloggning
  med vilket namn som helst, en meny (mötet med inlägg att läsa och
  skriva, filer, vem som är inne, tiden, hjälp, lägg på). Ett program
  hämtas så: BBS:en väntar tills texten har gått ut (in_ko och kärnans
  kö, `seriellt_vantar()`, ny i webb.c) och 30 sekunder till, medan man
  lämnar terminalen med PF1 och skriver `LOAD V24:VSA30B24.22A`; sedan
  sänds programmet rad för rad med CR, och BREAK avslutar LOAD. De tre
  programmen (HEJ, TABELL, GISSA) är prövade i terminalversionen med -f.
  På webbsidan är motparterna nu en lista (`motparter` i abc802.js), och
  den valda får det maskinen sänder på CH B. Prövat i webbläsaren med 80
  tecken och `LOAD V24:TSA30B24.22A`: välkomsten, inloggningen, mötet,
  filerna, GISSA.BAS hämtad och listad, tillbaka till menyn med en
  tangent, A som lägger på, och byte till Datavision.
- 2026-11-06: Datacentralen, en IBM-stordator med MVS och TSO i radläge
  på CH B, också en rekonstruktion (anläggningen har inte funnits; TSO-
  och JES2-meddelandena är skrivna ur minnet, (?)). `webb/stordator.js`:
  inloggning med vilket namn som helst, lösenordet ekas inte, sedan READY
  och HELP, TIME, LISTCAT, LIST, SUBMIT, STATUS, OUTPUT och LOGOFF. Jobbet
  KVADRAT väntar 8 s, körs 8 s och meldar med $HASP165. Långa listor går
  ut rad för rad när kön har plats, och BREAK (0x03) avbryter med READY.
  Modemljud vid uppringning (lågt): klick, kopplingston 425 Hz,
  fingerskivans klick (d+1 per siffra, (?)), en signal, svarstonen
  2100 Hz och bärvågen (V.21, för Datavision V.23); när förbindelsen är
  uppe är det tyst, och ett klick hörs när den bryts. Prövat i
  webbläsaren: inloggning, SUBMIT, STATUS och meddelandet; BREAK och
  LOGOFF i Node (fliken var dold i webbläsaren, och då går emulatorn
  långsamt).
- 2026-11-06: Tidsproven granskade och rättade (`../prov/*.BAS`). Varje
  program på `tidsprov.img` listas nu av maskinen till
  `../prov/skivor/listor/` (gor.py), och raderna jämförs med källan:
  BASICINI.SYS ger felen på svenska, så `"Error"` i utskriften räckte
  inte för att se att en rad inte togs emot. Rättat: SNAKE1-3 (ramen
  ritades innan Mx fanns, eller med rad och kolumn omkastade så att
  SET DOT gick utanför skärmen; ormen ritades på raden R(1); maten hade
  rad och kolumn ur fel gräns; WIDTH 80 i SNAKE1-2 som i SNAKE3; C(4) i
  stället för C(5)), SNAKE2-3 (FNGshift), SNAKE3 (ON Cu GOTO och GOTO
  pekade på radnumren i SNAKE2), MANDEL3 (MX i stället för MX%: allt
  blev punkter; nu 88 s som på maskinen), SCRUSS2 (TIME i stället för
  TIME$), B8 (K<1000, som tiden på maskinen gäller), och ’ ” ü till
  ' " ^ i filerna. NOEL1 skriver -23788: heltalens + slår runt utan fel
  (IADD), 500500 - 7*65536; så på maskinen också (8,00 s mot 8,01).
  Emulatorn: DART:ens dataregister ger det förra tecknet igen när inget
  nytt har kommit (var $FF). SNAKE läser tangenten med
  ABS(SYS(8)*INP(34)) efter att avbrottet redan tagit tecknet; nu svänger
  ormen. SCRUSS2 och MANDEL3 behöver klockan ställd (POKE 65521%,1%).
- 2026-11-06: ORD 800 (v. 2.4, ABC 830-skiva som Teledisk) startar och
  går att skriva i: `../prov/ord800/` packar upp skivan, flyttar filerna
  till en MF-skiva och lappar kopieringsskyddet. Skyddet ger kontrollern
  kommando $10 och vill ha $0A i byte 1 av bufferten; annars XOR:as
  hopptabellen på $F900. Svaret finns inte i avbilden, så JR Z blir JR
  (README där). `-k` tar nu `\xHH` (PF1-PF8 är `\xC0`-`\xC7`). Spara
  och hämta på DR1 (programskivan med `-DP`) och utskrift på CH A
  fungerar.

## Tiderna mot maskinen (metod TA i ../prov/README.md)

En första jämförelse; steg 6 i FARDPLAN.md gör den ordentligt.

| Program | Maskinen | Emulatorn |
|---|---|---|
| B1 | 0,81 | 0,81 |
| B2 | 1,76 | 1,76 |
| B3 | 5,91 | 5,92 |
| B4 | 5,93 | 6,00 |
| B5 | 6,4 | 6,41 |
| B6 | 11,71 | 11,71 |
| B7 | 19,84 | 19,85 |
| B8 (K<1000) | 29,26 | 29,26 |
| AHLS3 | 22,62 | 22,65 |
| AHLS4 | 37,49 | 37,54 |
| NOEL | 18,89 | 18,88 |
| NOEL1 | 8,01 | 8,00 |
| PRIMES | 32,91 | 32,87 |
| PRIMES1 | ~17 | 17,62 |
| FIZZBUZZ | 2,47 | 2,32 |
| FIZZBUZ1 | 1,91 | 1,76 |
| SIEVE | 24 | 30,75 |

- `B8.BAS` i prov/ har nu `K<1000`, som tiden på maskinen gäller.
- AHLS2 ger samma noggrannhet och RND-summa som maskinen (0,247559 och
  10,7994). Filerna hade `ü` för `^` och `”` för `"` (överförda från ABC:s
  teckenuppsättning); nu har de `^` och `"`.
- Öppet: FIZZBUZZ och FIZZBUZ1 är 0,15 s för snabba (utskriften: väntar
  processorn på bildminnet?), och SIEVE 28 % för långsam (?).
- Samma prov i Forth (`../forth/tidsprov.py`, 2026-11-06): se
  `../forth/README.md`. Forth är 2,8–6,2 gånger snabbare, PRIMES1 1,1.

## Summern på IN 5 (2026-11-07)

Datorkortets avkodare (8205, 15F) klockar en 74LS390 (4E) när port 5
läses, och dess utgång SOUND driver piezosummern på kommunikations-
modulen. `las_port` slår om `summer` och skriver tiden (T-cykler) i
`summer_tid`; `abc802_summer` gör prov av det sedan förra anropet
(SDL-fönstret och webbsidans `summer()`/`summern_prov()`). `-i` skriver
antalet omslag. PRINT CHR$(7) ger 200 omslag (BEEP på $0241) och ett pip.

Kvar i tangentbordet (bokens avsnitt I emulatorn): 600 bit/s,
OUT 34-kommandona (klick, repetition, klockan, CAPS), läget upp och ned.

## Webbsidan: av/på, lampor, skrivarens fart (2026-11-09)

Efter ABC80-sidan. Av/på laddar om sidan, så att allt nollställs.
Lamporna vid Skivminnet, Kassetten och CH A och CH B visar tre lägen:
inkopplad, används (250 ms efter ett byte) och gul varning när det
kommer något på en kontakt som inte visas. Skrivarkortet (kort 60)
svarar upptaget ($97) i 1/80 s efter varje tecken (`tid`, `tecken_tid`,
`klar`); det går också att välja "Genast". SDL-fönstret har kvar en
skrivare som alltid är klar. Utskriftslappen kan dras större, och dess
storlek sparas; en sidmatning (FF eller minst fem LF) visas som en
streckad linje. Att den riktiga skrivaren klarade 80 tecken/s är en
uppskattning.
