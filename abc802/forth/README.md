# Forth för ABC 802

Bokens Forth (ABC80, band 2, exempel 7: kärnan e7k.inc, tolken e7i.fs
och korskompilatorn e7fc.py), flyttad till RAM på $8000 och anpassad
till BASIC II och DOS:en i ABC 802, med fler ord och mosaikgrafik.

    ./gor.sh

ger forth.asm, forth.lst, forth.bin och skivan forth.img med FORTH.ABS
(cirka 4,9 KB) och BASICINI.SYS.

## Starta

Med forth.img i DR0: startar Forth direkt när maskinen startar. Skärmen
töms och visar

    Forth för ABC 802. BYE går till BASIC (RUN ORD, DEMO, MASK)
    och Z=CALL(32768,1) tillbaka.

Det gör BASICINI.SYS (`basicini.asm`). DOS:en läser filens första post
till sektorbufferten på $F500 vid starten, och börjar den med JP körs
koden där (DOSSTA, $6CD7 i ROM:en). Koden flyttar sig till $AF00,
eftersom DOS:en laddar FORTH.ABS genom samma buffert, laddar FORTH.ABS
med DRUN och sätter AUTOST ($FF83) på ett kommando. När BASIC är klar
kopierar ROM:en det till radbufferten och utför det utan att visa det,
som om det hade skrivits in: `PRINT CHR$(12);"Forth ..."` och
`Z=CALL(32768)`. Feltexterna ur BASICINI.SYS stängs av (ERRTXV = 0), så
BASIC:s fel blir nummer, som utan filen.

Utan BASICINI.SYS (en annan skiva med FORTH.ABS) laddas Forth från BASIC
II, i direktläge, med skivan i MF0:

    POKE 32768,14,255,195,30,96,70,79,82,84,72,32,32,32,65,66,83:;CALL(32768,32773)
    NEW
    Z=CALL(32768)

Den första raden lägger `LD C,$FF : JP $601E` (DOS:ens DRUN) och namnet
`FORTH   ABS` på $8000 och anropar dem. DRUN läser in filen och hoppar
till LADDAD, som flyttar BASIC:s BOTRAM, COMEND, FREPTR och VARBEG till
$B800. Efter NEW ligger BASIC ovanför $B800 och rör inte Forth.
Inläsningen ska ske med `;CALL`, inte `Z=CALL`: en variabel som skapas
under anropet hamnar på $8000. BASICINI.SYS gör samma sak innan BASIC
har startat, och då gör ROM:en NEW själv.

- `Z=CALL(32768)` startar FORTH: bara bildens ord.
- `Z=CALL(32768,1)` startar QUIT: orden som definierats förut finns kvar.
- `BYE` går tillbaka till BASIC; det översta värdet blir CALL:s värde.
- `Z=CALL(32768,2)` och `Z=CALL(32768,VAROOT(A$))`: se nedan.

Minnet: kärnan och ordlistan från $8000 uppåt, radbufferten TIB på
$B7B0-$B7FF. Efter start är HERE $9A6C, så 7492 bytes är fria.
BASIC har $B800 och uppåt kvar, till MEMTOP på $F186. Gränsen var
$B000 till 2026-11-09 och flyttades 2 KB uppåt för KALKYL; ORD:s
program går nu till $E7D9, och BASIC har 2,4 KB kvar till variablerna.

## Forth i BASIC-program

Forth laddas först som ovan. Sedan kan BASIC-programmet ha sina ord i
REM-rader och anropa dem med strängar:

    10 Z=CALL(32768,2) : IF Z THEN PRINT "Fel på rad";Z : STOP
    20 A$="7 KVADRAT" : PRINT CALL(32768,VAROOT(A$))
    30 A$="3 4 + 5 * KUB" : PRINT CALL(32768,VAROOT(A$))
    40 END
    100 REM FORTH : KVADRAT  DUP * ;
    110 ! FORTH : KUB DUP
    120 ! FORTH KVADRAT * ;

- `Z=CALL(32768,2)` tömmer ordboken och tolkar varje rad som börjar med
  `REM FORTH` eller `! FORTH`, i ordning. En definition får gå över
  flera rader. Z är 0, radnumret vid ett fel (felet skrivs på skärmen),
  eller -1 om `;` saknas sist. REM-texten tolkas där den står i
  programmet, så raden kan vara lång. Bara rader som börjar med REM
  räknas, inte `X=1 : REM FORTH ...`.
- `CALL(32768,VAROOT(A$))` tolkar strängen med de ord som finns och ger
  talet överst på stacken, 0 om stacken är tom och -32768 vid ett fel.
  COLD skiljer en adress från ett ingångsnummer på den höga byten.
  Har FORTH aldrig körts får strängen bildens ord.

Provat i emulatorn: programmet ovan ger 0, 49 och -22661 (35³ räknat
med 16 bitar), ett okänt ord ger `FEL ? 110`, och BASIC fungerar som
vanligt efteråt.

(REM) håller radens adress i variabeln RAD, inte på stacken, eftersom
IF, DO och BEGIN lämnar adresser på stacken mellan raderna medan en
definition kompileras.

### Spara och ladda ord

Orden sparas som källtext i programmets REM-rader: `SAVE DR0:NAMN`
sparar BASIC-programmet med dem, och efter `LOAD DR0:NAMN` kompilerar
`Z=CALL(32768,2)` dem igen (Forth måste vara laddad). Den kompilerade
ordboken sparas inte; att spara $8000-HERE som en egen fil, som Jupiter
Ace gjorde, går att lägga till senare.

### Demon

`forth.img` har DEMO (källan är `demo.bas`; `gor.sh` skriver in den i
emulatorn och sparar den). Ladda Forth, sedan `LOAD DR0:DEMO` och `RUN`:

- HILBERT: Hilbertkurvan av nivå 5 med en sköldpadda (X Y, riktning D,
  ordet STEG) och RECURSE. 1023 steg på 4,5 s.
- DRAKE: drakkurvan, papperet vikt 10 gånger, rekursivt. 1024 steg på
  4,6 s.
- LIV: Game of Life på 48 x 33 rutor om 2 x 2 punkter tills en tangent
  trycks. RUTA väljer PLOT eller UNPLOT som exekveringstoken
  (`' PLOT CONSTANT TAND`) och kör den med EXECUTE.

BASIC mäter tiden med klockan och visar svaret från Forth. Efter
HILBERT och DRAKE ritar BASIC samma kurva själv (rad 2000-2620, med
`SET DOT`). GOSUB är rekursivt, men variablerna är gemensamma, så
fälten A%() och B%() håller varje nivås riktning, som Forth har den på
stacken. BASIC II tar 30,9 s för Hilbertkurvan (6,8 gånger Forth) och
34 s för draken (7,3 gånger).

### Masken

`forth.img` har också MASK (källan är `mask.bas`) och MASKMK.ABS (källan
är `mask.asm`). Det är spelet i `../benchmark/SNAKE1.BAS`, skrivet i Forth och
i maskinkod, med samma steg: läs tangenten (a z s w), flytta huvudet,
ät eller krocka, skifta fälten med rader och kolumner ett steg och rita
hela masken igen. Ladda Forth, sedan `LOAD DR0:MASK` och `RUN`. Rad 20
kompilerar orden i REM-raderna, och rad 30-40 laddar MASKMK.ABS till
$A800, ovanför Forths ordbok. Menyn väljer Forth (`V MASK`), maskinkod
(`Z=CALL(43008,V)`) eller ett tidsprov. V är väntan per steg i avbrott
(1/93,75 s); 10 går att spela.

Tidsprovet kör från start till krock, utan tangenter och utan väntan:
masken går 76 steg åt vänster till ramen. BASIC II mäts på samma sätt,
med SNAKE1-3 på `../emulator/skivor/tidsprov.img` och klockan läst när
GAME OVER kommer:

| Masken | Tid | gånger SNAKE1 |
|---|---|---|
| SNAKE1.BAS | 6,87 s | 1 |
| SNAKE2.BAS | 7,03 s | 1,0 |
| SNAKE3.BAS | 7,06 s | 1,0 |
| Forth | 1,59 s | 4,3 |
| maskinkod | 0,35 s | 20 |

Forth tar 21 ms per steg och maskinkoden 4,6 ms. Det mesta av tiden i
maskinkoden går åt till punkterna: ROM:ens DOTADR räknar ut adressen för
varje punkt, och masken ritas om helt i varje steg.

### ORD: ordbehandlaren

ORD ska bli en ordbehandlare skriven i Forth. Det första steget är
textbufferten i `ord.bas` (ORD på skivan). Hela texten ligger på
RAM-skivan, från $0000 (27392 tecken; resten av RAM-skivan upp till
$77FF har KALKYL:s formeltexter), med en lucka vid markören: texten
före markören på 0 till GS och texten efter på GE till 27392.
Att skriva in eller ta bort ett tecken ändrar bara GS eller GE.

| Ord | Stacken | |
|---|---|---|
| `TNY` | | töm texten |
| `INS` | `( c -- )` | skriv in ett tecken vid markören |
| `INSN` | `( adr n -- )` | skriv in n tecken från vanligt RAM |
| `RADERA` | `( n -- )` | ta bort n tecken före markören |
| `STRYK` | `( n -- )` | ta bort n tecken efter markören |
| `HOGER`, `VANSTER` | `( n -- )` | flytta markören n tecken |
| `GA` | `( p -- )` | flytta markören till p |
| `HAMTA` | `( p adr n -- n2 )` | läs högst n tecken från p, utan luckan |
| `POS`, `LANGD`, `LUCKA` | `( -- n )` | markören, textens längd, luckan |

Framåt flyttas texten efter luckan ned med RMOVE. Bakåt måste texten
flyttas upp, och LDIR7F kopierar bara framåt, så VANSTER flyttar 256
tecken åt gången via MB i vanligt RAM, från slutet. Med 20000 tecken
tar `0 GA` från slutet 0,44 s och `20000 GA` från början 0,18 s.

### ORD: skärmen och tangenterna

`RUN` på ORD laddar orden och visar en introduktion med tangenterna.
CTRL-Q går till en meny: fortsätta med texten, ny text, introduktionen,
sluta, spara eller läsa (se nedan). ABC 802:s tangentbord har ingen ESC, så `TKEY` gör om
CTRL-Q (17) till ESC (27); med en PC:s tangentbord går ESC också.
Q sitter längst till vänster bredvid CTRL och går med en hand. Texten
ligger kvar på RAM-skivan när man slutar, men `RUN ORD` börjar med en
tom text, eftersom rad 30 kör `TNY ORD`; från Forth (`Z=CALL(32768,1)`)
tar `ORD` upp den igen. Rad 0-23 visar texten och rad 24 markören, längden och hur mycket som är fritt.
Raderna bryts vid blanksteg, högst 79 tecken; ett långt ord bryts vid
79. 6845:ans markör (R14-R15) visar var markören är. Medan ORD arbetar
är den släckt (R10 = 41), och när ORD väntar på en tangent blinkar den
(R10 = 73).

Tangenterna är som när BASIC läser en rad, och som i VERK: ← (kod 8,
samma som CTRL-H) suddar tecknet före markören, och CTRL-S går bakåt
som i WordStar, där CTRL-E och CTRL-X redan är upp och ned. `TKEY`
gör DEL (en PC:s) till ← och CTRL-D till →, så att `TANGENT` bara ser
8 och 9. ABC 802 har ingen DEL, så förut gick det bara att ta bort
efter markören med CTRL-G på maskinen.

| Tangent | |
|---|---|
| tecken, RETURN | skriv in |
| ^S, → (^D) | ett tecken bakåt, framåt |
| ← (^H, DEL) | ta bort tecknet före markören |
| ^G | ta bort tecknet efter markören |
| ^E, ^X | en rad upp, ned (samma kolumn) |
| ^A, ^F | radens början, slut |
| ^R, ^V | en sida upp, ned |
| ^Q (ESC) | sluta |

`LAYOUT` hämtar sidan från TOP till PB i vanligt RAM med en HAMTA och
räknar ut raderna: början i RS och längden i RL. Att rita hela
skärmen tar omkring 0,16 s, så ORD ritar bara om det som ändrats.
`ANDRAD` minns var texten ändrades, och `OMRITA` ritar från den första
rad som påverkas tills raderna åter börjar där de gjorde förut; resten
flyttas bara i RS. ROM:en har plats för en tangent, så en tangent som
kommer medan skärmen ritas skulle skriva över den förra. Därför läser
`POLL` tangenterna till kön KQ under ritandet, och ORD ritar bara om
när kön är tom. Tangenter som behöver raderna (^E ^X ^A ^F ^R ^V)
räknar om dem först om texten har ändrats. Med 30 ms nedtryckt och
30 ms mellan tangenterna (17 tecken i sekunden) tappas inget, inte
heller när skärmen rullar.

Skärmen har 25 rader i ORD. 6845:ans tabell i ROM:en ($0463 i BASIC
02-11) har R6 = 24 synliga rader med 10 linjer (R9 = 9), 1920 byte av
bildminnets 2 KB, men minnet räcker till 25,6 rader. ORD sätter R6 =
25 (som `OUT 56,6,57,25` i Bit för bit med ABC 800, 4.2). 6845:an
läser bildminnet som en ring: adresserna går runt i de 2 KB, så rad r
ligger på $7800 + (SO + 80r) mod 2048, där SO är bildens början
(R12-R13). Rad 0-23 är texten och rad 24 läget, som `LAGE` skriver i
bufferten SB. För att rulla en rad ändrar ORD bara SO med 80
(`VRUM`) och ritar den nya raden och läget; resten av bilden står
kvar, och att rulla går mycket fortare än att rita om skärmen. När
ORD slutar sätts R6 = 24 och $7800 tillbaka.

### ORD: spara och läsa

5 i menyn sparar texten på skivan och 6 läser in den igen. BASIC
(rad 300-399) frågar efter namnet (förra namnet gäller om man bara
trycker RETURN), gör om det till versaler och till formen `NAMN    EXT`
och anropar `SPARA NAMN    EXT` eller `LADDA NAMN    EXT` i Forth.
Namnet får vara högst 8 tecken, med punkt och högst 3 till; utan punkt
blir det `.TXT`. SPARA och LADDA lämnar en kod till BASIC, som skriver
meddelandet: 0 gick bra, sant (LADDA) att texten inte fick plats, 2 att
filen inte finns och 256 + n diskfel n. (I BASIC II avbryter `MID$` med
två argument raden utan felmeddelande, så rad 374 och 380 har tre.)

Filen är en vanlig textfil, som BASIC skriver med `PRINT #`: block om
253 byte, med ETX ($03) efter det sista tecknet i varje block, radslut
CR och en 0 efter texten. Den går alltså att läsa med `INPUT LINE #` i
BASIC och att kompilera med Pascal (`PC`). `SPARA` lägger alltid till
ett CR sist och `LADDA` tar bort det, så att texten blir densamma efter
varje varv. `LADDA` tömmer texten och läser filen tecken för tecken med
`INS`; 9 och ett tal n är n blanksteg (som BASIC packar indrag), LF och
andra styrtecken hoppas över, och markören hamnar först. Får inte hela
filen plats läses så mycket som ryms, och ORD säger till.

Anropen till DOS:en är desamma som i KALKYL och VERK (`kalkyl.fs`):
`XDOS` anropar en rutin i DOS:en med kanalen i B, och ett diskfel går
via DERRX ($FD33) till `DERRH`, som tar tillbaka Forths stack. `DV` har
DOS-rutinens adress, stackpekaren och den förra DERRX. Kanal 1 har
bufferten på $F600. PREPARE ($6015) och post 0 ($602A) skapar filen,
OPEN ($6018) öppnar den, och $6030 och $602D skriver och läser ett
block. Namnet läses direkt ur raden från BASIC (`0 PARSE DROP 1+`).

Forths ordbok måste sluta under TIB ($B7B0, 47024), som ligger närmast
under BASIC på $B800. Med ORD:s ord slutar den på 46987, så det är 37
byte kvar; därför är orden för filerna så korta, och BASIC sköter namn
och meddelanden. REM FORTH-raderna i `ord.bas` har inga kommentarer,
eftersom BASIC-programmet och variablerna bara har $B800 till MEMTOP
($F186), omkring 14,7 KB, och ord.bas är nu 13770 byte. Orden beskrivs
här i stället.

För att prova orden: CTRL-Q, 4 och `Z=CALL(32768,1)` för Forth med orden
kvar. `TNY T" HEJ VARLDEN" VISA` skriver in texten och visar den,
markören och längden, och `ORD` startar ordbehandlaren igen.

## Programskivan

`gor.sh` gör också `program.img`, programskivan till webbsidan
`abc802.html` (knappen *Program*, eller `?skiva=program`). Den har
FORTH.ABS, ORD (`ord.bas`, sparad av emulatorn med `SAVE DR0:ORD`),
KALKYL och VERK med sina .ABS-filer (och VERK.OVL och VERK.HLP), exemplen
OFFERT.KAL och BUDGET.KAL till KALKYL och brevet OFFERT.VRK till VERK
(`offert.vrk`, som BASIC skriver med `PRINT #1`), och en
BASICINI.SYS ur samma `basicini.asm` med `PROGRAM EQU 1`. Den laddar
Forth som på Forth-skivan men startar inte tolken; skärmen visar

    Programskivan till ABC 802. Forth är laddat.

    RUN ORD     ordbehandlaren
    RUN KALKYL  kalkylprogrammet
    RUN VERK    text och kalkyl

och maskinen står i BASIC. `RUN ORD` kompilerar ORD:s ord (cirka 25 s)
och startar ordbehandlaren. `RUN KALKYL` laddar KALKYL.ABS (se
`KALKYL.md`), som flyttar gränsen mot BASIC till $D800. `RUN VERK`
laddar VERK.ABS på samma sätt (`verk.fs`, se `VERK.md`) med gränsen
$ED00: ORD:s editor med KALKYL:s celler och fält i texten, inläst på
en gång, och VERK.OVL (läsningen och FORMAT) vid starten. Kommandoraden i
BASICINI.SYS måste vara kort: med 241 bytes i filen kom Error 20 vid
starten.

## Orden

Utöver bokens:

- Räkna: `* U/MOD /MOD / MOD ABS +! MAX MIN SGN XOR > 0>`. `/MOD` är
  maskinkod; kvoten avrundas mot noll och resten får täljarens tecken,
  som i BASIC II.
- Stacken: `PICK 2DUP 2DROP DEPTH` (0 PICK = DUP).
- Slingor: `J`, `+LOOP` (slutar när indexet når gränsen, eller går
  under den med negativt steg) och `LEAVE` (slingan slutar vid nästa
  LOOP; resten av varvet körs, som på Jupiter Ace).
- Ordboken: `RECURSE` (ordet som definieras anropar sig självt), `CREATE` (som VARIABLE utan cell, för fält med ALLOT),
  `FORGET namn` (bara egna ord) och kommentaren `( ... )`. `\` finns
  inte: kod 92 är Ö på ABC 802.
- Minne och portar: `CMOVE`, `CMOVE>` (bakåt från slutet, när till
  ligger efter från), `FILL ( adr n c -- )`, `P@ ( port -- b )`,
  `P! ( b port -- )`.
- Söka, i maskinkod med CPIR och CPDR: `SCAN ( adr n c -- adr2 n2 )`
  ger första c och det som är kvar från det (adr + n och 0 om inget c
  finns); `-SCAN ( adr n c -- n2 )` ger antalet byte till och med sista
  c, 0 om inget finns. Till ordbrytning: `adr 81 32 -SCAN`.
- RAM-skivan, de undre 32 KB RAM: `$0000-$77FF` (30 KB) nås med ROM:ens
  LDIR7F på $7FFD. `RMOVE ( från till n -- )` är CMOVE där $0000-$77FF
  är RAM-skivan, åt båda hållen; `RC@ ( adr -- c )` och
  `RC! ( c adr -- )` tar en byte. `100 C@` läser ROM:en, `100 RC@`
  RAM-skivan. MEM: i BASIC använder samma minne.
- Skärmen: `PAGE BEEP AT ( kolumn rad -- ) SPACES`.
- `RND ( n -- 0..n-1 )` och `RANDOMIZE ( n -- )`, med 0 från klockan.
- `INKEY ( -- c | 0 )` läser tangenten utan att vänta, från ROM:ens
  LASTKY ($FFE3) och KEYFLG ($FFE2).

Idéer från Jupiter Ace-ROM:en (bara listan över ord, inget av koden)
som inte är med än: `*/` och `*/MOD` med 32 bitars mellanresultat,
`DOES>` (Ace: DEFINER och COMPILER), `<# # #S #> HOLD SIGN`, `BASE`
med `HEX` och `DECIMAL`, `CALL` för maskinkod, och `LIST`/`EDIT` för
att rätta definierade ord.

Flyttalen, med ROM:ens rutiner för BASIC II och DOUBLE (16 siffror),
på en egen stack om 16 tal à 8 byte:

- `F+ F- F* F/ F< F= FNEGATE FABS FDUP FDROP FSWAP FOVER`.
- `F@ F! ( a -- )`: 8 byte i minnet, t.ex. `VARIABLE X 6 ALLOT`.
- `S>F ( n -- )` och `F>S ( -- n )`, avrundat; för stort ger 32767.
- `>FLOAT ( a n -- f )` läser ett tal som BASIC gör (`1.25E-3`, `.5`);
  f är 0 om texten inte är ett tal. `PARSE-NAME >FLOAT` läser nästa ord.
- `F.` skriver talet som PRINT i BASIC II med DOUBLE: `.3333333333333333`,
  `1.E+16`, `1.25E-15`. `(F.) ( -- a )` ger ROM:ens decimala form.
- `FERR ( -- a )` blir -1 vid spill, division med noll, en tom eller full
  stack; `FDEPTH ( -- n )` och `FCLEAR`.

Orden sätter talens längd ($FF1E) till 8 under anropet och lägger
tillbaka den efteråt, så BASIC räknar som förut (SINGLE om inget annat
sagts).

Grafiken (mosaik, 2 x 3 punkter per ruta, med ROM:ens DOTADR):

- `GR` tömmer skärmen och sätter grafikkoden 151 först på varje rad.
- `PLOT UNPLOT ( x y -- )` och `POINT? ( x y -- f )`: x 0-157 (0-77
  med 40 tecken), y 0-71 uppifrån. Punkter utanför skärmen hoppas över.
- `LINE ( x1 y1 x2 y2 -- )` drar en linje med Bresenhams metod.

Exempel:

    : RUTA 0 0 77 0 LINE 77 0 77 59 LINE 77 59 0 59 LINE 0 59 0 0 LINE ;
    : STJ 20 0 DO 38 30 I 4 * 0 LINE 38 30 I 4 * 59 LINE LOOP ;
    GR RUTA STJ

## Tidsproven

Tidsproven i `../benchmark/` skrivna i Forth och mätta i emulatorn med
maskinens klocka, som metod TA i BASIC (`../benchmark/README.md`).
Sekunder, och avbrott à 1/93,75 s; TA läser dem som hundradelar. BASIC
II:s tider är emulatorns, mätta på samma sätt.

| Prov | BASIC II | Forth | gånger |
|---|---|---|---|
| B1 tom FOR | 0,81 | 0,13 | 6,2 |
| B2 K=K+1, IF | 1,76 | 0,58 | 3,0 |
| B3 K/K*K+K-K | 5,92 | 2,10 | 2,8 |
| B4 K/2*3+4-5 | 6,00 | 1,92 | 3,1 |
| B5 + GOSUB | 6,41 | 2,19 | 2,9 |
| B6 + FOR 1-5 | 11,71 | 2,89 | 4,1 |
| B7 + M(L)=A | 19,85 | 3,89 | 5,1 |
| NOEL1 | 8,00 | 1,89 | 4,2 |
| PRIMES1 | 17,62 | 15,41 | 1,1 |
| SIEVE | 30,75 | 6,58 | 4,7 |

- B1-B7 räknar med flyttal i BASIC och heltal i Forth; NOEL1, PRIMES1
  och SIEVE med heltal i båda. B8 (LOG, SIN) finns inte i Forth.
- Svaren stämmer: NOEL1 -23788 (16 bitar), PRIMES1 slutar på 541,
  SIEVE ger 1027.
- Division är det långsamma. Med `/MOD` som kolonord tog B3 3,32 s,
  B4 3,21 och PRIMES1 24,84 (0,7 gånger BASIC); i maskinkod 2,10, 1,92
  och 15,41. PRIMES1 gör en MOD i varje varv och vinner ändå bara lite:
  divisionen tar 16 varv i båda, och BASIC II:s MOD är också maskinkod.
- BRANCH kollar stackarna i varje varv. Det kostar i alla slingor men
  gör att en slinga som växer stacken stoppas.

## Filerna

- `fk.inc`: kärnan i Z80-assembler (;W-rader för orden).
- `f.fs`: orden i Forth före tolken. `fi.fs`: tolken och kompilatorn.
- `ffc.py`: korskompilatorn. `abs.py` lägger bilden som en .ABS-fil
  i postformat på en MF-skiva, och med en ändelse (BASICINI.SYS) en fil
  med poster om 253 bytes.
- `basicini.asm`: BASICINI.SYS, som startar Forth, se ovan.
- `tidsprov.py`: tidsproven i Forth, se ovan.
- `demo.bas`: demon (DEMO på skivan), se ovan.
- `mask.bas` och `mask.asm`: masken (MASK och MASKMK.ABS på skivan), se
  ovan.
- `ord.bas`: ordbehandlaren ORD (ORD på skivan), se ovan.
- `program.img`: programskivan till webbsidan, se ovan.
- `KALKYL.md`: upplägget för kalkylprogrammet KALKYL; flyttalen
  (milstolpe 1), formelkompilatorn (milstolpe 2), cellerna med
  omräkningen (milstolpe 3), filerna (SPARA och LADDA) och utskriften
  (FORMAT och SKRIV) är klara.
- `VERK.md`: upplägget för VERK, text och kalkyl i ett som i Canon Cat:
  ORD:s editor med fält i texten och formeln i statusfältet, och
  formelkompilatorn som överlägg på RAM-skivan. Överlägget är gjort
  och provat i KALKYL (milstolpe 1), och editorn finns som VERK.ABS
  (milstolpe 2) med fälten (milstolpe 3), statusfältet med CTRL-K
  (milstolpe 4) och filerna NAMN.VRK med utskriften (milstolpe 5);
  på programskivan med en offert som brev (milstolpe 6).
- `offert.vrk`: brevet OFFERT.VRK, en offert med sju celler.
- `kalkyl.fs`: KALKYL så långt det är skrivet, med orden SATT, TABORT,
  VISA, NY, SPARA, LADDA, FORMAT, SKRIV och BYE och 40 celler; exemplen
  `offert.kal` och `budget.kal` blir OFFERT.KAL och BUDGET.KAL. `gor.sh` skriver in det i Forth i
  emulatorn och gör KALKYL.ABS med `kabs.py`.
- `kalkyl.bas`: laddaren KALKYL på programskivan.
- `verk.fs`: VERK, ORD:s ord ur `ord.bas`, fälten (`FALT`), CTRL-K
  (`KALK`, överlägg 3) och menyn
  och introduktionen i Forth; ingång 1 är `VERK`. `gor.sh` lägger
  `kalkyl.fs` till raden "Hit är KALKYL och VERK lika" före och gör
  VERK.ABS med `kabs.py ... VERK`, och överlägg 4 (läsningen och
  FORMAT) blir VERK.OVL; `verk.bas` är laddaren VERK.
- `verk.hlp`: VERK:s introduktion, som `hlp.py` gör till VERK.HLP.
  `gor.sh` bygger allt.
