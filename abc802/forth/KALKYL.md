# KALKYL: upplägget

KALKYL är ett kalkylprogram för ABC 802, skrivet som ORD: ett
BASIC-program med orden i Forth (REM FORTH-rader), på programskivan.
Här står upplägget. Flyttalen (milstolpe 1), formelkompilatorn
(milstolpe 2), cellerna (milstolpe 3) och filerna (milstolpe 5, utan
utskriften) är skrivna; skärmen inte.

## Förebilderna

Luxor sålde Kalkyl 800 (Watchdog Computer Assistance, december 1982,
68 22011-, 1 750–3 000 kr), ett kalkylblad av VisiCalcs slag; se
`bok/KONTORSPROGRAM.md`. I ABC-klubbens programbank (devili.iki.fi,
`pub/Luxor/software/ABC-klubben/abc800/`) finns två program:

- **CALC** (`berakn/`, 1985, ursprungligen för ABC80). Det är ett
  rutnät, AA00 längst upp till vänster med raderna AA–AJ ... och
  kolumnerna 00–15, och storleken bestäms när kalkylen skapas.
  - Varje ruta har en enda funktion av sex typer: medel, upprepa, + - * /
    över en följd av rutor, t.ex. `AC00=AA00 + AA01...AA01 D 2`.
  - Villkor kan inte användas. Sammansatta uttryck kräver hjälprutor.
- **SPRIT** (`sprit/`, 1986, "inspirerat av CALC", som inte räckte).
  - Kalkylen är en lista med namngivna variabler, först indata och sedan
    utdata, i matrisen `A()`.
  - Utdata räknas ut av BASIC-satser, högst 11 rader var, med IF, FOR och
    funktioner. Satserna fogas till ett program, KALKYL.TMP, som kedjas
    in.
  - Utdata räknas i nummerordning. En formel som pekar framåt får förra
    omgångens värde.
  - Upp till fem omgångar skrivs ut sida vid sida, och ändrade värden
    markeras med `##`.
  - Rutnätet (HR-grafik) är frivilligt.

KALKYL tar SPRIT:s idé, namngivna celler med riktiga formler, och gör om
de två svagheterna. Formlerna kompileras direkt i stället för att ett
program kedjas in, och cellerna räknas i beroendeordning.

## Varför celler och inte ett rutnät

- **Skärmen:** ORD visade att en hel omritning tar 0,16 s. En lista med
  en cell per rad behöver bara rita om de rader vars värden ändrats, och
  den rullar bara uppåt och nedåt.
- **Formlerna:** `MOMS = PRIS * 0.25` går att läsa och skriva ut utan
  förklaring. `AC00=AA00*AB00` går inte det.
- **Områden:** det enda som verkligen kräver ett rutnät är summor över
  områden. Det löses med ett intervall i listan: `SUMMA(JAN..DEC)` är
  cellerna från JAN till DEC i listans ordning, som CALC:s `...` fast
  med namn.
- **Ett rutnät senare:** en tabellvy kan bli ett annat sätt att visa
  samma celler.

## Det användaren ser

Skärmen har 80 tecken och en rad per cell. Statusraden är längst ned,
som i ORD.

```
  PRIS          1 250.00   1250
  ANTAL               12   12
  NETTO        15 000.00   PRIS*ANTAL
  MOMS          3 750.00   NETTO*0.25
  SUMMA        18 750.00   NETTO+MOMS
* RUBRIK                   "Offert 1984-03-12"

KALKYL  OFFERT  cell 3 av 6  ändrad
```

- **Kolumnerna:** namnet (högst 8 tecken), värdet högerställt med två
  decimaler som standard, och formeln. En cell med bara ett tal är
  indata. En cell med text är en rubrik och har inget värde.
- **Tangenterna** är desamma som i ORD där det går:
  - pil upp och pil ned flyttar mellan cellerna;
  - RETURN redigerar cellens formel på raden;
  - en ny cell skjuts in efter markören;
  - en cell tas bort, med en fråga om andra celler använder den.
- **Felen:** en formel som inte går att översätta, eller ett namn som
  inte finns, ger ett meddelande på statusraden med platsen i formeln.
  Cellen behåller sin gamla formel.
- **Omräkningen:** efter varje ändring räknas de celler om som beror på
  den, och värdena ritas om. Ett ändrat värde kan markeras, som SPRIT:s
  `##`.

## Formlerna

- Infix med `+ - * / ^`, parenteser och unärt minus.
- Talen skrivs med decimalpunkt, som i BASIC. Visningen kan senare få
  decimalkomma och mellanrum mellan tusentalen.
- Namn på andra celler.
- **Funktioner i första versionen:**
  - `SUMMA MEDEL MIN MAX ANTAL` över ett intervall (`A..B`) eller en
    lista med namn;
  - `AVR(x;n)`, x avrundat till n decimaler;
  - `OM(villkor;a;b)` med `< > = <> <= >=` (det CALC saknade).
- **Kompileringen:** en formel översätts med rekursiv nedstigning till
  ett Forth-ord, som lägger cellens värde på flyttalsstacken och sparar
  det. Ordet för `NETTO*0.25` blir ungefär `NETTO F@ 0.25 F* MOMS F!`.
  - Formeln sparas också som text. Det är texten som skrivs på skivan.
  - När en formel ändras kompileras alla formler om från texten: FORGET
    till en markering, och sedan i beroendeordning. Det blir enkelt och
    tar troligen under en sekund; det ska mätas.
- **Beroendena:** varje formel ger listan över de celler den läser.
  Ordningen sorteras topologiskt när formlerna kompileras, och en cirkel
  (A beror på B som beror på A) är ett fel som anger cellerna.

## Formelkompilatorn (milstolpe 2)

`kalkyl.bas` är än så länge ett BASIC-program som laddar orden och går
till Forths tolk (`Z=CALL(32768,1)`). Där finns fyra ord:

- `SATT namn resten` sätter en cell. Resten är ett tal (som `>FLOAT`
  läser), en rubrik som börjar med `"`, eller en formel. Ett nytt namn
  läggs sist i listan; ett namn som finns får det nya innehållet.
- `RAKNA` räknar formlerna i listans ordning (beroendeordningen är
  milstolpe 3).
- `VISA` listar namn, värde och om cellen är en formel eller en rubrik.
- `NY` tömmer listan och lämnar tillbaka minnet.

```
SATT PRIS 1250
SATT NETTO PRIS*ANTAL
SATT X OM(JAN<FEB;-(2+3)*4;0)
SATT Y PRIS*(ANTAL
FEL: ) saknas vid tecken 12
```

**Översättningen.** Formeln läses med rekursiv nedstigning (UTTRYCK,
TERM, UNAR, POT, PRIM) och översätts direkt till ett huvudlöst
kolonord i ordboken: `205 C, DOCOL ,`, koden, och sist cellens adress
och `F! EXIT`. Ett tal blir `(FLIT)` följt av talets 8 bytes, och ett
namn blir cellens adress och `F@`. Ordets exekveringstoken sparas i
cellen.

- **Funktionerna** är Forth-ord som heter `=SUMMA`, `=MAX` och så vidare
  och kompilerar sin egen kod; ett namn följt av `(` slås upp så. Det
  finns `SUMMA MEDEL MIN MAX ANTAL` (med `A..B` eller en lista med `;`)
  och `OM(a<b;x;y)`, som kompileras med `0BRANCH` och `BRANCH`.
- **Områden** tar cellerna från A till B i listans ordning och hoppar
  över rubrikerna. B före A är ett fel.
- **Beroendena:** varje formel får en bitmängd med de celler den läser
  (8 bytes, 64 celler). Den används i milstolpe 3.
- **Felen** har nummer 1–17 med text (okänt namn, `)` saknas, en rubrik
  har inget värde, fel område, formeln är för djup ...) och platsen är
  tecknet i formeln där felet upptäcktes. Det första felet gäller. Den
  halvfärdiga koden tas bort (`HERE - ALLOT`) och cellen behåller sitt
  gamla innehåll.
- **Djupet:** kompilatorn räknar hur många tal formeln lägger på
  flyttalsstacken och vägrar mer än 16.
- **Räknefel:** en formel som ger spill eller delar med noll (FERR) visas
  som `fel`. Celler som läser den får än så länge dess gamla värde;
  att felet sprids längs beroendena hör till milstolpe 3.

En cell är 28 bytes: namnets längd och 8 tecken, typen (0 tal, 1 formel,
2 rubrik, 3 formel med räknefel), exekveringstoken, värdet (8 bytes) och
bitmängden. Rubrikens text sparas inte än.

Proven i batchläget gav rätt värden för de fyra räknesätten, unärt
minus, parenteser, exponentform (`2.5E1`), negativa tal, områden med en
rubrik i, alla fem funktionerna, OM med `<=` och `<>`, och division med
noll; och rätt fel och plats för bl.a. `PRIS*(ANTAL`, `3*PRISS`,
`OM(1;2;3)`, `MAX()`, `2^3`, `12XY`, `R+1` med en rubrik och
`SUMMA(B..A)`.

**Minnet.** Med gränsen mellan Forth och BASIC på $B000 hade Forth 5444
bytes kvar efter starten, och KALKYL:s ord tog nästan allt: med 40
celler var bara 158 bytes kvar, för lite för en enda formel
(kompilatorn kräver 400 bytes). BASIC:s minne slutar på MEMTOP ($F186),
och ORD:s program gick till $DFD9, så BASIC hade ungefär 4,5 KB över.
Gränsen flyttades därför 2 KB uppåt, till $B800 (`UPP` i `fk.inc` och
`fi.fs`, och BASICINI.SYS:s kopia på $B700). Nu har Forth 7492 bytes
efter starten och 2136 med KALKYL laddat och 40 celler; en formel tar
20–40 bytes. (Med milstolpe 3 räckte det inte; se Cellerna och
omräkningen.) ORD går till $E7D9 och har 2,4 KB kvar till BASIC:s
variabler. Räcker det inte till milstolparna 3–5 kan felmeddelandena
läggas i BASIC, eller cellerna på RAM-skivan som ORD:s text.

## Cellerna och omräkningen (milstolpe 3)

Varje cell har en post om 28 bytes i `CELLER`: namnet, typen, värdet
(8 bytes), adressen till den kompilerade formeln och bitmängden `CDEP`
med cellerna som formeln läser. Typerna är 0 tal, 1 formel, 2 rubrik,
3 räknefel (division med noll, spill, eller en cell den läser är fel)
och 4 trasig formel (den gick inte att kompilera om). Texten som
skrevs efter namnet sparas på RAM-skivan, 80 bytes per cell från 27392
(`TXT`), alltså ovanför det ORD får använda. Plats nummer `CMAX` är en
tillfällig plats för cellen som håller på att sättas.

| Ord | | |
|---|---|---|
| `SATT namn text` | | sätt en cell (tal, formel eller rubrik i citattecken) och räkna om |
| `TABORT namn` | | ta bort en cell som ingen formel använder |
| `VISA` | | lista cellerna: namn, värdet till höger och formeln |
| `FORMAT namn mall` | | mallen som i PRINT USING, till exempel `####.##` (se Utskriften) |
| `SKRIV` | | skriv cellerna på PR:, eller tabellen om det finns en |
| `NY` | | töm kalkylen |
| `NYPOS` | variabel | -1: nya celler läggs sist; annars före cell nummer NYPOS |

**Ordningen.** `ORDNA` gör listan `ORDN` med formlerna i
beroendeordning: i varje varv tas de formler vars lästa celler redan är
klara (`KLAR?`, bitmängden är en delmängd av `KLAR`). Ett tal eller en
rubrik är klar från början. Varven slutar när ett varv inte ger någon
ny; står cellerna redan i ordning räcker ett varv. `RAKNA` går sedan
listan igenom; en formel som läser en cell av typ 3 eller 4 får själv
typ 3 utan att räknas, så att felet sprids till allt som beror på det.
Felcellerna samlas under tiden i bitmängden `FELM`, så att provet är ett
snitt av två bitmängder (`SNITT?`).

**Bara det som ändrats.** `SATT` kompilerar bara den nya formeln; den
gamla koden blir liggande som skräp. `KOMPALLA` kompilerar om alla
formler från texterna på RAM-skivan (koden ligger efter `KBAS`, och
`HERE` sätts tillbaka dit först), men bara när cellerna flyttas
(`TABORT`, eller en ny cell före `NYPOS`), eftersom koden har cellernas
adresser, och när minnet börjar ta slut (`KPLATS`, mindre än 700 bytes
kvar till TIB). Ordningen görs bara om när formeln läser andra celler
än förut: färre beroenden gör aldrig en ordning fel, och en ny cell
läggs sist i `ORDN`. `RAKNA1` räknar från cellens plats i ordningen och
bara de formler som läser en ändrad cell (bitmängden `ANDM`, som växer
under varvet).

Uppmätt i emulatorn med 40 celler i en kedja (C2 = C1*2 och så vidare):

| | |
|---|---|
| `SATT C40 C39+1`, `SATT C39 7` | under 0,1 s |
| `SATT C20 C19+C1` (nya beroenden) | cirka 0,4 s |
| `SATT C1 2` (alla 40 räknas om) | cirka 0,4 s |
| cirkeln `SATT C1 C40`, med vägen genom alla 40 | några sekunder, mest utskriften |
| `TABORT` eller en ny cell före `NYPOS` | över 4 s (allt kompileras om) |

Förut kompilerades allt om efter varje `SATT`, med ordningen och ett
cirkelprov som gick i O(n³): vid 24 celler tog en `SATT` över 20 s.
Små saker räknades också: `CELL` multiplicerar med 28 med `2*`, `BIT`
och i/8 tas ur tabeller, och felcellerna samlas i bitmängden `FELM`.
300 `SATT` i rad på samma cell (mer skräp än minnet rymmer) går igenom
med städningen.

**Cirkelfelet.** Innan en ny formel godtas provar `CIRK?` om den ger
en cirkel: en ny cell kan bara läsa sig själv, eftersom ingen formel
läser den än, och för en gammal cell görs ordningen om; kommer inte alla
celler med finns en cirkel. Bara då söker `CIRKEL?` vägen, bredden
först med en kö, så att varje cell tas en gång; `PAR` har cellen som
läser den, och `CIRKV` skriver vägen, där varje cell används av nästa:

```
SATT X Y+1   SATT Y Z*2   SATT Z X-3
FEL: cirkel: Z Y X Z
```

Cellen behåller då sitt gamla innehåll, med den gamla koden och
bitmängden, och den nya koden tas bort (en ny cell skapas inte). En rubrik som används i en
formel ger fel 6, och en cell som används kan inte göras till rubrik
eller tas bort ("FEL: cellen används i en formel").

**Minnet igen, och KALKYL.ABS.** Med cellerna och omräkningen tog
KALKYL:s ord så mycket att `CMAX` fick vara 16 när källan låg i
REM-rader i `kalkyl.bas`, och BASIC-programmet hade bara 1 KB kvar till
MEMTOP. Nu är KALKYL förkompilerat:

- Källan är `kalkyl.fs`. `gor.sh` startar Forth i emulatorn, sätter
  gränsen mot BASIC till $D800 (talet i `TIB`, så att TIB blir $D7B0),
  skriver in filen och sparar minnet med emulatorns `-M`.
- `kabs.py` tar $8000 till `HERE` ur minnet, ser efter att alla ord i
  `kalkyl.fs` finns i ordlistan och sätter samma gräns i `LADDAD`. Det
  blir KALKYL.ABS på programskivan, 20051 bytes, och överlägg 2 blir
  KALKYL.OVL, 4840 bytes (se Överläggen).
- `kalkyl.bas` är nu bara laddaren: den lägger en liten rutin på $D7B0
  som läser in KALKYL.ABS med DOS:ens `DRUN` och går till Forth.

När KALKYL är laddat har Forth 2397 bytes fritt (4167 före
tabellen, 5331 före omgångarna, 2811 före överläggen, 5160 innan felens texter blev en
tabell, `FTT`) och BASIC $D800 till MEMTOP (cirka 6,5 KB). `CMAX` är 40, så många som formeltexterna på
RAM-skivan får plats för. `BYE` går till BASIC med `NEW` (laddaren kan
vara överskriven), och `Z=CALL(32768,1)` tillbaka till KALKYL med
cellerna kvar. ORD får inte plats bredvid KALKYL; skivan får startas om.
Ord som man själv definierar i Forth efter KALKYL försvinner när
`KOMPALLA` körs, eftersom formlernas kod läggs från `KBAS`.

## Överläggen

Det som bara används när något ändras eller visas ligger i två
överlägg, som delar ett område i minnet:

- överlägg 1 (3490 bytes): formelkompilatorn, `SATT`, `TABORT`,
  cirkelprovet, `ORDNA`, `KOMPALLA` och kompileringen i `LADDA`;
- överlägg 2 (4840 bytes): `VISA` med raderna, `FORMAT`, `SPARA`,
  inläsningen i `LADDA`, `SKRIV` och tabellen (3534 bytes före den).

Det i överlägg 2 som VERK också använder (filnamnet, filerna och
raderna) är märkt med `----`-rader i `kalkyl.fs`, så att `gor.sh` kan
ta det till VERK:s överlägg 2 och 4 (se Minnet i VERK.md).

I den fasta delen finns cellerna, formeltexterna, talen med mallarna
(`NTAL`), omräkningen (`RAKNA`, `RAKNA1`) och `NY`. Båda ligger på RAM-skivan strax
under formeltexterna (under 27392), och området på `OVB` är lika stort
som det större.

- `OVT` har för varje överlägg platsen på RAM-skivan, längden och en
  summa av bytena (24 bytes, eftersom VERK har fyra). `OV n` hämtar överlägget med `RMOVE` om det inte
  redan är där (`OVNU`) och ser efter att summan stämmer (`SUM`, i
  maskinkod). Stämmer den inte, för att RAM-skivan har skrivits över
  (till exempel av ORD), skriver `OVFEL` ett fel och kompilatorn och
  filerna går inte att använda förrän KALKYL laddas igen; omräkningen
  fungerar.
- Orden i ett överlägg har en egen kedja (`OVLST` och `OV2LST`), som
  tas bort ur ordlistan när överlägget är kompilerat. De nås via
  variabler (`'SATT`, `'VISA` osv.), som får sina xt när överlägget
  kompileras; `SATT`, `VISA` osv. i den fasta delen hämtar överlägget
  och kör ordet. `FUNK` letar efter `=SUMMA`
  osv. först i ordlistan och sedan i `OVLST`.
- Ett ord i ett överlägg får inte anropa ett ord som hämtar ett annat
  överlägg. Därför läser `(LADDA)` i överlägg 2 bara in cellerna, och
  `(LADDA2)` i överlägg 1 kompilerar och räknar sedan; `LADDA` i den
  fasta delen anropar båda.
- `gor.sh` kompilerar överlägg 2 först på `OVB` och sparar det på
  RAM-skivan, sedan överlägg 1 på samma plats; det blir kvar i området.
  `kabs.py` tar överlägg 2 från RAM-skivan till filen KALKYL.OVL, som
  laddas på `OVB` och slutar med en RET som är filens startadress.
  Ingång 1 är `OVSTART` (kabs.py sätter den): första gången läggs
  överlägg 1 från området på RAM-skivan, och KALKYL.OVL läses med
  DOS:ens `DRUN` (`KOVL`, som `XDRUN` i VERK) och läggs där också.
  Saknas filen eller stämmer inte summan blir det `FEL: KALKYL.OVL
  kunde inte läsas`. Förut låg en kopia av överlägg 2 i KALKYL.ABS
  efter `KBAS`, men med tabellen fick KALKYL.ABS inte plats under
  $D7B0.
- Att hämta ett överlägg tar några hundradels sekunder (`RMOVE` cirka
  7,5 µs per byte och summan), och det sker bara när man går mellan
  formler och filer.

Vinsten blev 2401 bytes. Med bara kompilatorn i överlägg 1 blev den
868: av de 5416 bytes som mättes för formelkompilatorn är det mesta
tabeller som omräkningen också behöver (`CELLER` 1160 bytes, `MALLAR`
516, `FT` 562 osv.), och själva kompilatorn är 1694 bytes. Hämtningen,
summan och de fasta ingångarna tar cirka 800 bytes.

## Talen

Forth räknar med 16-bitars heltal, och det räcker inte till pengar. Det
finns två vägar:

1. **ROM:ens flyttal (förslaget).** BASIC II har flyttalsrutinerna. I
   ABC 802:s ROM hittar uttryckets hopptabell på $0B18 till dem: FPUSH,
   FTOI, ITOF, STRNUM, FCMP, FARITH och FPOW. Forth får ord som anropar
   dem: `F@ F! F+ F- F* F/ F^ F< F= FNEGATE`, talet från text och text
   från talet.
   - Det visar hur ROM:en räknar, vilket passar bokens ämne.
   - Kalkylen räknar som BASIC.
   - DOUBLE bör ge tillräckligt många siffror för belopp.
2. **Fast komma med 32 bitar i Forth (reserven).** Talen räknas i
   hundradelar, med `D+ D- D*` och `D/` i Forths kärna.
   - Det blir exakt för belopp, men utan `^` med decimaler och utan
     funktioner som SQR.
   - Det är oberoende av ROM:en.

**Det blev väg 1** (2026-11-09). F-orden i kärnan (`fk.inc`, blocket
`(FLT)`) anropar rutinerna direkt i stället för via hopptabellen:

| Rutin | Adress | Vad |
|---|---|---|
| FADD, FSUB | $4F41, $4F39 | addition, subtraktion |
| FMUL, FDIV | $4FEA, $505F | multiplikation, division |
| FCOMP | $4C61 | jämförelse: CY om a<b, Z om lika |
| ITOFS, FTOIR | $4CC0, $4CF3 | heltal till tal och tillbaka, avrundat |
| TXTNUM | $47F3 | text till tal (DE dit, HL texten; använder IX) |
| FTODEC | $5153 | tal till decimal form |

- Talen läggs på Z80-stacken med det översta på den lägre adressen före
  anropet, och resultatet ligger där efteråt; CY betyder spill (och
  division med noll).
- Längden läses från $FF1E. Orden sätter den till 8 (DOUBLE: 16 siffror)
  och lägger tillbaka BASIC:s värde efteråt.
- Formatet: exponenten först (överskott $80, 0 är noll), sedan tecknet i
  bit 7 och mantissan, högsta byte först.
- `F.` (i `f.fs`) avrundar ROM:ens decimala form till 16 siffror och
  skriver som PRINT i BASIC II med DOUBLE. Proven i emulatorn gav samma
  utskrift för bl.a. 1/3, 2/3, 0.1+0.2, 1E16, 1.25E-15 och 1E38*10.

ROM:en har också ASCII-aritmetiken (`ADD$` med flera, med n decimaler),
som räknar exakt i decimal form. Den är långsam men kan vara ett sätt att
kontrollera resultaten.

## Filerna (milstolpe 5)

En kalkyl sparas som en textfil på skivan, en cell per rad: namnet, en
blank och texten som skrevs i `SATT`.

```
PRIS 1250
ANTAL 12
NETTO PRIS*ANTAL
MOMS NETTO*0.25
SUMMA NETTO+MOMS
RUBRIK "Offert 1984-03-12"
%PRIS ######.##
```

Raderna med `%` är mallarna från `FORMAT` (se Utskriften); de skrivs
efter cellerna.

- `SPARA namn` skriver filen och `LADDA namn` läser den. Filen heter
  NAMN.KAL om ingen typ ges (`SPARA BUDGET` ger BUDGET.KAL, `SPARA
  B.TXT` ger B.TXT). Namnet har högst 8 tecken och typen högst 3.
- Formatet är DOS:ens textfil, samma som PRINT# i BASIC: CR efter varje
  rad, $03 (ETX) där ett blocks data slutar och $00 sist. BASIC läser
  den med `OPEN "PROV.KAL" AS FILE 1` och `INPUT LINE #1,A$` (provat),
  och ORD kan rätta den. `LADDA` läser också `$09 n` (n blanka).
- `LADDA` tömmer kalkylen (`NY`) och lägger in raderna direkt, med namn,
  text och typ, utan att kompilera. Sedan körs `KOMPALLA` och `RAKNA` en
  gång. En formel kan därför använda en cell som står längre ned i
  filen, och 40 celler laddas på några sekunder. En formel som når sig
  själv ger `FEL: cirkel:` och blir trasig (typ 4).
- Forth anropar DOS:en direkt, på kanal 1 med bufferten på $F600, i
  stället för via BASIC som planen först var. `DOS ( de b adr -- a f )`
  kör rutinen adr i hopptabellen ($6015 DPREP, $6018 DOPEN, $6021
  DCLOSE, $602A DSETRC, $602D DRDREC, $6030 DWRREC) med C = $FF (vilken
  enhet som helst). Under anropet pekar DOS:ens felutgång DERRX ($FD33)
  på `DERRH`, som tar tillbaka stacken och ger felet; BASIC:s utgång
  läggs tillbaka efteråt. Ett skivfel skrivs som `FEL: skivan, fel n`
  (DOS:ens fel 35–48), och en fil som saknas som `FEL: filen finns
  inte`. Felvägen är provad med en läsning bortom filens slut (fel 38).
- Att skriva över: `SPARA` på en fil som finns behåller filens storlek,
  som PREPARE i BASIC. Var den gamla filen längre ligger resten kvar
  efter $00, men läses inte.

Provat i emulatorn: rubrik, tal och formler fram och tillbaka, 24 celler
på tre block, fel i formler (`C/0` och en cell som läser den), en fil
som saknas och läsningen i BASIC.

## Utskriften

- `FORMAT namn mall` ger cellen en mall som i BASIC II:s PRINT USING:
  `####.##`, `#####`, `#.##` (högst 11 tecken). `FORMAT namn` utan mall
  tar bort den, och `FORMAT mall` utan namn gäller alla celler och de som
  läggs till sedan. En mall utan talfält ger `FEL: mallen har inget
  talfält`.
- Talen formateras med ROM:ens egna rutiner för PRINT USING: `USEFLD`
  ($4713) läser mallen och `FMUSE` ($4575) skriver talet efter den. Det
  går eftersom Forths flyttal har BASIC:s format; `(F.)` ger den
  decimala formen som `FMUSE` vill ha. Får talet inte plats skrivs `%`
  och talet i fri form, som i BASIC (`PRFREE`, $4504). Utan mall skrivs
  talet som med `F.`.
- `VISA` ställer värdena till höger i 16 tecken, så att kolumnerna står
  rakt, och sedan formeln. Raderna byggs i en radbuffert.
- `SKRIV` skriver samma rader utan formlerna på skrivaren, och rubriken
  som text utan namnet. Forth anropar skrivardrivrutinen för PR:
  ($7091) direkt: öppna med standardvärdena (som `OPEN "PR:"` i BASIC),
  skriv varje rad med CR LF och stäng. Ett fel ger `FEL: skrivaren, fel
  n`. Den riktiga drivrutinen ser inte om ett skrivarkort saknas.

Provat i emulatorn: mallar per cell och för alla, för smala mallar (`%`),
felaktiga mallar, fel celler, `SPARA` och `LADDA` med mallarna, läsningen
i BASIC och utskriften till en fil med skrivarkortet (`-P`).

## Omgångarna

Som i SPRIT kan värdena sparas i omgångar och visas sida vid sida:

- `OMGANG` sparar alla cellers värden som nästa omgång, högst fyra
  (`OMAX`). `OMGANG NY` tömmer dem, och `NY` och `LADDA` gör det också.
- `JAMFOR` skriver en rad per cell: namnet, och för varje omgång värdet
  i 16 tecken (`OW`) med cellens mall, och `#` efter ett värde som
  skiljer sig från omgången före. En rubrik ger sin text, ett räknefel
  `fel`.
- En omgång är `ONR` poster om 18 bytes på RAM-skivan från 13568 (720
  bytes per omgång, under överläggen): namnet (längden och 8 tecken),
  typen och värdet. Posten söks på namnet (`OHAMT`), först på samma
  nummer, så celler kan läggas till och tas bort emellan; saknas cellen
  blir fältet tomt.
- Orden ligger efter `( sedan verk.fs till VERK.ABS ---- )` i
  `kalkyl.fs`, i KALKYL:s fasta del, så VERK påverkas inte. De tar 1164
  bytes.
- Omgångarna sparas inte i filen, och `JAMFOR` går inte till skrivaren
  (utskriften ligger i överlägg 2).

Provat i emulatorn: tre omgångar av OFFERT, felen (inga omgångar, fem
omgångar, fel ord efter `OMGANG`), en rubrik, en ny cell som saknas i
de första omgångarna, `fel` för 1/0, och att `OMGANG NY` och `NY` tömmer.


## Tabellen

Listan från `VISA` och `SKRIV` har en rad per cell. En tabell gör en
snyggare uppställning: raderna är text där namnen på cellerna står, och
`TABELL` och `SKRIV` byter namnen mot värdena.

    TRAD OFFERT
    TRAD Pris per styck          PRIS
    TRAD Antal                  ANTAL
    TRAD Att betala             SUMMA

- Ett ord (tecknen mellan blanktecken) som är namnet på en cell blir
  cellens värde med dess mall, högerställt så att det slutar där namnet
  slutar. Ett längre värde går åt vänster. En rubrik blir tom, ett
  räknefel `fel`. Annat är text och står kvar i sin kolumn; `"` först
  gör ett ord till text (citattecknet tas bort).
- `TRAD text` lägger till en rad, högst 79 tecken och 40 rader (`TMAX`).
  `n TBYT text` byter rad n, `n TTA` tar bort den, `TLISTA` visar
  raderna med nummer och `TNY` tömmer tabellen. `NY` och `LADDA`
  tömmer den också.
- `SKRIV` skriver tabellen på PR: när det finns en, annars listan.
- `SPARA` skriver raderna efter cellerna och mallarna, med `%=` först,
  och `LADDA` läser dem. I `gor.sh` blir `%=` i exemplen `TRAD`, så
  OFFERT.KAL har en tabell.
- Raderna ligger på RAM-skivan från 8192, 80 bytes var med längden
  först (`TADR`), och antalet på 8191 (`NTAB@`), så att de finns kvar
  när överlägget byts. `OVSTART` tömmer tabellen första gången.
- Orden ligger i överlägg 2 mellan `---- Tabellen: bara i KALKYL` och
  `---- Tabellen slut`, som `gor.sh` tar bort för VERK, så VERK.ABS är
  oförändrat. Där finns också nya `(SPARA)`, `(LADDA)` och `(SKRIV)`.
  Raden byggs i `LB` med `U+` och `US`, som raderna i `VISA`. I den fasta
  delen slår `OV2'` upp xt för `(TABELL)` osv. i överläggets kedja när
  KALKYL byggs, och `TABELL`, `TRAD` osv. hämtar överlägg 2 och kör dem.
- Överlägg 2 växte med 1306 bytes, och området med det. Överlägg 1
  hamnade då under 16384, så omgångarna flyttades till 13568.

Provat i emulatorn: OFFERT.KAL med tabellen, `TABELL` utan tabell,
`TBYT`, `TTA` med och utan rätt nummer, `SKRIV` till en fil (`-P`),
`SPARA` och `LADDA` med `%=`, och `VISA`, `SKRIV`, `OMGANG` och `JAMFOR`
utan tabell.

## Hur programmet delas

Delningen följer ORD:
- **BASIC** (`kalkyl.bas`) laddar programmet. Filerna och utskriften
  gör Forth själv med DOS:ens och ROM:ens rutiner (se Filerna och
  Utskriften), och talen formateras med PRINT USING:s rutiner.
- **Forth** gör skärmen, tangenterna, formelkompilatorn, beroendena och
  omräkningen.

Cellerna (namn, formeltext och värde) ligger i en tabell i Forths minne.
Blir den för stor kan de flyttas till RAM-skivan ($0000-$77FF, `RMOVE`),
som ORD:s text.

## Milstolparna

1. **Flyttal i Forth (klart 2026-11-09).** F-orden i kärnan med ROM:ens
   rutiner och DOUBLE, `F.` som BASIC II:s PRINT; provade mot BASIC II
   i emulatorn (se Talen). `F^` saknas än.
2. **Formelkompilatorn (klar 2026-11-09).** Infix till Forth,
   namnuppslag, funktionerna och fel med platsen; provad i batchläget
   (se Formelkompilatorn). AVR väntar, som `F^`, på att ROM:ens
   rutiner kan anropas utan BASIC:s felhantering.
3. **Cellerna och omräkningen.** Tabellen, beroendeordningen och
   cirkelfelet. Klart, med 40 celler i KALKYL.ABS (se Cellerna och
   omräkningen).
4. **Skärmen och tangenterna**, med ORD:s rutiner för markören och
   omritningen.
5. **Spara, ladda och skriva ut (klart 2026-11-09).** `SPARA`, `LADDA`,
   `FORMAT` och `SKRIV` (se Filerna och Utskriften).
6. **Programskivan.**
   - KALKYL på `program.img`, och `RUN KALKYL` i BASICINI.SYS:s text
     (klart, KALKYL.ABS och laddaren).
   - Exempel: en offert med moms (OFFERT.KAL) och en hushållsbudget med
     `SUMMA(JAN..DEC)` (BUDGET.KAL) finns på skivan; ett annuitetslån
     med `^` väntar på `F^`.
7. **Senare:**
   - tabellvyn;
   - fler funktioner;
   - ett kapitel i boken om KALKYL.

## Att ta reda på

- **Potenser:** `F^` (BASIC:s `^`) och kanske SQR, med ROM:ens FPOW.
- **Minnet:** löst med KALKYL.ABS. Forth har 6 KB och BASIC 6,5 KB
  när KALKYL är laddat, vilket bör räcka till skärmen (milstolpe 4) och
  filerna (milstolpe 5).
- **Namnen:** om åäö ska vara tillåtna i namnen. De är `{ | }` i ABC
  802:s teckenkod och kan inte stå i namn i BASIC, men i Forth går det.
