# Forthaid II: Forthaid med TKN80 (utanför boken)

**Inte med i boken.** Ett försök i två delar. Först flyttas Forthaid
(`../forthaid/`) ur vägen för TKN80, MyAB:s kort för 80 tecken per rad.
Sedan får den mer plats genom bankning i den trådade tolken. Planen står
i `PLAN.md`. Forthaid ändras inte av det här: `gor.sh` kopierar dess
källor och bokens och ändrar bara kopiorna.

Läget 2026-10-26: alla åtta steg är klara. Hur Forthaid II används står
i `MANUAL.md`, och `README.md` är en engelsk sammanfattning av den här
filen.

Steg 1 är Forthaid utan bankning under `$5800`, steg 2 bankkortet i emulatorn och steg 3 fjärrorden i
korskompilatorn. I steg 4 ligger alla Forthaids kommandon i en fast del
och tre banker, i steg 5 räknar editorn och LIST med 80 kolumner, och
steg 6 är mätningen. I steg 7 ligger kompilatorn i en fjärde bank, och
SAVEW och LOADW sparar de egna orden på skivan och laddar dem i en
femte.

| Fil             | Vad                                                                  |
|-----------------|----------------------------------------------------------------------|
| `gor.sh`        | bygger `forthaid2.bin` och bankerna ur kopior av Forthaids och bokens källor; `-1` bygger steg 1, `-p` kör proven |
| `banker.txt`    | vilken bank Forthaids ord hamnar i                                   |
| `prov/`         | Forthaids prov; `prov/steg1/` har det som är annorlunda i steg 1      |
| `kort/prov.sh`  | provet för bankkortet: byter bank från BASIC och läser tillbaka |
| `bank/fc2.py`   | korskompilatorn med banker, på bokens `e7fc.py`                 |
| `bank/fb.inc`   | fjärrorden i kärnan: DOFAR och vägen tillbaka                    |
| `bank/prov.sh`  | provet för fjärrorden, med exempel 7:s tolk och två banker; mäter anropen |
| `bank/fil.fs`, `bank/fil.inc` | SAVEW och LOADW, och FCALL som anropar ROM:ens filrutiner |
| `prov/fil.sh`   | provet för SAVEW och LOADW, med DOS:en och en kopia av exempeldisketten |
| `matning.sh`    | tiderna för FIND, VAR, REN och CHANGE mot Forthaid och Smartaid 3, anropen över bankgränsen och storleken |
| `MANUAL.md`     | manualen: att starta, kommandona, SAVEW och LOADW, TKN80, minnet     |
| `README.md`     | på engelska, en sammanfattning av den här filen                      |
| `PLAN.md`       | planen, med stegen                                                   |
| `forthaid2.bin` | den fasta delen, 4 080 byte på `$4000`–`$4FEF`                       |
| `forthaid2_b0.bin` … `_b4.bin` | bankerna på `$5000`                                 |
| `steg1.bin`     | steg 1, kretsen utan banker, 6 128 byte på `$4000`–`$57EF`           |


## Varför Forthaid inte går med TKN80

TKN80 har sitt bildminne på `$5800`–`$5FFF`. Den övre halvan är samma
minne som ABC80:s bildminne på `$7C00`–`$7FFF` (band 2, kapitlet
Skärmen). Forthaid krockar med kortet på tre sätt:

- Kretsen ligger på `$4000`–`$5FFF`, så dess sista 2 KB och kortets
  bildminne ligger på samma adresser. I emulatorn syns Forthaids kod som
  tecken på skärmen.
- Vektortabellen KTAB ligger på `$5F34` (I = `$5F`), alltså i kortets
  minne.
- Variablerna ligger i bildminnets osynliga byte, `$7C78`–`$7F7F`, som
  i boken. I 80-teckensläget hamnar `$7C78`–`$7C7F`, `$7D78`– och så
  vidare mitt i raderna 12–15 och skrivs över av utskrift och rullning.
  Bara `$7xF8`–`$7xFF` är osynliga i båda lägena (figuren tkn80rader i
  band 2).


## Minneskartan utan bankning (steg 1)

Steg 1 byggs med `gor.sh -1` till `steg1.bin`.

| Var             | Vad                                                              |
|-----------------|------------------------------------------------------------------|
| `$4000`–`$57EF` | kretsen; KTAB på `$5734` (I = `$57`)                             |
| `$7CF8`–`$7EFD` | 11 variabler, i gömställena som klarar sig i båda lägena          |
| `$7EFE`–`$7EFF` | stegningens flagga (STEPF)                                        |
| `$FB00`–`$FB4F` | radbufferten, som förut                                          |
| `$FB50`–`$FB6B` | resten av variablerna                                            |
| `$FB6C`–`$FB6F` | SAVESP och S0 (förut `$7C78` och `$7C7A`)                         |
| `$FB70`–`$FC7F` | de egna orden, 272 byte (förut 304)                              |
| `$FC80`–`$FCFF` | stackarna medan kretsen läser en rad eller kör ett kommando, som förut |

`$7FF8`–`$7FFF` används inte. Där ligger exempel 1:s variabler i boken.

Variablerna fördelas i den ordning de definieras i källorna (bokens
`e7.fs` och `e7i.fs`, sedan `fa.fs`). Tolkens egna variabler, som
STATE, DP och LAST, hamnar därför i bildminnet. De ska överleva mellan
kommandona, och bildminnets gömställen rör ROM:en inte när den läser
eller skriver en fil. Variablerna i kassettbufferten används bara medan
ett kommando körs, och då läses ingen fil. De egna orden ligger kvar i
kassettbufferten, som i Forthaid, och försvinner liksom där när en fil
läses eller skrivs.


## Vad det kostar (fråga 1 i planen)

Forthaid använder 8 101 av 8 192 byte. Under `$5800` finns 6 144. Det
som måste bort är omkring 2 KB, ungefär en fjärdedel av kretsen.

`gor.sh` tar bort kommandona i `TABORT` och de av Forthaids egna ord
(`fa.fs`, `fak.inc`) som bara de använder. HELP:s lista och tabellen i
CMD? följer med. Prövade urval, med lediga byte före KTAB på `$5734`:

| Bort                          | Lediga |
|-------------------------------|-------:|
| PEEK VAR REN OLD              |   −475 |
| PEEK VAR REN CHANGE           |   −180 |
| PEEK VAR REN OLD AUTO         |   −255 |
| PEEK VAR OLD CHANGE           |   −303 |
| PEEK REN OLD CHANGE           |   −690 |
| **VAR REN OLD CHANGE**        |  **2** |
| PEEK VAR REN OLD CHANGE       |    175 |
| PEEK VAR REN OLD CHANGE AUTO  |    395 |

VAR och CHANGE delar det mesta av sin kod: variabeltabellen, MATCHVAR
och SETVARS i `fak.inc`, och VARNAME och NAME? i `fa.fs`. Därför räcker
inte det ena av dem. Valet blev VAR, CHANGE, REN och OLD, med 19
hjälpord. Kvar är SYS, LIST, PEEK, FIND, DEL, AUTO, ED, START, HELP,
WORDS, BYE, EX och FORTH, inkopplingen med skärmeditorn och stegningen.
Kretsen har 2 lediga byte före KTAB och 16 sist.

Svaret på fråga 1 är alltså att Forthaid utan bankning kan gå med
TKN80, men bara om den förlorar kommandona för variabler och radnummer.
Det är de som Smartaid-användaren troligen saknar mest. Det är också
skälet till bankningen i nästa steg.


## Proven i steg 1

`prov/kor.sh -1` kör de 19 proven i 9913 och 11273, först som i Forthaid
och sedan med TKN80 (`emulator/abc80 -tkn80 roms/tkn80-iii.rom`) i båda
lägena. Med kortet jämförs skärmen med samma prov utan kortet. I
40-teckensläget (`Z=INP(3)` och en rensad skärm först) ska skärmen vara
densamma. I 80-teckensläget ska tecknen vara desamma bortsett från
radbrytningen. Alla 114 körningar går igenom, också med emulatorn i
`emulator/` i stället för bokens värdemulator. Stegningens prov räknar
ett varv längre än i Forthaid (310 i stället för 309), eftersom
tiderna mellan tangenterna faller annorlunda.

Skärmeditorn klarar proven i 80-teckensläget, men proven har inga rader
längre än 40 tecken. Med längre rader går det inte, och steg 1 har inte
plats för rättelsen; den finns bara med bankerna (steg 5).


## Bankkortet (steg 2)

**Kortet har inte funnits.** Det är påhittat för försöket och finns bara
i emulatorn. Det är det kort som emulatorns flagga `-b` redan beskriver
(bokens exempel 4 i band 2 använder det med två banker på `$4000`):

- upp till 8 banker, här 2 KB var, i ett fönster på `$5000`–`$57FF`;
- en skrivning till `$5040`+n väljer bank n, vilket värde som helst;
  en skrivning till `$5040`+n där bank n saknas gör ingenting;
- fönstret är skrivskyddat som en krets, och bank 0 är inkopplad vid
  start.

Den fasta delen läggs med `-l` på `$4000`–`$4FFF`, bredvid fönstret:

    abc80 -l fast.bin@4000 -b b0.bin,b1.bin,b2.bin@5000 ...

Sättet att välja bank är detsamma som Super Smartaids (`$4040` och
`$4041`, band 2), flyttat till fönstrets adress. Det är valt för att
emulatorn och webbsidan redan har det, inte för att Forthaid II ska
efterlikna Smartaid. Skillnaden ligger i hur tolken anropar orden i en
annan bank (steg 3). Kortet har inget eget RAM. Variablerna ligger där
steg 1 lade dem.

Adresserna `$5040`–`$5047` kan ändå innehålla kod och data i varje bank,
eftersom kortet bara lyssnar på skrivningar. Fönstret slutar under
`$5800`, så TKN80:s bildminne berörs inte.

`kort/prov.sh` gör fyra banker där bank n har n på `$5000` och `$A0`+n
på `$57FF`, och en fast del fylld med `$5A`. Ett BASIC-program väljer
banken med `POKE 20544+N,0` och läser båda ändarna och den fasta delen
med PEEK. Det prövar också att `POKE 20480,99` inte går igenom och att
en skrivning till `$5044` (ingen bank 4) inte byter bank. Provet går i
9913 och 11273, och med TKN80 i 80-teckensläget.

Webbsidan kan redan koppla in banker (`e.bank`), men bara för bokens
exempel, där alla filer är banker på samma adress. En fast del bredvid
bankerna (`?krets=forthaid2`) läggs till när det finns en Forthaid II
med banker att köra (steg 4).


## Fjärrorden (steg 3)

Ett ord i en bank anropas från den fasta delen eller från en annan bank
genom ett **fjärrord** i den fasta delen. Det är fem byte:

    F_C1:   CALL DOFAR
            DEFB 1,2         ; bank 1, plats 2 i bankens tabell

Varje bank börjar med sitt nummer på `$5000`, antalet ord i tabellen på
`$5001` och från `$5002` en tabell med adresserna till de ord som
anropas utifrån. Kortet går inte att läsa av, så det är numret i banken
som säger vilken bank som är inkopplad. DOFAR (`bank/fb.inc`) gör som
DOCOL men lägger också den inkopplade banken på returstacken. Sedan
kopplar den in den nya banken, slår upp ordet i dess tabell och hoppar
dit med IP = FARX. När ordet är klart ger dess EXIT IP = FARX, och NEXT
kör FAREXIT, som kopplar tillbaka den gamla banken och gör EXIT till
det anropande ordet. Returstacken under anropet, med högre adresser
överst:

    IP i det anropande ordet
    den bank som var inkopplad
    FARX                       <- IX, lagt dit av ordets DOCOL

Ordet i banken vet inte att det anropas utifrån. Ett anrop inom banken
och ett anrop av den fasta delen är vanliga adresser i tråden, som
förut. DOFAR, FARX och FAREXIT är 61 byte i kärnan.

Tabellen gör att den fasta delen inte behöver någon adress ur
bankerna. `fc2.py` skriver den fasta delen för sig och varje bank med
hela den fasta delen först, så att etiketterna där blir desamma, och
`prov.sh` tar ut `$4000`–`$4FFF` ur den ena och `$5000`–`$57FF` ur de
andra. En bank kan alltså göras om utan att den fasta delen ändras,
vilket behövs om banker ska laddas från skivan (steg 7).

### Korskompilatorn

`bank/fc2.py` läser in bokens `e7fc.py` och använder dess läsare och
kompilator som de är. Bara bygget är nytt. Källfilerna anges med bank:

    python3 fc2.py -H -n '$C000' -e e7fc.py -k kärna.inc -o p \
        e7.fs e7i.fs f1.fs b1a.fs@1 f2.fs b2.fs@2 b1b.fs@1 f3.fs

En fil utan `@` hamnar i den fasta delen, `fil@n` i bank n, och flera
filer kan dela bank. Ett ord får ett fjärrord om det anropas från den
fasta delen eller en annan bank, används med `[']` eller är en ingång.
`[']` ger alltid fjärrordet, så att EXECUTE fungerar var det än görs.
Med `-H` (huvuden, som i Forthaid) får alla bankade ord ett fjärrord,
och huvudet ligger före fjärrordet i den fasta delen. Ordlistan ligger
då helt i den fasta delen och tolken söker som förut. Ett ord som
tolken hittar och kör eller kompilerar in i ett nytt ord är fjärrordet.

Ett ord kan bara använda ord som redan är definierade, så två banker
kan bara anropa varandra fram och tillbaka om de delas på flera filer,
som `b1a.fs` och `b1b.fs` i provet.

### Provet

`bank/prov.sh` kompilerar exempel 7 med tolken (`e7.fs`, `e7i.fs`) och
provets ord: den fasta delen blir 3 599 byte, bank 1 122 byte och bank
2 74 byte; bank 0 är tom och inkopplad vid start. Provet
skriver vilken bank som är inkopplad i varje ord:

    A1/1
    B2/2 VIA/2 C1/1 VIA/2 C1/1 B2/2 A1/1 0

A1 i bank 1 anropar B2 i bank 2 i en DO-slinga. B2 anropar VIA i den
fasta delen, som anropar C1 i bank 1 medan bank 2 är inkopplad, och
sedan C1 direkt, och efter anropen har B2 en egen slinga. Varje ord
ser sin egen bank igen efter ett anrop, och slingornas index stämmer.
Den fasta delen kör också C1 med `['] C1 EXECUTE`. I tolken körs A1 och
B2, ett nytt ord i RAM anropar B2, och IMM2 i bank 2 är IMMEDIATE och
körs när ett ord kompileras. Allt går i 9913 och 11273, och med TKN80 i
80-teckensläget.

### Vad ett anrop kostar (fråga 2 i planen)

TID mäter 10 000 anrop av ett tomt ord med klockan (`$FDF0`, 50 Hz) och
drar ifrån en tom slinga. En tick är 6 T per anrop.

| Anrop                              | T per anrop |
|------------------------------------|------------:|
| i den fasta delen (T0)             |         335 |
| inom banken (T1)                   |         335 |
| från bank 1 till bank 2 (T2)       |         754 |
| från den fasta delen till bank 1 (T3) |      760 |

Räknat i koden kostar fjärrordet 423 T mer än ett vanligt anrop:
`CALL DOFAR` 17, DOFAR 226, NEXT till FARX 38, FAREXIT 74 och EXIT 68.
Mätningen ger 419–425. Ett anrop över gränsen tar alltså drygt dubbelt
så lång tid som ett vanligt, omkring 250 µs mot 112 µs. Ett anrop inom
banken kostar ingenting extra.

I platsen kostar ett ord som anropas utifrån fem byte i den fasta delen
och två i bankens tabell, och med `-H` huvudet i den fasta delen.
Returstacken får tre celler per anrop över gränsen i stället för en.

### Det som inte går

- Data i en bank finns bara när banken är inkopplad. En text från `."`
  eller `S"` i en bank kan skickas till TYPE i den fasta delen, men inte
  till ett ord i en annan bank.
- Ett ord som tar bort det anropande ordets returadress (`R> DROP` för
  att lämna två nivåer) fungerar inte över gränsen, eftersom banken och
  FARX ligger emellan. Forthaid gör inte så; dess `>R` och `R>` står
  parvis i samma ord.
- Orden i maskinkod (`;W`) låg i steg 3 alla i den fasta delen. I steg
  4 kan de också ligga i en bank (nedan).


## Kommandona i bankerna (steg 4)

Alla Forthaids kommandon finns med igen. Den fasta delen har kärnan,
tolken (`e7.fs`, `e7i.fs`), fjärrorden och de av Forthaids ord som flera
banker eller inkopplingen (KBD) behöver. Kommandona ligger i tre banker:

| Bank | Vad                                       | Byte  | Lediga | Ord | I tabellen |
|-----:|-------------------------------------------|------:|-------:|----:|-----------:|
| 0    | LIST, PEEK, FIND, SYS, START, ED, DEL     | 1 971 |     77 |  31 |         15 |
| 1    | VAR och CHANGE                            | 1 145 |    903 |  14 |          2 |
| 2    | REN, AUTO, OLD och HELP                   | 1 347 |    701 |  19 |          5 |

| Var             | Vad                                                              |
|-----------------|------------------------------------------------------------------|
| `$4000`–`$4FEF` | den fasta delen; KTAB på `$4F34` (I = `$4F`), 107 byte lediga före den och 16 sist |
| `$5000`–`$57FF` | fönstret med bankerna, 2 KB var                                   |
| `$7CF8`–`$7EFF` | variablerna och STEPF, som i steg 1                               |
| `$FB50`–`$FB6F` | resten av variablerna, SAVESP och S0, som i steg 1                |
| `$FB70`–        | de egna orden, som i steg 1                                       |

Fördelningen står i `banker.txt`, en rad per bank och grupp av ord:

    1: ADV1 VARNAME VKEY MATCHVAR USES? .VAR SETVARS ALLVARS VAR

Ord som inte står där ligger i den fasta delen. Ett litet hjälpord som
PEEKC, SKIPBL eller FINDLN står i flera banker och får en kopia i var
och en. Det kostar några byte men sparar ett fjärranrop varje gång det
används inom banken. Ett anrop utifrån går till kopian i den första
banken. Det som är skrivet i Forth i `fa.fs` ändras inte. Korskompilatorn
lägger varje ord där kartan säger.

### Huvudena

Med `-H` får varje bankat ord ett huvud och ett fjärrord i den fasta
delen (steg 3). För 56 ord i banker får det inte plats. `fc2.py -h` ger därför bara huvud och fjärrord åt de ord som
behöver dem: de som anropas från den fasta delen eller en annan bank,
används med `[']` eller är ingångar, och de som står på en rad `h:` i
kartan. Där står de ord som MANUAL.md säger går att skriva vid OK, som
`H.` och `LISTLINE`, också när de bara används i sin bank. Övriga ord i
bankerna syns inte för tolken. Det blir 22 fjärrord.

### Orden i maskinkod

DETOK, MATCHVAR, SEARCH, SETVARS, (INS) och EDIT i `fak.inc` är stora
och används bara av kommandona, så de ligger i bankerna. Fjärrordet för
ett sådant ord fungerar som för ett kolonord: DOFAR hoppar till koden
med DE = FARX, och ordets `JP NEXT` kör då FAREXIT. `fc2.py` flyttar
ordets rader ur kärnan till banken och kontrollerar att ingen kod i den
fasta delen eller i en annan bank hoppar direkt till dem. De övriga
orden i `fak.inc` ligger kvar i den fasta delen.

### Proven

`prov/kor.sh` kör alla 25 av Forthaids prov i 9913 och 11273, utan TKN80
och med TKN80 i båda lägena: 150 körningar, alla ok, både i bokens
värdemulator och i `emulator/`. Stegningens prov `steg2` stannar i
`emulator/` på rad 20 i stället för 10, eftersom tiderna mellan
tangenterna faller annorlunda än i steg 1. Därför har den egen utdata (`steg2.emu.out`). Steg 1:s utdata ligger i
`prov/steg1/`, och `kor.sh -1` och `bank/prov.sh` går fortfarande
igenom.

På webbsidan sätter `?krets=forthaid2` (eller valet Forthaid II) i den
fasta delen på `$4000` och bankerna på `$5000`, också med TKN80
(`&tkn80=pa`). Prövat i Chromium och WebKit i båda lägena.


## Editorn med 80 kolumner (steg 5)

Forthaids editor och LIST räknar med 40 tecken per rad på fyra ställen:
EDLIN byter rad när kolumnen når 40, CTRL-S går inte förbi kolumn 39,
CTRL-D och CTRL-P flyttar 39 − kolumnen tecken, och LISTLINE räknar
skärmraderna som längden delad med 40. Med TKN80 i 80-teckensläget och
en rad längre än 40 tecken går det fel: ED tror att raden tog två
skärmrader, flyttar markören två rader upp och läser in fel rad (ERR 31).

ROM:ens egna rutiner som EDLIN använder (GETKEY, RUB, CRLF) ligger i de
två kilobyte som TKN80 byter ut och klarar sig själva. Det som behövs är
radlängden. Den syns i radtabellen: rad 0 börjar på `$58xx` i
80-teckensläget och annars på `$7Cxx` (`PEEK(885)`, band 2). `gor.sh`
ändrar kopiorna:

- `fak.inc`: en rutin EDW ger radlängden, 40 eller 80, i A; EDPUT, EDRGT
  och EDREST räknar med den i stället för 40 och 39;
- `fa.fs`: ett ord `COLS ( -- n )` gör samma sak, och LISTLINE delar med
  `COLS`; LIST och ED räknar då skärmraderna rätt, och MORE? fyller
  sidan med 22 skärmrader som förut.

COLS ligger i bank 0 med LISTLINE (`banker.txt`). Bank 0 blir 49 byte
större (77 lediga); den fasta delen ändras inte. Läget läses varje gång,
så det går att byta läge med `INP(3)` och `INP(4)` mellan två kommandon.
Steg 1 (`gor.sh -1`) har bara 2 byte lediga och får inte ändringen.

Tre nya prov: `ed4` (ED på en rad längre än 40 tecken, som körs som de
andra och jämförs mellan lägena), `ed80` (CTRL-D och CTRL-P på kolumn
45 och CTRL-S förbi kolumn 39) och `list80` (15 långa rader ryms på en
sida). Prov vars namn slutar på 80 körs bara med TKN80 i
80-teckensläget och jämförs med `N.t80.out`, eftersom skärmen ska vara
en annan än i 40-teckensläget. Före ändringen ger `ed4` och `ed80` ERR
31 i 80-teckensläget. Nu går alla 160 körningar igenom, och `kor.sh -1`
(114) som förut.

Webbsidan (`?krets=forthaid2`) är ombyggd och prövad i Chromium och
WebKit med samma rad som i `ed4`, med och utan TKN80.


## Mätningen (steg 6)

`./matning.sh`, 2026-10-26, i emulatorn med 9913, på samma program och
på samma sätt som Forthaids `matning.sh` (se `../forthaid/LASMIG.md`):
den emulerade maskinens tid i ms för ett program med programmets längd
utöver ett med två rader. Forthaid II mäts utan TKN80 och med TKN80 i
80-teckensläget (rad II, TKN80). Forthaid och Smartaid 3 mäts igen i
samma körning.

| Program  | Kretsen     | FIND Z9 | VAR    | VAR Z9 | REN 10,10 | CHANGE B1 |
|----------|-------------|---------|--------|--------|-----------|-----------|
| 2 060 B  | Forthaid II | 0,35 s  | 0,60 s | 0,18 s | 0,41 s    | 0,18 s    |
|          | II, TKN80   | 0,35 s  | 0,61 s | 0,20 s | 0,43 s    | 0,19 s    |
|          | Forthaid    | 0,35 s  | 0,60 s | 0,18 s | 0,41 s    | 0,18 s    |
|          | Smartaid 3  | 0,08 s  | 0,21 s | –      | 0,23 s    | 0,14 s    |
| 8 180 B  | Forthaid II | 1,50 s  | 1,05 s | 0,74 s | 2,21 s    | 0,72 s    |
|          | II, TKN80   | 1,51 s  | 1,06 s | 0,76 s | 2,22 s    | 0,74 s    |
|          | Forthaid    | 1,50 s  | 1,05 s | 0,74 s | 2,21 s    | 0,72 s    |
|          | Smartaid 3  | 0,42 s  | 0,74 s | –      | 1,60 s    | 0,62 s    |
| 12 266 B | Forthaid II | 2,29 s  | 1,36 s | 1,12 s | 3,90 s    | 1,09 s    |
|          | II, TKN80   | 2,29 s  | 1,37 s | 1,14 s | 3,91 s    | 1,10 s    |
|          | Forthaid    | 2,29 s  | 1,36 s | 1,12 s | 3,90 s    | 1,09 s    |
|          | Smartaid 3  | 0,66 s  | 1,10 s | –      | 3,00 s    | 0,93 s    |

Forthaid II utan TKN80 tar lika lång tid som Forthaid, på tio
millisekunder när (mätningens upplösning), och Forthaid och Smartaid 3
ger samma tider som när Forthaid mättes 2026-10-20. Jämförelsen med
Smartaid 3 står alltså kvar: 3,5–4,5 gånger långsammare med FIND,
1,2–3 gånger med VAR, 1,3–1,8 med REN och 1,2–1,3 med CHANGE. Med
TKN80 i 80-teckensläget tar kommandona 0–20 ms längre, lika mycket
oavsett programmets längd. Det är utskriften genom TKN80:s rutiner,
inte bankningen.

### Anropen över bankgränsen

`matning.sh` räknar med emulatorns `-P` hur många gånger DOFAR körs
medan kommandot körs, utan det anrop av EDIT (bank 0) som läser in
kommandoraden:

| Kommando  | Anrop | Varför                                              |
|-----------|------:|-----------------------------------------------------|
| FIND Z9   |     1 | FIND                                                |
| VAR       |     1 | VAR                                                 |
| VAR Z9    |     4 | VAR, och PAGE, LISTLINE och MORE? i bank 0 för raden med Z9 |
| REN 10,10 |     1 | REN                                                 |
| CHANGE B1 |     1 | CHANGE                                              |

Antalet beror inte på programmets längd. Varje kommando ligger med sina
hjälpord i en bank, och slingorna över raderna stannar i banken eller
anropar den fasta delen, vilket inte kostar något extra. Bara VAR med
namn går över till bank 0, två gånger för varje rad där variabeln står
och en gång för PAGE. Med 423 T per anrop (steg 3) är det 0,14 ms för
de flesta kommandona; en variabel på 40 rader skulle kosta omkring 12
ms.

Fråga 2 i planen har alltså två svar. Ett enskilt anrop över gränsen
tar drygt dubbelt så lång tid som ett vanligt (754 T mot 335), men med
kommandona fördelade som i `banker.txt` märks det inte i tiden, eftersom
gränsen ligger mellan kommandona och inte inne i deras slingor. Hade
LISTLINE eller DETOK legat i en annan bank än FIND, hade varje rad i
programmet kostat ett anrop till.

### Storleken

| Del                          | Byte  |
|------------------------------|------:|
| den fasta delen (`forthaid2.bin`, utan de 107 lediga före KTAB) | 3 973 |
| bank 0                       | 1 971 |
| bank 1                       | 1 145 |
| bank 2                       | 1 347 |
| **Forthaid II**              | **8 436** |
| Forthaid (utan de lediga före KTAB) | 8 101 |

Forthaid II är 335 byte större än Forthaid. Fjärrorden är 22 à 5 byte i
den fasta delen och 2 i bankens tabell (154), DOFAR, FARX och FAREXIT
61 och bankernas nummer och antal 6. Resten är kopiorna av små hjälpord
i flera banker (PEEKC, SKIPBL, FINDLN, GETNUM och RANGE), COLS och EDW
för 80 kolumner, och huvudena för de ord som anropas utifrån. Lediga
är 107 byte före KTAB och 16 sist i den fasta delen och 77, 903 och
701 i bankerna, alltså nästan 1,8 KB, där Forthaid har 75 före KTAB och
16 sist.


## Kompilatorn i en egen bank (steg 7)

Kompilatorn är de ord i `e7i.fs` som bara behövs när man definierar
nya ord: `S,`, HEADER, REVEAL, IMMEDIATE, `:`, `;`, VARIABLE, CONSTANT,
`'`, LITERAL, IF, THEN, ELSE, BEGIN, UNTIL, AGAIN, WHILE, REPEAT, DO,
LOOP och `."`. De ligger nu i bank 3, med två rader i `banker.txt`:

    3: S, HEADER REVEAL IMMEDIATE : ; VARIABLE CONSTANT ' LITERAL
    3: IF THEN ELSE BEGIN UNTIL AGAIN WHILE REPEAT DO LOOP ."

och samma ord på rader `h:`, eftersom de skrivs vid OK och måste
hittas av tolken. Inget annat ändrades. Den yttre tolken når orden
genom sina fjärrord i den fasta delen, också de som är IMMEDIATE: biten
står i huvudet, som ligger i den fasta delen, och INTERPRET kör
fjärrordet medan ett nytt ord kompileras. Det som orden lägger i det
nya ordet (`0BRANCH`, `(DO)`, `(S")`, `TYPE`, `EXIT`, DOCOL) är adresser i
den fasta delen, så ett eget ord behöver aldrig bank 3 när det körs.
Ett ord som `'` ger för ett ord i en bank är fjärrordets adress, som
EXECUTE kör som vilket ord som helst.

| Del                    | Före  | Nu    |
|------------------------|------:|------:|
| lediga före KTAB       |   107 |   443 |
| bank 3                 |     – |   485 (1 563 lediga) |
| fjärrord               |    22 |    43 |
| **Forthaid II**        | **8 436** | **8 585** |

Den fasta delen får 336 byte mer. Det är kompilatorns kod (485 byte i
banken, där bankens nummer och tabell är 44) minus 21 fjärrord à 5 byte
som nu ligger kvar bredvid huvudena. Helheten blir 149 byte större,
fjärrorden och bankens tabell. Lediga är nu 443 byte före KTAB och 16
sist i den fasta delen och 77, 903, 701 och 1 563 i bankerna, drygt
3,6 KB.

Ett ord som kompileras gör ett anrop över bankgränsen för `:`, `;` och
varje IF, DO, `."` med flera i definitionen, omkring 420 T extra var
(steg 3). Det märks inte vid tangentbordet. Kommandona och de egna
orden när de körs påverkas inte.

Det nya provet `prov/komp.test` använder alla kontrollord, `."`,
VARIABLE, CONSTANT, LITERAL, IMMEDIATE och `'`, och ryms på en skärm med
40 kolumner så att det kan jämföras med 80-teckensläget. `prov/kor.sh`
kör nu 26 prov, 166 körningar, alla ok; `kor.sh -1` (steg 1, där
kompilatorn ligger i kretsen som förut) 120 ok, och `bank/prov.sh` ok.
Webbsidan tar bank 3 med av sig själv (`bygg.sh` tar alla
`forthaid2_bN.bin`); prövad i Chromium och WebKit utan TKN80 och med
TKN80 i båda lägena.

## De egna orden på skivan (steg 7)

Den andra halvan av steg 7 är SAVEW och LOADW. SAVEW namn skriver de
egna orden till en fil, och LOADW namn läser dem tillbaka, också i en
annan körning. Orden laddas i RAM:et mellan BASIC och stacken, inte i
en bank: emulatorn ändras inte, och de laddade orden ligger kvar när
nya ord definieras på `$FB70` som förut. De två kommandona finns i
KTAB, så de går också direkt i BASIC, och HELP visar dem på en egen
rad.

Filen har ett huvud på 8 byte: `FW`, LATEST, längden n och det sista
ordets huvud räknat från filens början. Sedan kommer orden, n byte där
varje adress till ett eget ord är räknad från filens början, och sist
en bitkarta med en bit per byte: här börjar en adress som ska flyttas.
LATEST är adressen till den fasta delens sista ord. En fil från en
annan Forthaid II, där den fasta delens ord ligger på andra platser,
ger därför `?` i stället för att laddas.

SAVEW går igenom orden från LAST, huvud för huvud. Länken flyttas, och
i ett kolonord varje cell som pekar in bland de egna orden. Talet efter
LIT och texten efter `(S")` hoppas över. Allt annat i ett kolonord är
ett ord eller ett hopp, och hoppen efter BRANCH, 0BRANCH och `(LOOP)`
pekar in i samma ord, så de flyttas på samma sätt. En VARIABLE eller
CONSTANT sparas som den är. En konstant som håller adressen till ett
eget ord pekar därför fel efter LOADW. Orden och bitkartan byggs i
BASIC:s lediga minne över HEAP och skrivs sedan block för block.

LOADW läser filen till minnet under STKTOP och sänker STKTOP lika
mycket. BASIC tar sitt SP därifrån vid nästa kommando. Den första LOADW
sparar det gamla STKTOP (TOP0, med en kontrollsumma), och de laddade
orden slutar alltid där. En ny LOADW tar alltså samma minne, och
BASIC:s minne krymper bara med den största filen. LAST pekar sedan på
filens sista ord, vars länk går till den fasta delen, och DP på
`$FB70`, som efter FORTH.

Filrutinerna är ROM:ens OPEN, CLOSE, DVGET och DVPUT med en FCB på 15
byte. FCALL i `bank/fil.inc` sätter registren, anropar rutinen och
lämnar HL och DE i ett fält. Den byter stack till BASIC:s lediga minne,
för DOS:en använder mer stack än vad som ryms under Forths stackar.
OPENF vägrar enheter vars buffert ligger i kassettbuffertarna
(`$FB00`–`$FCFF`), där de egna orden och KBD:s stackar finns. Med
kassetten blir det alltså `?`, och SAVEW och LOADW kräver ett
skivminne. En fil som inte finns ger fel 21 i BASIC, som när OPEN
misslyckas annars. Namnet får ingen filtyp om man inte anger en.

LOADW efter Z=CALL(16384,n) är begränsat. Då ligger BASIC:s stack
(SAVESP) under STKTOP, och Forths stackar under den, så minnet under
STKTOP kan inte tas. LOADW laddar bara om orden ryms över STKTOP, i
minnet som en tidigare LOADW i BASIC har tagit, och ger annars `?` utan
att något ändras. I BASIC och efter FORTH är BASIC:s stack tom och allt
minne under STKTOP ledigt.

Orden ligger i bank 4, 1 781 byte (267 lediga), och bara SAVEW och
LOADW har huvuden och fjärrord. HELP:s nya rad ligger i bank 2 (683
lediga). Den fasta delen har 413 byte lediga före KTAB i stället för
443, för de två fjärrorden, KTAB och HELP:s gräns för de egna orden,
som nu är `$8000` så att de laddade orden också visas. Forthaid II är
10 414 byte i fem banker och den fasta delen.

| Del                    | Före  | Nu    |
|------------------------|------:|------:|
| lediga före KTAB       |   443 |   413 |
| bank 2                 | 1 347 | 1 365 |
| bank 4                 |     – | 1 781 (267 lediga) |
| fjärrord               |    43 |    45 |
| **Forthaid II**        | **8 585** | **10 414** |

Bokens värdemulator har inget skivminne, så provet ligger i
`prov/fil.sh` och inte i `kor.sh`. Det kör emulatorn i `emulator/` med
DOS:en på `$6000` och en kopia av exempeldisketten på kort 45, i 9913
och 11273, utan TKN80 och med TKN80 i 80-teckensläget. Det prövar SAVEW
och LOADW i BASIC och i Forth, fel signatur, saknat namn, en fil som
inte finns, ett BASIC-program med GOSUB och CALL efter LOADW, LOADW
efter Z=CALL som ryms och en som inte ryms. Alla fyra körningarna är
ok. `prov/kor.sh` är 166 ok (`help.out` har den nya raden), `kor.sh -1`
120 ok och `bank/prov.sh` ok. På webbsidan finns ABC-DOS med en
diskett, och SAVEW och LOADW prövades med systemskivan
(`?krets=forthaid2&skiva=system`) i Chromium och WebKit, utan och med
TKN80.
