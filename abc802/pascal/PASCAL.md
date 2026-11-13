# Pascal med p-kod på ABC 802: upplägget

En liten Pascal-maskin för ABC 802 i BASIC-läget. Den har två delar:

- en kompilator för en delmängd av Pascal, skriven i delmängden själv;
- en tolk för p-kod i Z80-assembler, som ligger i RAM ovanför $8000 och
  använder ROM:ens rutiner för utskrift och filer, och FPP ur BBC BASIC
  för flyttalen (8.4).

Kompilatorn är ett p-kodprogram som tolken kör, så maskinen kompilerar
och kör Pascal själv. Det började som en plan, och avsnitt 7 säger vad
som är gjort. Det som är osäkert är märkt (?).

Detta är den **lilla** versionen. En **stor** version till en annan bok
ska senare ta i stort sett hela maskinen, under CP/M med omkring 56 KB.
Den använder samma p-kodformat och samma tolkkärna, och bara I/O-lagret
under kärnan är olika (avsnitt 6). Det som bestäms här ska alltså hålla
för båda.

Förebilder från tiden:
- **Tiny Pascal** (Chung och Yuen, BYTE 1978): en delmängd med en
  kompilator skriven i sig själv till p-kod, på 8080-maskiner med
  16–32 KB.
- **Pascal-S** (Wirth): kompilatorn och tolken i ett program, för en
  delmängd tänkt för undervisning, med en enkel stackmaskin.
- **UCSD Pascal**: p-kod som bytekod, och program delade i segment som
  laddas vid behov. Därifrån kommer kodningen och segmenten, inte
  instruktionerna i detalj.

Provskriptet `prov.sh` och `z80kor.c`, som nämns nedan, ligger inte här;
`sh bygg.sh` bygger tolken, kompilatorn och skivan.

## 1. Förutsättningarna på ABC 802

Ur `../forth/README.md` och `../forth/KALKYL.md`:

- ROM:en fyller $0000–$7FFF (BASIC II, DOS:en, PR:, V24:, CAS:). Det
  finns ingen ledig ROM-plats, så tolken ligger i RAM och anropar ROM:en
  därifrån.
- De nedre 32 KB RAM (RAM-skivan, MEM:) nås bara med LDIR7F eller LRS*.
  De duger för lagring men inte för kod som anropar ROM:en. `RC@` i
  Forth tar cirka 54 µs per byte.
- Med KALKYL uppmättes det att Forth kan ha $8000 till omkring $ED00,
  cirka 27,9 KB. BASIC:s SP vid CALL ligger på cirka $EE94. Pascal-tolken
  laddas på samma sätt som Forth och har samma plats: en `.ABS`-fil som
  DOS:ens DRUN laddar till $8000. BASIC flyttas upp, och sedan startar
  `Z=CALL(32768)` tolken. BASICINI.SYS kan göra det vid starten, som på
  Forth-skivan.
- DOS:en anropas direkt genom hopptabellen: DPREP $6015, DOPEN $6018,
  DCLOSE $6021, DSETRC $602A, DRDREC $602D, DWRREC $6030. Forth använder
  kanal 1 med bufferten på $F600, och DOS:ens sektorbuffert ligger på
  $F500. Felutgången DERRX ($FD33) pekas om under anropet.
- ROM:ens flyttal anropas direkt:

  | Rutin | Adress |
  |---|---|
  | FADD, FSUB | $4F41, $4F39 |
  | FMUL, FDIV | $4FEA, $505F |
  | FCOMP | $4C61 |
  | ITOFS, FTOIR | $4CC0, $4CF3 |
  | TXTNUM | $47F3 |
  | FTODEC | $5153 |

  Formatet: exponenten först (överskott $80, 0 är noll), sedan tecknet i
  bit 7 och mantissan, högsta byte först. Längden läses från $FF1E; 8
  byte är DOUBLE med 16 siffror. Forth-kärnan med flyttalen är 6 764 byte.
- Teckensatsen: `{ } [ ] \ |` visas som `ä å Ä Å Ö ö`. Det påverkar
  kommentarerna och fältindexen (avsnitt 2.3).

## 2. Delmängden

### 2.1 Med i den första versionen

- Typer: INTEGER (16 bitar med tecken), CHAR, BOOLEAN, REAL och
  endimensionella fält (`ARRAY [l..h] OF T`) av dem, med konstanta
  gränser. Fält av fält går inte.
- REAL har 40 bitar och cirka 9 siffror, med FPP ur BBC BASIC (8.4). Ett
  heltal blir REAL där ett REAL väntas, och `/` ger alltid REAL. Åt
  andra hållet går det bara med TRUNC och ROUND.
- Strängar är `PACKED ARRAY [1..n] OF CHAR`. De kan tilldelas och jämföras
  hela, med alla sex jämförelserna, och en strängkonstant med samma
  längd kan tilldelas eller jämföras med dem. De skrivs med WRITE, och
  tecknen nås ett och ett med index.
- Deklarationer: CONST, TYPE (bara namn på fälttyper), VAR, PROCEDURE och
  FUNCTION, nästlade.
- Parametrar: värdeparametrar och VAR-parametrar. Fält skickas helst som
  VAR, eftersom en värdeparameter kopierar fältet.
- Satser: tilldelning, anrop, sammansatt sats, IF, WHILE, REPEAT, FOR
  (TO och DOWNTO) och CASE.
- Standardprocedurer och -funktioner: WRITE, WRITELN, READ, READLN, ABS,
  SQR, ODD, ORD, CHR, SUCC, PRED, EOF och EOLN, och för filerna RESET,
  REWRITE och CLOSE (avsnitt 2.4). HALT avslutar programmet, som i
  UCSD och Turbo Pascal. För REAL finns dessutom SQRT, SIN, COS, ARCTAN,
  LN, EXP, TRUNC och ROUND. ABS och SQR ger samma typ som argumentet.
- WRITE av ett REAL: `X :8:3` med tre decimaler i bredden 8, och utan
  `:d` som i BBC BASIC, med upp till 9 siffror. READ läser ett REAL som
  `12.5E-1`.
- Operatorer: `+ - * / DIV MOD`, `= <> < <= > >=`, AND, OR och NOT.

### 2.2 Senare och inte alls

- Inte i den lilla versionen: RECORD och WITH, fält av fält, fält som
  värdeparameter (fel 21), pekare och NEW, mängder (SET), uppräknade
  typer och delomfång. De finns i den stora versionen, PVM.COM under
  CP/M (avsnitt 9). REAL och strängarna kom på samma sätt in i p-koden
  utan att tolken gjordes om (steg 7 i 7).
- Inte alls än: GOTO och etiketter, procedurer som parametrar och
  variantposter.

### 2.3 Tecknen

- Kommentarer skrivs `(* ... *)`. Med `{ }` skulle ett ä i en svensk
  kommentar, som är byte $7B, börja en kommentar. Kompilatorn godtar bara
  `(* *)`.
- Fältindex kan skrivas både `[ ]` och `(. .)`, som standarden tillåter.
  Byte $5B och $5D är `[` och `]` för kompilatorn, men på ABC 802:s skärm
  syns `A[I]` som `AÄIÅ`. Exempel som ska skrivas och läsas på maskinen
  använder därför `(. .)`. I boken trycks `[ ]`.
- Namn består av A–Z, a–z och 0–9; stora och små bokstäver är samma.
  Byte $5B–$5D och $7B–$7E, som visas som svenska bokstäver, får bara stå i
  strängar och kommentarer. Ett namn som TALÅR, med Å, går alltså inte.
- Strängar skrivs `'...'`, och `''` är en apostrof i strängen. Svenska
  bokstäver i strängar är bara bytes och skrivs som de visas.

### 2.4 Filer

Bara textfiler. Standardfilerna INPUT och OUTPUT är tangentbordet och
skärmen. Utöver dem får ett program deklarera variabler av typen `TEXT`:

    VAR F : TEXT;
    ...
    RESET(F, 'HEJ.PAS');   (* läsa *)
    REWRITE(F, 'HEJ.PK');  (* skriva *)
    READ(F, C); WRITE(F, C); CLOSE(F)

Filnamnet som andra parameter kommer från UCSD och Turbo Pascal, inte
från standarden. Det är en sträng eller ett `ARRAY OF CHAR` eller
`PACKED ARRAY OF CHAR`, där namnet slutar vid det första blanksteget; så får kompilatorn namnet på källan
från tangentbordet. Två filer får vara öppna samtidigt, och kompilatorn
behöver just två: källan och p-koden. I `pvm.asm` är de DOS:ens
kanaler 1 och 2 med buffertarna $F600 och $F700, och
självkompileringen på ABC 802 har dem öppna samtidigt (4.7).

### 2.5 Grammatiken

EBNF: `{ }` betyder noll eller flera gånger, `[ ]` noll eller en gång,
`|` ett val, och ord inom citattecken är ordagranna.

```
program      = "PROGRAM" namn [ "(" namn { "," namn } ")" ] ";" block "." .
block        = [ "CONST" kdef ";" { kdef ";" } ]
               [ "TYPE"  tdef ";" { tdef ";" } ]
               [ "VAR"   vdekl ";" { vdekl ";" } ]
               { ( prochuvud | funkhuvud ) ";" ( block | "FORWARD" ) ";" }
               sammansatt .
kdef         = namn "=" konstant .
konstant     = [ "+" | "-" ] ( tal | reellt | namn ) | tecken | sträng .
tdef         = namn "=" typ .
vdekl        = namn { "," namn } ":" typ .
typ          = namn | [ "PACKED" ] "ARRAY" vä konstant ".." konstant hö "OF" namn .
prochuvud    = "PROCEDURE" namn [ parametrar ] .
funkhuvud    = "FUNCTION" namn [ parametrar ] ":" namn .
parametrar   = "(" pgrupp { ";" pgrupp } ")" .
pgrupp       = [ "VAR" ] namn { "," namn } ":" namn .

sammansatt   = "BEGIN" sats { ";" sats } "END" .
sats         = [ tilldelning | anrop | sammansatt | omsats | medansats
               | upprepa | försats | fallsats ] .
tilldelning  = variabel ":=" uttryck .
anrop        = namn [ "(" arg { "," arg } ")" ] .
arg          = uttryck [ ":" uttryck ] .   (* fältbredd bara i WRITE *)
omsats       = "IF" uttryck "THEN" sats [ "ELSE" sats ] .
medansats    = "WHILE" uttryck "DO" sats .
upprepa      = "REPEAT" sats { ";" sats } "UNTIL" uttryck .
försats      = "FOR" namn ":=" uttryck ( "TO" | "DOWNTO" ) uttryck "DO" sats .
fallsats     = "CASE" uttryck "OF" fall { ";" fall } [ ";" ] "END" .
fall         = konstant { "," konstant } ":" sats .

uttryck      = enkelt [ relation enkelt ] .
relation     = "=" | "<>" | "<" | "<=" | ">" | ">=" .
enkelt       = [ "+" | "-" ] term { ( "+" | "-" | "OR" ) term } .
term         = faktor { ( "*" | "/" | "DIV" | "MOD" | "AND" ) faktor } .
faktor       = tal | reellt | tecken | sträng | variabel | funktionsanrop
             | "(" uttryck ")" | "NOT" faktor .
variabel     = namn [ vä uttryck hö ] .
funktionsanrop = namn [ "(" uttryck { "," uttryck } ")" ] .
vä           = "[" | "(." .
hö           = "]" | ".)" .
reellt       = siffror "." siffror [ exponent ] | siffror exponent .
exponent     = "E" [ "+" | "-" ] siffror .
```

`tecken` och `sträng` skiljs på längden: `'A'` är ett tecken, och en
sträng har annan längd. En sträng får bara stå i WRITE, som filnamn och
som konstant till eller jämförd med ett `PACKED ARRAY OF CHAR` med samma
längd; med en annan längd är det fel 8. `PACKED` får bara stå före
`ARRAY ... OF CHAR` (fel 20). Ett reellt tal får ha högst 32 tecken
(fel 27), och `1.` utan siffra efter punkten är inget reellt tal, så att
`1..5` fortfarande är ett omfång.

## 3. Kompilatorn

### 3.1 Ett pass

- **Rekursiv nedstigning:** en procedur per regel i grammatiken, som i
  Pascal-S. Läsaren (lexikal analys) ger en symbol i taget och läser
  källan ett tecken i taget från filen.
- **Koden skrivs direkt,** utan träd. Ett hopp framåt (IF, WHILE, CASE)
  skrivs med ett tomt mål. Adressen läggs på kompilatorns egen stack och
  fylls i när målet är känt. Eftersom koden går till en fil fylls hoppen
  i en buffert för den procedur som kompileras just nu: en procedurs
  kod hålls i minnet tills procedurens END och skrivs sedan ut. Det
  begränsar hur stor en procedur får bli (avsnitt 5).
- **Symboltabellen** är fält i kompilatorns globala variabler:
  - namnen, där alla tecken räknas (högst 32);
  - slag (konstant, typ, variabel, procedur, funktion, parameter);
  - typ;
  - nivå;
  - adress eller procedurnummer.

  Den är en stack: när en procedur är klar tas dess namn bort. Varje
  namn ligger först i en kedja efter sin hashsumma, så det närmaste
  namnet vinner (3.4).
- **Felen:** det första felet avbryter, med radnummer, kolumn och ett
  felnummer. `pc.pas` skriver bara numret; texterna står i `pc.py` och
  kan senare ligga i en egen fil, som MTERRS.TXT i Pascal/MT+. Ingen
  återhämtning; det sparar plats.

### 3.2 Igångsättningen och provet

1. **På värddatorn:** `pascal/pc.py` är samma kompilator i Python och ger
   samma p-kod. `pascal/pvm.py` är en p-kodtolk i Python, för att pröva
   p-koden utan emulatorn.
2. **Kompilatorn i Pascal:** `pascal/pc.pas` är skriven i delmängden.
   `pc.py` kompilerar den till `PC.PK`.
3. **Kompilatorn kompilerar sig själv:** `PC.PK` i `pvm.py` läser
   namnet `PC.PAS` från tangentbordet och skriver `PC.PK` på nytt, och
   den ska vara byte för byte lika med den från `pc.py`. Det är den
   (3.4). Därefter
   kan `pc.py` i princip tas bort; det räcker att `PC.PK` finns, som
   kompilatorns binära frö (?). Om fröet ska ligga i förrådet eller
   alltid byggas med `pc.py` får bestämmas.
4. **På ABC 802 i emulatorn:** tolken kör `PC.PK` på `pc.pas` från
   skivan, och resultatet jämförs med `PC.PK`. Det är provet i `make`,
   i batchläget som `cpmprov`. Det prövar på en gång tolken, filerna och
   kompilatorn.
5. Små exempel (HEJ, PRIMTAL, sortering och rekursion) körs i båda
   tolkarna och ger samma utskrift.

### 3.3 pc.py

`pc.py` är gjord och kompilerar alla proven (4.6). Det som bestämdes
när den skrevs, och som `pc.pas` ska göra likadant för att ge samma
bytes:

- **FORWARD** finns, eftersom kompilatorn själv behöver det: `faktor`
  anropar `uttryck`, som kommer före i en rekursiv nedstigning. Den
  riktiga deklarationen skrivs utan parametrar (`FUNCTION UDDA;`), som
  i standarden.
- **Procedurerna** numreras i den ordning deras huvuden kommer, och
  huvudprogrammet är 0. Koden skrivs i den ordning procedurerna blir
  klara, alltså de innersta först och huvudprogrammet sist. Eftersom
  de nästlade procedurerna står före `BEGIN` har en procedur ingen kod
  förrän de är klara, så det räcker med en buffert i taget.
- **`ENT b`** skrivs först med 0 och fylls i vid END, när antalet ord är
  känt; FOR lägger till ett gömt ord för gränsen per nivå av FOR i
  varandra.
- **FOR** räknar gränsen en gång, och slingan prövar `I <= gräns` före
  varje varv och räknar upp efter. Med gränsen 32767 tar den inte slut
  (?).
- **Hoppen:** framåt den långa formen (`UJP`, `FJP`) med målet ifyllt
  efteråt; bakåt den korta när den räcker.
- **CASE:** uttrycket, ett hopp till tabellen, fallen med ett hopp till
  slutet efter vart och ett, och sist `XJP` med tabellen och `ERR 4`.
  Ett värde utan fall är körfel 4, som i standarden. Omfånget får vara
  högst 256.
- **Fält:** ett ord per element, också för CHAR. Indexet prövas med
  `CHK`, och den undre gränsen dras av före `IXA 2`. Ett fält kan
  tilldelas helt (`B := A`, med `MOV`).
- **Konstanter** laddas med den kortaste formen; uttrycken räknas inte
  ut i förväg, så `-7` blir `LDC7 NEG`.
- **AND och OR** räknar båda sidorna, som i standarden.
- **Felen** har rad, kolumn och nummer. Kolumnen är symbolens; för ett
  typfel symbolen före.
- **Kommer senare:** fält som värdeparameter (fel 21) och
  funktionsvärdet satt från en procedur inuti funktionen (fel 22).
- **REAL** är tre ord. En reell konstant står som text i koden (`LDCR`),
  och FPP gör om den när den körs, så att kompilatorn inte behöver räkna
  med flyttal; en konstant med minus får minuset i texten. Ett heltal
  görs om med `FLT` överst och `FLO` under det översta. En lokal, global
  eller parameter av typen REAL nås genom sin adress med `INDR` och
  `STOR`, och ett fält av REAL har `IXA 6`. Ett funktionsvärde av typen
  REAL har tre ord, och anroparen lägger tre `LDC0`.
- **Strängarna:** ett `PACKED ARRAY OF CHAR` har två tecken per ord, och
  ett tecken nås med `IXA 1` och `INDB` eller `STB`. Tilldelningen är
  `MOV n` med n tecken, och jämförelsen `CPS n`. En strängkonstant
  läggs med `LSA` och kopieras eller jämförs på samma sätt.
- **P-kodfilen** har bit 15 i antalet procedurer satt när programmet
  använder REAL (4.5).
- **P-kodfilen** har procedurtabellen, de globalas storlek och antalet
  procedurer sist, efter koden (4.5), så att `pc.pas` kan skriva varje
  procedur när den är klar.
- **HALT** avslutar programmet (`HLT`); kompilatorn använder det efter
  ett fel.
- **Gränserna:** en sträng har högst 80 tecken (fel 24) och ett namn
  högst 32 (fel 25).

### 3.4 pc.pas

`pc.pas` är gjord, 1 595 rader med 77 procedurer och funktioner. Den gör
samma sak som `pc.py`, i samma ordning, och ger samma bytes för sig
själv och för alla proven, också samma rad, kolumn och nummer för
felen. `prov.sh` prövar det (4.6). Det som bestämdes när den skrevs:

- **Namnen** ligger packade i ett fält `POOL`, två tecken per ord
  (7 bitar vardera) efter ett ord med längden. Ett namn som bara söks
  läggs överst i `POOL` och tas bort igen, så bara deklarerade namn
  stannar.
- **Symboltabellen** är fyra fält: namnet (platsen i `POOL`), slag, typ
  och nivå i ett ord (slaget + 32 · typen + 2048 · nivån), värdet
  (adress, konstant eller procedurnummer) och länken. Länken går till
  nästa namn med samma hashsumma (summan av tecknen modulo 61); 61
  kedjehuvuden pekar på det senaste namnet i varje kedja. Ett namn
  söks bara i sin kedja, och när en procedur är klar kopplas dess namn
  ur kedjorna. De reserverade orden och de fördefinierade namnen ligger
  i samma tabell, med egna slag.
- **Symbolerna** är tecken: de reserverade orden `a`–`|` i ordning (WHILE
  är `{` och PACKED `|`), namn, tal, tecken och strängar `1`–`4`, filens
  slut `5`, `:=`, `<=`, `>=` och `<>` `6`–`9`, `..` `!` och ett reellt
  tal `#`. Tecknen `+ - * / ( ) [ ] , ; : . = < >` är sina egna
  symboler.
- **Ett reellt tal** sparas som text i `POOL`, som ett namn, och skrivs
  med `LDCR`. En reell konstant i CONST har platsen i `POOL` som värde,
  med minus om konstanten har minus.
- **De fördefinierade namnen** står i `ORDEN` som sex strängar om 64
  tecken med namnen skilda av blanksteg, och `DEFS` lägger in dem med
  slag, typ och värde. Det är mindre kod än ett anrop per namn.
- **FORWARD:** parametrarna stannar i tabellen och i `POOL` men kopplas
  ur kedjorna och märks döda (slaget + 10). Vid den riktiga
  deklarationen kopieras de överst i tabellen och kopplas in igen.
- **Koden** för en procedur ligger i `KOD`, två bytes per ord (den
  höga minus 128, så att ordet ryms med tecken), och skrivs till filen
  vid procedurens END. Procedurtabellen hålls i minnet och skrivs sist.
- **Utan kortslutning** i AND och OR måste ett index skyddas med IF i
  varandra, och tabellerna börjar på 0 med en tom post där.
- **Gränserna** är konstanter i början av `pc.pas` och ger fel 26 när
  de nås: 1 536 bytes kod i en procedur, 370 namn, 1 300 ord för namnen,
  90 procedurer, 90 parametrar, 30 fälttyper, 64 fall i CASE i
  varandra, 8 FOR i varandra, 16 namn i en VAR-grupp och 15 nivåer.
  Självkompileringen använder 352 namn, 1 261 ord för namnen, 78
  procedurer, 77 parametrar, 22 fälttyper och 1 328 bytes i den största
  proceduren.
- **Strängarna i kompilatorn** är packade: källans och p-kodens namn,
  strängen och namnet som läses, och `ORDEN`:s ord som lokal variabel.
  Annars rymdes inte REAL och strängarna (5.2).

## 4. P-koden

### 4.1 Maskinen

- En stackmaskin med 16-bitars ord. Uttrycken räknas på stacken.
- **Registren**, i Z80:ans register eller på fasta adresser i tolken:
  - PC: nästa instruktion;
  - SP: stackens topp, Z80:ans egen SP;
  - MP: den aktuella aktiveringsposten;
  - NP: gränsen för stacken.
- **Stacken:** p-maskinens stack är Z80:ans stack. Den växer nedåt från
  toppen av det fria minnet. BASIC:s SP sparas vid CALL och läggs
  tillbaka vid slutet. Standardprocedurerna går på BASIC:s stack (4.7).
- **Byteordningen** är Z80:ans, lägsta byte först, både i koden och i
  minnet.
- **Sant och falskt:** BOOLEAN är 0 eller 1 i ett ord. CHAR är ett ord
  med den höga byten 0.
- **REAL** är tre ord (6 byte) på stacken och i minnet, bytes
  `L' H' L H C 0`: FPP:s fem byte HLH'L'C (8.4) och en byte utfyllnad.
  Mantissan har tecknet i H:s högsta bit, och C är exponenten.
- **Strängarna** (`PACKED ARRAY OF CHAR`) har två tecken per ord, det
  första i den låga byten.

### 4.2 Aktiveringsposten

Bestämd i `pvm.py`. Anroparen lägger, i den här ordningen:

1. en plats för funktionsvärdet, om det är en funktion (`LDC0`);
2. märket, `MST n`: den statiska länken (posten n nivåer ut från
   anroparen; 0 när den anropade ligger direkt i anroparen, 1 för en
   syskonprocedur eller rekursion), den dynamiska länken (MP) och en
   plats för återhoppet;
3. argumenten, ett ord vardera (en VAR-parameter är en adress);
4. `CUP k p`, där k är argumentens bytes. Den sätter MP = SP + k,
   lägger återhoppet på MP och hoppar till procedur p.

```
MP+6     funktionsvärdet (LFA ger adressen)
MP+4     statisk länk    (posten för den omgivande proceduren)
MP+2     dynamisk länk   (anroparens MP)
MP   ->  återhoppet      (anroparens PC; 0 för huvudprogrammet)
MP-2     ord 0           argumenten först, sedan de lokala
MP-4     ord 1
...
```

- Ord i ligger på MP - 2(i+1), argument och lokala i en följd. `ENT b`
  lägger b nollställda ord efter argumenten och prövar att stacken inte
  går ner i de globala (körfel 3).
- `RET` sätter SP = MP + 6, MP = den dynamiska länken och PC =
  återhoppet. Argumenten och märket försvinner, och funktionsvärdet
  ligger kvar överst. Ett återhopp 0 avslutar programmet.
- Globala variabler ligger efter koden, på GB + 2w, och nås med
  `LDG`/`STG`/`LGA`.
- `LOD n b`, `STR n b`, `LDA n b` följer den statiska länken n steg. I
  verkliga program är det sällan mer än ett eller två steg, så det blir
  en kedja och inte en display som i Pascal-S. Det är mindre kod i
  tolken, och en display måste sparas och tas tillbaka vid varje anrop.

### 4.3 Kodningen

- Varje instruktion börjar med en opkodbyte, och tolken hoppar genom en
  tabell med 2 byte per opkod.
- **Korta former** (som i UCSD): de vanligaste fallen har operanden
  inbyggd i opkoden.
  - `LDCn`: konstanterna 0–15.
  - `LDLn`, `STLn`: de första åtta lokala orden.
  - `LDGn`: de första åtta globala orden.

  De gör koden mindre, och den lilla versionen lever på det.
- **Operanderna:** B är en byte (0–255) och W ett ord. Opkoderna $80–$FF
  med en byteoperand B kan vara "stora" med ett W i stället. Det
  bestäms när tabellen skrivs.
- **Hoppen** är relativa med en byte med tecken (`UJPs`, `FJPs`) eller
  ett ord (`UJP`, `FJP`). Kompilatorn väljer den långa formen när målet
  är okänt, för ett hopp framåt fylls i efteråt. Den korta formen
  används bakåt, där målet är känt.

### 4.4 Instruktionerna

Tabellen i `pkod.py` är den som gäller; `pasm.py`, `pvm.py` och
kompilatorn läser den. Byte = instruktionens storlek. Stacken skrivs
före -- efter.

| Grupp | Instruktion | Opkod | Byte | Vad |
|---|---|---|---|---|
| Konstanter | `LDC0`–`LDC15` | $00–$0F | 1 | konstant 0–15 |
| | `LDCN` | $10 | 1 | -1 |
| | `LDCB b` | $11 | 2 | konstant 0–255 |
| | `LDCW w` | $12 | 3 | konstant -32768..32767 |
| | `LSA n bytes` | $13 | 2+n | strängens adress (strängen ligger i koden) |
| Lokala | `LDL0`–`LDL7` | $18–$1F | 1 | lokalt ord 0–7 |
| | `STL0`–`STL7` | $20–$27 | 1 | lagra lokalt ord 0–7 |
| | `LDL b`, `STL b` | $28, $29 | 2 | lokalt ord b |
| | `LLA b` | $2A | 2 | lokal adress |
| Globala | `LDG0`–`LDG7` | $30–$37 | 1 | globalt ord 0–7 |
| | `LDG w`, `STG w` | $38, $39 | 3 | globalt ord |
| | `LGA w` | $3A | 3 | global adress |
| Omgivande | `LOD n b`, `STR n b` | $3C, $3D | 3 | ord b, n nivåer ut |
| | `LDA n b` | $3E | 3 | adress, n nivåer ut |
| Indirekt | `IND` | $40 | 1 | adr -- ord |
| | `STO` | $41 | 1 | adr ord -- |
| | `INDB`, `STB` | $42, $43 | 1 | adr -- tecken, adr tecken -- (byte) |
| | `MOV w` | $44 | 3 | till från --, kopiera w byte |
| | `CPS w` | $45 | 3 | a b -- -1/0/1, jämför w byte |
| Fält | `IXA w` | $48 | 3 | adr i -- adr+i·w |
| | `CHK lo hi` | $49 | 5 | i -- i, körfel 1 utanför lo..hi |
| Heltal | `ADD SUB MUL DIV MOD` | $50–$54 | 1 | DIV mot noll, körfel 2 |
| | `NEG ABS` | $55, $56 | 1 | |
| | `INC1 DEC1 SQRI` | $57–$59 | 1 | +1, -1, SQR |
| Jämförelse | `EQU NEQ LES LEQ GRT GEQ` | $60–$65 | 1 | heltal, CHAR, BOOLEAN |
| Logik | `AND OR NOT ODD` | $68–$6B | 1 | på 0/1 |
| Hopp | `UJPS s`, `FJPS s` | $70, $71 | 2 | relativt, en byte |
| | `UJP r`, `FJP r` | $72, $73 | 3 | relativt, ett ord |
| | `XJP lo hi tabell` | $74 | 5+2n | i --, CASE; utanför går vidare |
| Anrop | `MST n` | $78 | 2 | märket (4.2) |
| | `CUP k p` | $79 | 3 | anropa procedur p, k byte argument |
| | `ENT b`, `ENTW w` | $7A, $7B | 2, 3 | b lokala ord, nollställda |
| | `RET` | $7C | 1 | återhopp |
| | `LFA` | $7D | 1 | funktionsvärdets adress |
| | `CXP s p` | $7E | 3 | annat segment (stora versionen; här körfel 6) |
| Standard | `CSP n` | $80 | 2 | standardprocedur n |
| Stack | `DUP POP` | $84, $85 | 1 | |
| Slut | `ERR b` | $8E | 2 | körfel b (CASE utan fall) |
| | `HLT` | $8F | 1 | programmets slut |
| REAL | `LDCR n text` | $A0 | 2+n | -- r, talet som text |
| | `INDR`, `STOR` | $A1, $A2 | 1 | adr -- r, adr r -- |
| | `FLT`, `FLO` | $A3, $A4 | 1 | i -- r, i r -- r r |
| | `TRC`, `RND` | $A5, $A6 | 1 | r -- i, TRUNC och ROUND |
| | `ADR SBR MPR DVR` | $A7–$AA | 1 | r r -- r |
| | `NGR ABR SQRR` | $AB–$AD | 1 | r -- r |
| | `EQR NER LSR LER GTR GER` | $AE–$B3 | 1 | r r -- boolesk |
| | `SQT SIN COS ATN LNR EXR` | $B4–$B9 | 1 | r -- r, SQRT … EXP |
| | `WRR` | $BA | 1 | f r b d --, WRITE (d < 0: utan `:d`) |
| | `RDR` | $BB | 1 | f -- r, READ |

- Hoppen räknas från adressen efter instruktionen, för `XJP` efter hela
  tabellen.
- Det blir 126 opkoder när de korta formerna räknas var för sig, 98 i
  $00–$8F och 28 för REAL i $A0–$BB. En opkod som saknas i tolken ger
  körfel 6.
- `CPS w` jämför w byte från de två adresserna och ger -1, 0 eller 1, som
  sedan jämförs med 0 av `LES`, `EQU` och de andra.
- REAL-opkoderna går till FPP.BIN (4.7). Ett program utan REAL har dem
  inte, och FPP.BIN laddas inte.
- `CSP n`: allt som har med världen utanför att göra går genom CSP, och
  CSP är det enda som skiljer tolkarna åt (avsnitt 6). f är filen, ett
  kanalnummer där 0 är tangentbordet och skärmen; b är fältbredden, och
  utskriften högerställs i den.

  | n | Namn | Stacken | |
  |---|---|---|---|
  | 0 | `WRC` | f tecken b -- | WRITE(CHAR) |
  | 1 | `WRI` | f tal b -- | WRITE(INTEGER) |
  | 2 | `WRS` | f adr längd b -- | WRITE('...') |
  | 3 | `WRB` | f boolesk b -- | WRITE(BOOLEAN) |
  | 4 | `WLN` | f -- | WRITELN |
  | 5 | `RDC` | f -- tecken | READ(CHAR); radslutet ger blanksteg |
  | 6 | `RDI` | f -- tal | READ(INTEGER) |
  | 7 | `RLN` | f -- | READLN |
  | 8 | `EOL` | f -- boolesk | EOLN |
  | 9 | `EOF` | f -- boolesk | EOF |
  | 10 | `RST` | filvariabelns adr namnets adr längd -- | RESET(F, 'NAMN') |
  | 11 | `RWR` | samma | REWRITE(F, 'NAMN') |
  | 12 | `CLO` | f -- | CLOSE |
  | 13 | `RSV` | filvariabelns adr fältets adr längd -- | RESET(F, NAMN) |
  | 14 | `RWV` | samma | REWRITE(F, NAMN) |

  `WRC` skriver byten oförändrad, också 10 och 13, så att en fil kan få
  vilka bytes som helst; p-kodfilen skrivs med den. I `RSV` och `RWV`
  står namnet i ett `ARRAY OF CHAR` med ett tecken per ord, och i `RST`
  och `RWR` i en sträng eller ett `PACKED ARRAY OF CHAR`. Namnet slutar
  vid det första blanksteget.

  REAL har egna opkoder i stället för CSP-nummer.
- **Körfelen:** 1 index utanför, 2 division med noll, 3 stacken full,
  4 CASE utan fall, 5 filen, 6 okänd instruktion, 7 okänd
  standardprocedur, 8 flyttalsfel (för stort tal, roten ur ett negativt
  tal, LN av 0 ...). Division med noll i REAL är körfel 2. Tolken skriver felet och PC:n och går tillbaka till
  BASIC.

### 4.5 P-kodfilen och segmenten

Ett kompilerat program är en fil, till exempel `HEJ.PK`:

```
huvud     "PK", version (1), antal segment (1 i den lilla)
segment 0 procedurernas kod, i den ordning de blev klara
          strängkonstanter ligger i koden (LSA)
          procedurtabell (ord: offset från kodens början)
          globalernas bytes (ord), antal procedurer (ord, bit 15
          satt om programmet använder REAL)
```

Tabellen och storlekarna ligger sist, eftersom de blir kända sist:
kompilatorn skriver varje procedur med en gång och behöver ingen
hjälpfil. Tolken läser de fyra sista byten först.

- Procedur 0 är huvudprogrammet. `CUP k p` slår upp p i tabellen, så koden
  kan flyttas utan att länkas om.
- I den lilla versionen finns bara segment 0, och hela filen läses in i
  minnet.
- Den stora versionen kan ha flera segment, och `CXP` laddar ett segment
  från filen om det inte redan ligger i minnet, som UCSD:s
  `SEGMENT PROCEDURE`. Formatet har plats för det från början. Den lilla
  tolken behöver bara läsa segment 0.
- Filnamnstillägget `.PK` med två tecken går i DOS:en (4.7). I CP/M är
  det inte prövat; annars `.PKD` (?).

### 4.6 Verktygen på värddatorn

- `pkod.py`: tabellen ovan, gemensam för de andra.
- `pasm.py`: en assembler för p-kod skriven för hand, bara för proven
  och listor. Kompilatorn skriver p-koden själv, utan assembler.
- `pc.py`: kompilatorn i Python (avsnitt 3.3), med `-l` för en lista
  över p-koden.
- `pvm.py`: tolken i Python, med `-t` för ett spår av varje instruktion
  och `-s` för antalet instruktioner och stackens djup. Minnet är som på
  ABC 802 (koden på $9000, de globala efter koden, stacken nedåt från
  $ED00, eller från $DD00 med REAL), så att `-s` säger något om den
  riktiga maskinen. REAL räknas med samma FPP.BIN som i `pvm.asm`, och
  den körs av Z80-processorn ur emulatorn (`z80kor.c`, som `pvm.py`
  översätter med cc första gången), så att talen blir lika i båda
  tolkarna.
- `prov.sh`: assemblerar `prov/*.pa` och kompilerar `prov/*.pas`, kör
  dem med `prov/*.in` som tangentbord och jämför skärmen med
  `prov/*.ut`; `prov/*.fel` är ett körfel som ska komma och
  `prov/*.kfel` ett kompileringsfel. Först bygger `bygg.sh fpp` FPP.BIN.
  Sist kompileras `pc.pas` med
  `pc.py` till `pc.pk`, som i `pvm.py` kompilerar `pc.pas` och alla
  `prov/*.pas` en gång till; p-koden ska bli lika byte för byte och
  felen ska ha samma rad, kolumn och nummer. Självkompileringen tar
  11,5 miljoner instruktioner och 6 sekunder i `pvm.py`.
  - I p-kod för hand: `phej` (sträng, WHILE), `fak` (rekursiv funktion),
    `nast` (nästlad procedur, LOD/STR, VAR-parameter), `falt` (fält,
    CHK, CASE med XJP), `fel` (index utanför ger körfel 1) och `fil`
    (REWRITE, RESET, EOLN, READLN, READ).
  - I Pascal: `halsa` (FOR, SQR, WRITE med bredd), `primtal`
    (Eratosthenes såll till 8191 med ett globalt fält på 16 KB, 1028
    primtal, knappt 600 000 instruktioner), `rekursion` (funktioner,
    FORWARD, Hanois torn), `sortera` (quicksort i en nästlad procedur
    med fältet som VAR-parameter, fälttilldelning), `satser` (CASE,
    REPEAT, DOWNTO, CHAR som index, BOOLEAN, DIV och MOD med negativa
    tal), `lasa` (READ och READLN från tangentbordet, en fil som skrivs
    och läses), `index` (körfel 1) och `typfel` (fel 8 med rad och
    kolumn).
  - REAL och strängar: `reella` (konstanter, de fyra räknesätten, heltal
    blandade med REAL, funktionerna, jämförelserna, ett fält och
    parametrar av REAL, WRITE med och utan `:d`, och READ från
    tangentbordet), `strangar` (tilldelning och jämförelse av hela
    strängar, strängkonstanter, tecken med index, strängar som
    VAR-parameter och som filnamn), och kompileringsfelen `strfel` (en
    sträng med fel längd) och `reellfel` (REAL till INTEGER).
- `skiva.py`: lägger en fil på en skiva eller hämtar den, i formatet som
  `pvm.asm` läser och skriver (4.7), med `-t` för text (UTF-8 och LF på
  värden, ABC 802:s tecken och CR på skivan).
- `bygg.sh`: assemblerar `pvm.asm`, FPP.BIN (`preal.asm`, tolkens
  symboler ur `pvm.lst` och `fpp/fpp.asm`) och `basicini.asm`, och
  bygger skivan `pascal.img` med `PASCAL.ABS`, `BASICINI.SYS`,
  `FPP.BIN`, `PC.PK` (från `pc.py`), `PC.PAS` och åtta av proven i
  Pascal. `bygg.sh prov` kör dessutom alla
  proven utom kompileringsfelen i `pvm.asm` i emulatorn, med
  `BASICINI.SYS` som startar tolken, och jämför skärmen med `prov/*.ut`,
  där skärmens Ä Ö Å ä ö å läses som `[ \ ] { | }`.
  Sist kompilerar `PC.PK` på skivan `pc.pas` och ska ge samma bytes som
  `pc.py`. Det tar omkring 20 sekunder.

### 4.7 Tolken i Z80

`pvm.asm` gör samma sak som `pvm.py` och ger samma skärm i proven.

- **Registren:** DE är IP, IX är MP och Z80:ans SP är p-stacken.
  Opkoden slås upp i två tabeller om 256 byte på $8100 och $8200, den
  låga och den höga byten av adressen, så att en instruktion börjar med
  nio Z80-instruktioner. Opkoder som inte finns ger körfel 6.
- **Standardprocedurerna** (CSP) och laddaren går på BASIC:s stack,
  under den SP som CALL kom med. Där finns ett par hundra byte för
  ROM:en och DOS:en, medan p-stacken bara har en marginal på 64 byte
  över de globala.
- **Starten:** `PASCAL.ABS` läses in med DRUN, och LADDAD flyttar BASIC
  till $ED00. `Z=CALL(32768)` frågar efter programmet (`Program: `),
  lägger till `.PK` om namnet saknar punkt, läser filen till $9000 och
  kör den. CALL:s värde är 0 eller körfelets nummer. `BASICINI.SYS`
  (`basicini.asm`) laddar tolken och startar den när maskinen startar.
- **Filerna:** 1 och 2 är DOS:ens kanaler 1 och 2. På skivan är en fil
  poster om 253 byte som i Kalkyl: data på plats 3–254, ETX sist och 0
  där filen slutar. Byten 0, 3 och 16 skrivs som 16 och byten + 64. I en
  textfil är radslutet CR; CR, CR LF och LF läses alla som radslut.
- **Körfelen** skrivs som i `pvm.py`, med numret, texten och PC.
- **REAL:** när bit 15 i antalet procedurer är satt laddar tolken
  `FPP.BIN` till $DD00, och p-stacken börjar under den i stället för
  under $ED00. Opkoderna $A0–$BB hoppar genom en vektor till limmet i
  `preal.asm`, som tar talen från p-stacken, anropar FPP på BASIC:s
  stack och lägger tillbaka resultatet. LDCR, WRR och RDR går genom
  FPP:s VAL och STR. `FPP.BIN` är 3 857 byte, $DD00–$EC10: limmet och
  FPP (`fpp/fpp.asm`, R.T. Russells kod med zlib-licensen i
  `fpp/COPYING`). Ändringarna är märkta i filen: raderna i 8.4, och
  FPP sparar SP i en egen variabel i stället för i IY, som ABC 802:s
  ROM använder. Tolken utan FPP är
  4 089 byte, $8000–$8FF8.

## 5. Minnet och storleken

### 5.1 Minneskartan

```
$8000  tolken: kärnan, CSP-lagret med ROM-anropen, laddaren      ~4 KB
$9000  p-koden för programmet (för kompilatorn: PC.PK)          ~12 KB
$C000  globala variabler (för kompilatorn: symboltabellen,
       procedurbufferten, hoppstacken)                            ~4 KB
$D000  fritt, och stacken nedåt från toppen                       ~7 KB
$ED00  slut (uppmätt för Forth med KALKYL)
$F500  DOS:ens sektorbuffert, $F600 kanalbufferten som Forth använder
```

Gränserna är inte fasta. Tolken lägger p-koden direkt efter sig, de
globala efter p-koden och stacken överst, efter vad filens huvud säger.

### 5.2 Skattningen

| Del | Skattning | Grund |
|---|---|---|
| Tolkkärnan | 2–3 KB | 97 opkoder, där de korta formerna delar kod, och hopptabellen ($00–$8F, 288 byte) |
| CSP-lagret och ROM-anropen | 1–1,5 KB | skärmen, tangenterna, filerna via DOS:en, heltalen som text |
| Laddaren och körfelen | 0,5 KB | |
| **Tolken** | **3–5 KB** | |
| Kompilatorn som p-kod | 8–14 KB | 1 500–2 000 rader à 5–7 byte (?) |
| Symboltabellen | 1,5–2,5 KB | 150 namn à 8+6 byte |
| Procedurbufferten och hoppstacken | 1–2 KB | den längsta proceduren i `pc.pas` |
| Stacken | 0,5–1 KB | nedstigningen är sällan djupare än 20–30 nivåer |
| **Allt** | **15–24 KB** | mot cirka 27,9 KB ($8000–$ED00) |

Siffrorna för kompilatorn är en jämförelse, inte en mätning:
- Pascal-S är omkring 2 000 rader med tolken.
- Tiny Pascal-kompilatorn var av samma storleksordning.

Uppmätt med `pc.pas` (3.4) och `pvm.py -s`:

| Del | Uppmätt |
|---|---|
| Kompilatorn som p-kod (`PC.PK`) | 13 432 byte, $9000–$C477 |
| De globala | 9 400 byte, $C478–$E92F |
| Stacken vid självkompileringen | 620 byte |
| Kvar under $ED00 | 976 byte |

Första gången var de globala 16,9 KB och kompilatorn rymdes inte. Den
rymdes när koden packades två bytes per ord, slag, typ och nivå lades i
ett ord och gränserna sänktes till vad självkompileringen behöver med
lite marginal. Det mesta i `PC.PK` är rader som ger 5–9 byte. Tolken i
Z80 har sina 4 KB på $8000–$8FFF. Plan B behövs alltså inte, men
marginalen är liten.

Med REAL och strängarna blev `PC.PK` först 14,6 KB och rymdes inte.
`ORDEN` med de 63 orden och fördefinierade namnen gjordes om till strängar
(3.4), strängarna i kompilatorn packades, och gränserna sattes efter
vad självkompileringen nu behöver; största procedurens buffert gick
från 2 048 till 1 536 byte. Kompilatorn använder inte REAL själv och
har alltså hela minnet upp till $ED00. Ett program med REAL har 4 KB
mindre, eftersom FPP.BIN ligger på $DD00–$ECFF.

Tolken i Z80 (4.7) blev 4 062 byte, $8000–$8FDD, och 4 089 byte,
$8000–$8FF8, med vektorn för REAL. På ABC 802, i emulatorn med skivan,
tar självkompileringen omkring 750 sekunder, 12,5 minuter, för 11,5
miljoner p-instruktioner: i medeltal omkring 65 µs för en
p-instruktion, med läsningen och skrivningen av filerna. Förut var det
650 sekunder; kompilatorn har mer att göra med REAL och strängarna.

### 5.3 Plan B: två överlägg

Blir kompilatorn för stor delas den i två p-kodfiler som tolken läser in
efter varandra på samma plats:

1. Deklarationerna och symboltabellen, som skriver tabellen till
   RAM-skivan eller en fil.
2. Satserna, som läser tabellen och skriver koden.

Det är segmenten i 4.5 använda en gång. Den lilla tolken behöver då bara
kunna läsa in en ny p-kodfil och fortsätta, alltså en enkel `CXP`.

RAM-skivan (32 KB under ROM:en) kan också ta symboltabellen eller hela
procedurbufferten, men den är långsam att nå. Den är en reserv.

## 6. Från den lilla till den stora

Kärnan, alltså opkoderna i 4.4 utom CSP, skrivs så att den inte vet var
den kör:
- inga anrop till ROM:en;
- inga fasta adresser utom sina egna variabler.

Allt annat går genom ett I/O-lager med en hopptabell:

| Ingång | Lilla (BASIC-läget) | Stora (CP/M) |
|---|---|---|
| skriv tecken | ROM:ens utskrift | BIOS CONOUT |
| läs tecken | ROM:ens tangentbord | BIOS CONIN |
| öppna, läs, skriv, stäng fil | DOS:en via $6015–$6030 | BDOS 15, 20, 21, 16 |
| flyttal + - * / jämför | FPP ur BBC BASIC (8.4) | samma |
| tal till text, text till tal | FPP:s STR och VAL (8.4) | samma |
| körfel, slut | tillbaka till BASIC | tillbaka till CCP |
| ladda segment | (bara segment 0) | BDOS, efter tabellen i filen |

- Den svåra punkten var flyttalen. Under CP/M är ROM:en bortkopplad, och
  att kopiera ROM:ens rutiner till CP/M-versionen går inte, eftersom de
  är Luxors. Därför använder också den lilla versionen FPP ur BBC BASIC
  (8.4), i FPP.BIN, så att samma program räknar likadant i båda. FPP
  rör inte ROM:en, så FPP.BIN kan flyttas till den stora versionen som
  den är.
- I den stora versionen kommer det som avsnitt 2.2 sparar till sedan:
  - pekare och NEW, mängder och RECORD med WITH (gjort, avsnitt 9);
  - flera segment;
  - kompilatorn delad i segment som laddas vid behov;
  - mer minne: $0100 till under CCP:n, cirka 56 KB.

  Det är samma p-kod med fler opkoder i tabellen. Ett program för den
  lilla kör oförändrat i den stora.

## 7. Ordningen

1. `pvm.py` och p-kodens tabell (avsnitt 4.4), med några program skrivna i
   p-kod för hand. Gjort (4.6).
2. `pc.py` för delmängden, med provprogram som körs i `pvm.py`. Gjort
   (3.3, 4.6).
3. `pc.pas` och självkompileringen på värddatorn (3.2, steg 3).
   Gjort (3.4, 5.2).
4. Tolken i Z80-assembler (`pascal/pvm.asm`), först med CSP för skärmen,
   sedan filerna. `PASCAL.ABS` och en skiva som Forth-skivan. Gjort
   (4.7, 4.6).
5. Provet i emulatorn: `PC.PK` kompilerar `pc.pas` på ABC 802 till samma
   bytes. Gjort (`bygg.sh prov`, 4.6).
6. Mätningen av 5.2; vid behov plan B. Gjort (5.2): plan B behövs inte.
7. REAL och strängar. Gjort (2.1, 3.3, 3.4, 4.4, 4.7, 5.2): REAL med FPP
   ur BBC BASIC i båda versionerna, och strängar som
   `PACKED ARRAY OF CHAR`.
8. Kapitel i boken med koden. Gjort: `bok/kapitel/pascal.tex`, Pascal på
   ABC 802, efter CP/M-kapitlet. Versionen i BASIC-läget är frusen som den
   är vid steg 7; det som inte ryms hör till 64 KB-versionen (avsnitt 8).
9. Språket i 64 KB-versionen: RECORD, WITH, fält av fält, fält som
   värdeparameter, pekare, mängder, uppräkningar och delomfång i `pc.py`
   och PVM.COM. Gjort (avsnitt 9), och i Pascal i `pcs.pas` (9.5), som
   kompilerar sig själv i PVM.COM. `pc.pas` är kvar vid delmängden.

## 8. Uppslag: ett system i Pascal

Inte bestämt. En idé att ta upp nu när tolken och självkompileringen
går på ABC 802 (steg 4 och 5 i avsnitt 7).

Tanken är ett system som till största delen är skrivet i Pascal, som i
den tidiga Macen, och som startar från skivan utan BASIC. Den närmaste
förebilden är UCSD p-System: där var filhanteraren, editorn, kompilatorn
och kommandoskalet p-kod, och under fanns bara tolken och ett tunt
lager för in- och utmatning. På Z80 var det just så. I Macen var Toolbox
skriven i Pascal men till stor del översatt för hand till
68000-assembler.

### 8.1 64 KB utan BASIC

Den egna CP/M:en i `emulator/cpm/` startar redan så. Laddaren i
BASICINI.SYS (`start.asm`) läser in systemet från skivan och slår på
DTR på DART B (LRS*). Därefter är hela minnet RAM, och ROM:en och BASIC
är borta. BIOS:en (`bios.asm`) är det tunna lagret:

- tangentbordet direkt på DART B;
- skärmen, där ROM:en och bildminnet på $7800 kopplas in en kort stund
  för varje utskrift;
- skivan direkt på kort 44, sektor för sektor.

Systemet behöver BIOS:en och tolken, men varken BASIC eller ROM:ens
DOS. Uppstarten blir som för CP/M: BASICINI.SYS laddar BIOS:en och
tolken, slår på 64 KB och startar skalet i Pascal från skivan.

- **Minnet:** omkring 56 KB med CP/M:s BDOS kvar, nära 61 KB
  ($0100–$F5FF) utan den, mot 28 KB i BASIC-läget. Kompilatorn ryms
  då med marginal, och ett skal och en editor kan ligga i minnet
  samtidigt.
- **Flyttalen:** ROM:ens rutiner finns inte i 64 KB-läget, men REAL går
  redan genom FPP ur BBC BASIC (8.4), som inte behöver ROM:en.
- **Filerna:** ROM:ens DOS finns inte heller, och skivorna går inte att
  läsa från BASIC. Valet av filsystem är aktivt.

### 8.2 Filsystemet i två steg

1. **CP/M:s BDOS först.** Den är 3,5 KB och fungerar redan. Filerna
   blir CP/M-filer, som `skiva.py`, Pascal/MT+ och de andra programmen
   på CP/M-skivan kan läsa. CSP för filerna anropar BDOS i stället för
   ROM:ens DOS. Det är den stora versionen i avsnitt 6.
2. **BDOS i Pascal sedan.** Filhanteringen skrivs i Pascal ovanpå
   BIOS:ens sektorläsning, med CP/M:s katalogformat kvar så att
   skivorna går att läsa som förut. Då är allt ovanför BIOS:en Pascal.

### 8.3 Systemet

- **Segmenten** (4.5) och `CXP` bär det. Ett skal i Pascal (katalog,
  kör, visa, kopiera) ligger kvar i minnet, och editorn och kompilatorn
  är segment som laddas när de behövs.
- **Det som måste vara maskinkod** ligger i BIOS:en eller bakom CSP:
  skärmrutinerna och sökningen i texten för editorn. Självkompileringen
  är 11,5 miljoner p-instruktioner och tar omkring 750 sekunder i
  `pvm.asm` i BASIC-läget, omkring 65 µs per instruktion med filerna
  (5.2). Det duger för en kompilering men inte för en editor i p-kod
  (?).
- **Den lilla versionen** i BASIC-läget finns kvar för program som vill
  ha ABC-filerna. Samma p-kod och samma FPP.BIN går i båda; bara lagret
  under tolken skiljer.

Ordningen, om det blir av:
1. Tolken under BIOS:en med CSP mot BDOS. **Gjort** (8.5).
2. `CXP` och segmenten.
3. Skalet och startskivan.
4. Editorn.
5. BDOS i Pascal.

När skalet finns syns det om idén bär.

### 8.4 Flyttalen: FPP ur BBC BASIC

Flyttalspaketet ur BBC BASIC (Z80), skrivet av R.T. Russell 1986, går
att använda som det är. Russell släppte källkoden under zlib-licensen
2019, och den finns i David Givens cpmish och i J.B. Langstons
bbcbasic-z80 (`fpp.asm`, 2 232 rader, med COPYING). Licensen tillåter
att koden ändras och sprids, om upphovsrätten står kvar och det syns
att den är ändrad.

- **Formatet:** 40 bitar, en exponent med överskott 128 och 32 bitars
  mantissa med tecknet i den översta biten, cirka 9 siffror. Noll är
  mantissa 0 och exponent 0. Heltal är 32 bitar med exponenten 0.
- **Funktionerna:** + − × /, jämförelserna, INT, ABS, SQR, SIN, COS,
  TAN, ATN, ASN, ACS, LN, LOG, EXP, ^, PI, och STR och VAL för tal till
  text och tillbaka, alltså WRITE och READ av REAL.
- **Anropet:** en ingång, `FPP`, med operationens nummer i A och talen i
  HLH'L'C och DED'E'B. Resultatet kommer i HLH'L'C. Ett fel ger CY och
  numret i A (division med noll, för stort, negativ rot ...). IX och IY
  rörs inte, utom av ^ som ändrar IX. Paketet behöver inget av ROM:en
  och går i både BASIC-läget och 64 KB-läget.
- **Storleken:** cirka 3 KB (`$0BF4` byte). Med förrådets z80asm
  assemblerar det efter en ändring av tre rader, där `CP (A-B)/2` läses
  som en minnesadress.
- **Beprövat:** det har gått i BBC BASIC sedan 1986, och `bbcbasic.com`
  på vår CP/M-skiva har det inbyggt.
- **REAL i p-koden** är 3 ord (6 byte) på stacken: talets 5 byte och
  en byte utfyllnad, som Real i Turbo Pascal (4.1).

Valet är gjort: **FPP i båda versionerna**. Ett program räknar likadant
i BASIC-läget och i 64 KB-läget, och REAL är 3 ord överallt. I den lilla
versionen kostar det FPP.BIN, 3 857 byte på $DD00 med limmet, men bara
för program som använder REAL; kompilatorn gör det inte och har kvar
hela minnet. ROM:ens 16 siffror går förlorade, men talen räknas lika i
de två versionerna. Källan ligger i `pascal/fpp/` med `COPYING`.

Andra paket som sågs över: z80float (Zeda, Apache 2.0, 32 och 80 bitar,
inte IEEE) och math48 (Anders Hejlsberg, 48 bitar som Real i Poly
Pascal och Turbo Pascal, i z88dk), där licensen för själva koden inte
gick att hitta.

### 8.5 Steg 1: PVM.COM under CP/M

`pvm.asm` har `CPM EQU 0`. Med `CPM EQU 1` blir den PVM.COM, ett
CP/M-program från $0100, och ABC-tolken blir byte för byte densamma som
förut (`sh bygg.sh cpm`):

- **Filen:** tolken, limmet `preal.asm` och FPP ligger i samma fil,
  och p-koden laddas till `KOD` efter dem, $2611. PVM.COM är 9 489 byte
  med det större språket (avsnitt 9); före det var KOD $2411.
  Det finns ingen FPP.BIN, och RVEK pekar på RGLUE när programmet
  använder REAL.
- **Minnet:** p-stacken börjar under BDOS, sidan under adressen på
  $0006, $E800 med vår CP/M. P-koden, de globala, högen och stacken
  har därmed $2611–$E7FF, 49,5 KB, mot 23,75 KB ($9000–$ECFF) i BASIC-läget.
- **Start och slut:** `PVM NAMN` kör NAMN.PK, med `.PK` tillagt om
  namnet saknar punkt. Utan namn frågar tolken `Program: ` som förut.
  Det som står efter namnet blir den första raden från tangentbordet
  (SVANS, KSVANS), så att `PVM PC HEJ.PAS` kompilerar HEJ.PAS; utan det
  läser PC namnet på nästa rad, och det som skrevs sedan blev annars
  källans namn och gav körfel 5. (Bara i PVM.COM; i BASIC-läget finns
  inte platsen under $9000.)
  Tolken har en egen stack på 256 byte (CSTK) för CSP, FPP och BDOS,
  med 0 underst, så att återhoppet blir en varmstart.
- **Tecknen:** PRTXT är BDOS 2 för varje tecken och INLINE är BDOS 10.
  WRITELN ger CR LF på skärmen och i filer.
- **Filerna:** varje fil har en sida (FBUF, fil 1 och 2):

  | Plats | Innehåll |
  |---|---|
  | +$00–$7F | posten |
  | +$80 | FCB:n |
  | +$C8 | antalet poster kvar, ur BDOS 35 när filen öppnas |
  | +$CA | antalet bytes i posten |

  RESET öppnar med BDOS 15. REWRITE tar bort den gamla med BDOS 19 och
  gör en ny med BDOS 22. Läsningen och skrivningen går post för post
  med BDOS 20 och 21, med DMA på sidan. Vid stängningen fylls en fil som
  skrivs ut med $1A och stängs med BDOS 16. I den sista posten är $1A
  sist inte med i filen. En .PK-fil slutar med $00 eller $80, så den
  klipps aldrig. Bytes behöver inte skyddas som 0, 3 och 16 i DOS:ens
  poster. Ett namn kan ha enheten först, som `B:UT.TXT`.
- **Namnen:** DIV0, MUL1 och FEL8 fanns både i tolken och i limmet. I
  tolken heter de nu DIVN0 och MULS1, och i limmet RFEL8.

`sh bygg.sh cpmprov` bygger dataskivan `cpm.img` (B:) med PVM.COM,
PC.PK, PC.PAS, PCS.PK, PCS.PAS (9.5) och exemplen, med CR LF i textfilerna. Proven körs i
emulatorn med CP/M-skivan `../emulator/cpm/a.img` som A:, och skärmen
jämförs med prov/*.ut som i `bygg.sh prov`. En rad om 80 tecken bryts av
BIOS:en, så den tomma raden efter den räknas inte. Sist kompilerar
`PVM PC` PC.PAS på B:, och `cpmfil.py` hämtar PC.PK ur skivan. Alla 16
går igenom, nu 18 med de tre i prov/stor, och PC.PK blir samma 13 432 byte som från `pc.py`. I emulatorn
tar självkompileringen 732 sekunder från returen efter PC.PAS till B>,
mot omkring 750 i BASIC-läget, och PCS.PK på PCS.PAS (9.5) 1 178
sekunder.

## 9. Den stora versionen: språket

PVM.COM (8.5) kör också ett större språk. `pc.py` kompilerar det, och
`pcs.pas` (9.5) är samma kompilator i Pascal. `pc.pas` är kvar vid
delmängden och ger PC.PK som förut.

### 9.1 Typerna

- **Uppräkningar:** `FARG = (ROD, GRON, BLA)`. Namnen är konstanter
  0, 1, 2, och ORD, SUCC, PRED, jämförelser, FOR och CASE går på dem.
  WRITE av en uppräkning går inte; man skriver ORD.
- **Delomfång:** `SIFFRA = 0..9`, också av CHAR och uppräkningar. En
  tilldelning till ett delomfång kontrolleras med CHK (körfel 1 utanför).
  Ett fält kan ha typen som index: `ARRAY [FARG] OF INTEGER`.
- **RECORD:** fälten ligger i ordning efter varandra, och en post kan
  tilldelas och skickas hel. `WITH P, Q DO` gör fälten till namn; WITH
  med ett uttryck som `A[I]` räknar adressen en gång och sparar den i
  ett eget ord i ramen.
- **Fält av fält:** `ARRAY [1..3, 1..3] OF INTEGER` är ett fält av fält,
  och `M[I, J]` är `M[I][J]`. Fält och poster kan vara värdeparametrar;
  den anropade proceduren kopierar dem till ramen efter ENT (fel 21 är
  borta i den stora versionen).
- **Pekare:** `^T`, där T får deklareras senare i samma TYPE-del, så att
  en lista kan peka på sig själv. NIL är 0, och pekare jämförs med `=`
  och `<>`. `NEW(P)` tar typens storlek ur högen och nollställer den.
  `MARK(P)` sparar högens topp i P och `RELEASE(P)` lämnar tillbaka allt
  efter den. DISPOSE finns inte.
- **Mängder:** `SET OF T`, där T är CHAR, BOOLEAN, en uppräkning eller ett
  delomfång inom 0..255 (fel 28 annars). En mängd är 32 byte. Det finns
  `[ ]`, `[A, B..C]`, `+ - *`, IN, `= <>` och `<= >=` som delmängd.

### 9.2 P-koden

En mängd ligger på stacken som 32 byte, byte 0 vid SP; elementet i är
bit i&7 i byte i>>3.

| Kod | Namn | Stacken |
|---|---|---|
| $90 | LDSE | -- m, den tomma |
| $91 | SGS | i -- [i], körfel 1 utanför 0..255 |
| $92 | SRS | lo hi -- [lo..hi] |
| $93–$95 | UNI INT DIF | m m -- m |
| $96 | INN | i m -- boolesk |
| $97–$98 | EQS NES | m m -- boolesk |
| $99–$9A | LSS GSS | m m -- boolesk, delmängd |
| $9B | INDS | adr -- m |
| $9C | STOS | adr m -- |

CSP 15 är NEW (pekarens adress och antalet bytes), 16 MARK och 17
RELEASE. Resten av det nya, poster, WITH, fält av fält och kopiorna,
klaras med de gamla opkoderna (IXA, MOV, LDA, CHK).

Ett program som använder något av det får versionsbyten 2 i .PK-filen,
annars 1 som förut. Den lilla tolken i BASIC-läget vägrar version 2, och
`pvm.py` utan `-c` ger körfel 7 för CSP 15–17; de nya opkoderna finns bara i PVM.COM, under
`IF CPM` i `pvm.asm`. Den lilla tolken är byte för byte densamma.

### 9.3 Högen

Högen växer uppåt från slutet av de globala (HOG0) och p-stacken nedåt
från $E7FF. NEW ger körfel 9, "minnet är fullt", när högens topp plus
storleken plus en marginal på 256 byte når stackpekaren. Stacken prövar
mot samma gräns. RELEASE med en pekare utanför HOG0..HOG ger också körfel 9.

### 9.4 Proven

`prov/stor/` har poster.pas (poster, WITH, matriser, uppräkningar och
CHK), pekare.pas (en lista, ett träd, MARK och RELEASE, och en hög som
tar slut), mangder.pas (mängder och ett såll) och mfel.pas (fel 28).
`sh prov.sh` kör dem i `pvm.py -c`, som har PVM.COM:s minne, och prövar
att den lilla `pvm.py` vägrar dem. `sh bygg.sh cpmprov` kör dem också i
PVM.COM i emulatorn.

### 9.5 pcs.pas

`pcs.pas` är kompilatorn för det större språket, skriven i delmängden,
2 157 rader med 100 procedurer och funktioner. Den bygger på `pc.pas`
och ger samma bytes som `pc.py`. `pc.py` kompilerar den till PCS.PK,
18 931 byte, med 21 362 byte globala, och den går bara i `pvm.py -c` och
PVM.COM. Det nya:

- **Typtabellen:** TK, T1, T2 och T3 för fält, strängar, poster,
  mängder, pekare, uppräkningar och delomfång; de enkla typerna är 1–7,
  och en strängkonstant med längden n har typen -n. Fält, pekare och
  mängder med samma led får samma nummer, så att jämförelsen av typer
  blir enkel. Fälten i posterna står i FNAMN, FOFF, FTY och FNXT, och
  TYPER, FTOP och POOLTOP sätts tillbaka när en procedur är slut.
- **TX i symboltabellen:** ett fält i WITH med en räknad adress pekar
  ut det gömda ordet, och ett fält eller en post som värdeparameter
  pekar ut ordet med adressen som KROPP kopierar från. `P.X` i en hel
  post blir en egen variabel i en post över TABMAX, som används igen i
  nästa sats.
- **NLISTA:** namnen i en deklaration läggs på en stack, eftersom en
  RECORD kan stå i en VAR-del och ha egna namn.
- **HEJ.TMP:** versionsbyten i huvudet blir 2 först när koden har en
  mängd eller högen, och det vet man på slutet. Koden skrivs därför
  till HEJ.TMP med två bokstäver per byte, och HEJ.PK skrivs när källan
  är stängd, eftersom PVM.COM har två filer öppna åt gången. HEJ.TMP
  töms efteråt.

`sh prov.sh` kompilerar `pcs.pas` med `pc.py`, och PCS.PK kompilerar i
`pvm.py -c` sig själv, `pc.pas`, de små proven och `prov/stor/*.pas` till
samma bytes som `pc.py` (mfel ger fel 28). `sh bygg.sh cpmprov` gör
detsamma med sig själv och `prov/stor/*.pas` i PVM.COM i emulatorn.

### 9.6 Kvar

- GOTO, procedurer som parametrar och variantposter.
- Skärmen och tangentbordet i PVM.COM, för skärmprogram (en editor, ett
  spel). Markören styrs redan med kontrolltecken genom WRITE, eftersom
  CSP 0 skickar tecknet oförändrat till BDOS 2 och BIOS:en tolkar
  ESC = r k (rad och kolumn + 32), CHR(26) töm skärmen, CHR(30) hem,
  CHR(8), CHR(11) och CHR(12) vänster, upp och höger, ESC T och ESC Y
  sudda till radens och skärmens slut, ESC E och ESC R skjut in och ta
  bort en rad. Det som behövs är procedurer i källkoden (GOTOXY, CLRSCR,
  CLREOL), kanske som en fil att ta med, och ett prov i `prov/stor/`.
  Det som saknas är tangentbordet: READ läser en hel rad med eko. Nya
  CSP efter 17, bara i version 2 och i PVM.COM: KEYPRESSED, som frågar
  utan att vänta (BDOS 11), och READKEY, som läser en tangent utan eko
  (BDOS 6), i pc.py, pvm.asm och pvm.py -c. Den lilla tolken under
  ROM:ens CON: klarar inte ESC = tecken för tecken och får inte det här.
