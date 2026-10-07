# abc80 – manual

`abc80` är en ABC80 i terminalen. Den kör ABC80:s ROM, eller BASIC II för
ABC80, med de kretsar man lägger till, och den räknar processorns tid i
T-cykler, så att ett program tar lika lång tid som på en riktig ABC80.
Den skrevs för boken *ABC80 inifrån*, och bokens exempel provas med den.

    abc80 [flaggor]              ABC80 i terminalen; Ctrl-] avslutar
    abc80 -t ms [flaggor]        batchläget: kör, och skriv sedan skärmen

## Flaggorna

| Flagga | |
|---|---|
| `-r fil` | ROM:en från $0000: 16 KB (ABC80:s BASIC) eller 32 KB (BASIC II för ABC80). Utan `-r`: `../disass/abc80/abc80new.rom` räknat från där programmet ligger |
| `-l fil@adr` | en krets: filen läses in på adressen (hex) och är skrivskyddad. Flera `-l` går bra |
| `-b f0,f1,…@adr` | en krets med banker, högst 8 à 4 KB. En skrivning till adr+$40+n väljer bank n; bank 0 är vald från början |
| `-m kb` | RAM överst i minnet: 16 (standard, $C000–$FFFF), 32 ($8000–) eller 48 (också $8000–; under $8000 sitter inget RAM) |
| `-k text` | tangenter att trycka efter starten (se *Tangenttexten*) |
| `-f fil` | en textfil att skriva in som tangenter, rad för rad; t.ex. ett BASIC-program |
| `-K ms,ms` | hur länge varje tangent hålls nere och sedan är släppt; standard `40,60` |
| `-s text` | tecken att ta emot på V24, efter tangenterna (8 bitar, ingen paritet, en stoppbit) |
| `-S baud` | V24:s hastighet; standard 1200 |
| `-t ms` | batchläget: kör så här länge efter sista tangenten och skriv sedan skärmen på standard ut |
| `-d adr,n` | batchläget: skriv också n bytes av minnet från adr (hex) efter skärmen |

`-k` och `-f` skrivs i den ordning de står, så att man kan förbereda
med `-k`, skriva in ett program med `-f` och köra det med ett `-k` till.

### Tangenttexten

I texten till `-k` och `-s`:

| | |
|---|---|
| `\r` | RETURN |
| `\xHH` | koden HH (hex), t.ex. `\x03` = CTRL-C (BREAK) |
| `\wN` | vänta N ms (bara `-k`) |
| `\.` | ingenting: skiljer en paus från en siffra efter, `\w50\.1` |
| `\\` | ett `\` |
| `å ä ö Å Ä Ö é É ü Ü` | (UTF-8) ABC80:s koder för dem |

Skriv texten inom `'…'` i skalet, så att det inte ändrar `\` eller `$`.

## I terminalen

Skärmen ritas i terminalen, 40 tecken × 24 rader, och tangentbordet
läses därifrån.

| Tangent | ABC80 |
|---|---|
| Ctrl-] | avslutar emulatorn |
| Ctrl-C | BREAK (CTRL-C) |
| ← | ← (kod 8) |
| → | → (kod 9) |
| backsteg | ← |
| RETURN | RETURN |
| å ä ö … | ABC80:s koder för dem |

Emulatorn går i ABC80:s takt, 50 bilder i sekunden. Medan det finns
kvar av `-k` och `-f`, också deras pauser, går den så fort datorn kan,
så att ett inskrivet program och väntan på att det ska köra klart går
fort.

Terminalen behöver UTF-8 och ett typsnitt med Unicodes *sextanter*
(U+1FB00–1FB3B) för grafiken; de flesta moderna typsnitt för terminaler
har dem.

## Batchläget

Med `-t` körs ABC80 utan terminal: först en sekund för starten, sedan
tangenterna, V24-tecknen och `-t` ms till. Sedan skrivs de 24
raderna av skärmen som text, utan blanktecken i slutet av raderna och
med markören som `_`. Så provas bokens exempel, och utdata går att
jämföra med `diff`.

    $ ./abc80 -t 500 -k 'PRINT 6*7\r'
    ABC80
    PRINT 6*7
     42

    ABC80
    _

## Exempel

ABC80:s BASIC och BASIC II för ABC80 (som vill ha 32 KB RAM):

    ./abc80
    ./abc80 -r ../disass/basicii/basicii80.rom -m 32

Exempel 1 i boken, kretsen på $4000. När den är inkopplad räknar den
tangenttryckningarna i skärmens övre högra hörn:

    $ ./abc80 -l ../exempel/e1.bin@4000 -t 300 -k 'Z=CALL(16384)\rPRINT "HEJ"\r'
                                       12
    ABC80
    Z=CALL(16384)
    ...

Exempel 4, en krets med två banker (skrivning till $4040/$4041 väljer):

    ./abc80 -b ../exempel/e4a.bin,../exempel/e4b.bin@4000

Exempel 6 tar emot på V24 i ett BASIC-program:

    ./abc80 -l ../exempel/e6.bin@4000 -s 'HEJ! DETTA KOM PÅ V24.\x04' \
        -k '10 Z=CALL(16384)\r20 A=CALL(16390)\r30 IF A<0 THEN 20\r40 IF A=4 THEN 70\r50 PRINT CHR$(A);\r60 GOTO 20\r70 Z=CALL(16387)\rRUN\r'

Forth (exempel 7): flytta BASIC-programmens början, skriv in
laddprogrammet, kör det (det tar ett par minuter i ABC80:s takt, här
bara sekunder, eftersom väntan står i `-k`) och starta tolken:

    $ ./abc80 -K 10,10 -k 'POKE 65052,0,208\rNEW\r' -f ../forth/e7i.bas \
        -k 'RUN\r\w150000\.NEW\r\w50\.Z=CALL(49152,3)\r' \
        -k '\w200\.: KV DUP + ;\r\w200\.7 KV .\r' -t 500 | tail -3
    : KV DUP + ; OK
    7 KV . 14 OK
    _

`-K 10,10` skriver fortare än standard; det klarar ROM:en, men inte
mycket fortare än så. Tolken läser inte tangenterna medan den skriver
` OK`, så en rad som skrivs in direkt efter förra tappar sina första
tecken; `\w200` före raden väntar ut det.

## Maskinen

Det här är vad emulatorn gör av ABC80. Boken beskriver var och en av
delarna i ROM:en som använder dem.

**Klockan.** Z80 på 2,9952 MHz (11,9808 MHz / 4), 59 904 T-cykler per
bild (20 ms). Varje instruktion tar sina T-cykler enligt Zilog; ABC80
har inga väntetillstånd.

**Minnet.** ROM:en ligger från $0000 och är skrivskyddad. Bildminnet är
$7C00–$7FFF (också med BASIC II, vars kod slutar på $77FF), och RAM:et
ligger överst efter `-m`. Kretsar från `-l` och `-b` är skrivskyddade.
Allt annat läser $FF, som en tom buss, och skrivningar dit försvinner.

**Portarna.**

| Port | In | Ut |
|---|---|---|
| $38 | tangentbordet: koden, bit 7 = tangenten är nere | – |
| $39 | – | PIO A:s styrord |
| $3A | bit 0 = V24 in (RxD); de andra bitarna 1 | – |
| övriga | $FF | – |

**Avbrotten.** NMI kommer var 20:e ms (bildens vertikalsignal); ROM:en
räknar klockan med den. PIO A:s styrord följs: läge, avbrott på och av,
vektorn (från början $34). I läge 3, som ROM:en ställer den i, ger en
nedtryckt tangent INT. I läge 1 kommer INT från bildens linjesignal på
strobeingången, 7 812,5 gånger i sekunden. Ett INT väntar tills
processorn tar emot det (eller PIO:n stänger av avbrotten). Processorn
har läge 0, 1 och 2; ROM:en använder läge 2, med I och PIO:ns vektor.

**Tangentbordet.** En tangent hålls nere 40 ms och är sedan släppt 60
ms (`-K`). Det är långt nog för ROM:ens tangentrutin, som läser
koden i avbrottet.

**Skärmen.** 24 rader à 40 tecken; rad r börjar på
$7C00 + (r mod 8)·$80 + (r div 8)·40. Tecknen är ABC80:s svenska
variant av ASCII:

| Kod | $24 | $40 | $5B | $5C | $5D | $5E | $60 | $7B | $7C | $7D | $7E |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Tecken | ¤ | É | Ä | Ö | Å | Ü | é | ä | ö | å | ü |

Bildkretsen är en teletextkrets (SAA5050). Koderna $00–$1F visas som
mellanslag. $11–$17 slår på grafik från nästa tecken och $01–$07 tillbaka
till text, och varje rad börjar med text. I grafikläget är $20–$3F och
$60–$7F rutor med 2 × 3 punkter, där bit 0–4 och 6 är punkterna, uppifrån
och från vänster, och $40–$5F är tecken som vanligt. Bit 7 i en byte är
markören, som visas inverterad. Blinkning, dubbel höjd och de andra
teletextkoderna visas som mellanslag utan verkan.

**Inte med.** Kassetten, skrivaren, ljudet och korten på ABC-bussen
(skivminne, IEC) finns inte. V24 kan bara ta emot.

## Koden

| Fil | |
|---|---|
| `z80.h` | processorns struct: registren och fyra funktioner som datorn ger (läs och skriv minnet och portarna) |
| `z80.c` | instruktionerna |
| `abc80.c` | datorn: minnet, portarna, PIO A, tangentkön, skärmen, terminalen och `main` |

**Processorn** (`z80.c`) delar operationskoden i fälten x (bit 7–6),
y (5–3) och z (2–0), som Zilogs tabeller gör, och nästan varje grupp av
instruktioner är en rad i en `switch` på dem; registrens nummer är
desamma som i operationskoden. Prefixen $DD och $FD gör HL till IX eller
IY, $CB är bitoperationerna och $ED resten. `z80_steg` kör en
instruktion och ger antalet T-cykler, `z80_nmi` och `z80_int` tar emot
avbrotten. Flaggorna räknas med de odokumenterade bitarna 3 och 5, och
de odokumenterade instruktionerna (IXH, SLL …) finns med, eftersom
program för ABC80 använder dem.

**Datorn** (`abc80.c`) kör processorn i `kor`: efter varje instruktion
läggs dess T-cykler till tiden, och sedan kommer NMI när en bild har gått,
INT om ett väntar, linjesignalen och nästa steg i tangentkön. Kön har
koderna från `-k`, `-f` och terminalen och pauserna; en tangent trycks
ned, hålls, släpps och vilar. I terminalen kör `interaktiv` en bild i
taget, ritar om skärmen när bildminnet har ändrats och väntar in nästa
bild.

## Så har den provats

- Processorn klarar *zexdoc* helt och *zexall* utom `BIT n,(HL)`, där
  flaggbitarna 3 och 5 kommer från processorns inre register MEMPTR,
  som inte är med.
- Batchläget har körts med bokens prov för ABC80:s BASIC, BASIC II och
  exemplen, och ger samma skärm som emulatorn bokens prov skrevs för,
  utom i några prov som beror på exakt hur långt ett program hinner på
  en viss tid.
