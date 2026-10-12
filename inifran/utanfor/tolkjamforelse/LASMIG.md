# Två tolkar, samma förbättringar? (utanför boken)

*Inte med i boken.* Ett kontrafaktiskt försök. Antag att ABC80-BASIC:en
(1978) och BASIC II för ABC80 (ABC800:s tolk, flyttad till ABC80 omkring
1984-86) hade fått en omgång till: vad hade gjort dem snabbare, och
hjälper samma sak båda? Försöket mäter tolkarna med samma prov i
emulatorn, fördelar tiden på ROM:ens rutiner och provar ändringar i
lappade kopior av ROM:arna. Tyngdpunkten ligger på ABC80. Bokens filer
och ROM:arna i `emulator/roms/` ändras inte.

## Det viktigaste

- *ABC80-BASIC:en kunde ha gått 14-31 % fortare utan att språket
  ändrades.* Det kräver sju ändringar, omkring 730 byte kod och 512
  byte tabeller i en tilläggs-ROM på `$5000`, plus några hopp i den gamla
  ROM:en. Varje prov ger samma skärm, samma fel och samma tal (B1 och
  FIZZ1 vinner bara 3,5-3,8 %).
- *Tiden sitter på tre ställen.* Det är BCD-divisionen, satsslingan
  med variablerna, och tilldelningen `V=V+X`, som är en stor del av de
  inre slingorna.
- *Där tolkningen är det som kostar blir ABC80 lika snabb som BASIC
  II.* Det gäller också när BASIC II lappas på samma sätt: NOEL1 6,42 s
  mot 6,42 och SIEVE 20,9 mot 25,4. BASIC II:s tolk är inte i grunden
  snabbare.
- *Där talen är det som kostar är BASIC II kvar långt före.* B8 tar
  11,3 s mot 2,9 och MANDEL 139,6 mot 92,6. Skillnaden är BCD mot binära
  flyttal, och den rår ingen lapp i tolkningen på.
- *Samma grepp hjälper båda, men olika mycket.* Mönstret `V=V+X` ger
  23 % i ABC80 och 17 % i BASIC II (NOEL1). Slingan i fältindexet ger
  14 % i BASIC II (SIEVE), och ABC80 behöver den inte. NEXT med HL i
  stället för IX ger nästan ingenting.
- *En hel omskrivning av ABC80-BASIC:en hade inte lönat sig för
  tolkningens skull.* Den hade bara köpt flyttalen, och priset hade
  varit kompatibiliteten: BAC-filerna, tilläggs-ROM:arna och talens
  decimaler. Se "Hade en omskrivning lönat sig?" nedan.

## Förutsättningar

- *Emulatorn, inte en riktig maskin.* Tiden är emulatorns T-cykler vid
  2,9952 MHz, från att CR efter `RUN` trycks ned tills tolken är
  tillbaka i `READY` (`$00CC` i ABC80, `$0110` i BASIC II). Allt som skrivs
  på skärmen ingår. BASIC II i emulatorn stämmer med användarens
  mätningar på en riktig ABC 802: B3 5,98 mot 5,91 s, B7 19,83 mot 19,84,
  NOEL1 7,84 mot 8,01 och MANDEL 92,6 mot 93. Mot en riktig ABC80 är det
  inte prövat.
- *Minnet.* ABC80 har 16 KB RAM och BASIC II 32 KB.
- *ABC80-ROM:en.* Ändringarna gäller den gamla ROM:en (abc80old). Den
  nya går lika fort i alla prov.
- *Platsen för ändringarna.* ABC80:s ROM är nästan full. De nya
  rutinerna ligger därför i en krets på `$5000`, som en tilläggs-ROM, och
  ROM:en får bara de hopp som leder dit. Det är en krets till i maskinen.
  I BASIC II ligger lapparna i `$0450-$05D3`. Där står bara nollor
  (`$044A-$06A9`), för ABC800 M:s kassettdrivrutin CAS: togs bort i
  ABC80-versionen. `$7800-$7FFF` är `$FF` i ROM-filen men inte ledigt i
  maskinen, se "Vad som kom fram på vägen".
- *Inget i språket ändras.* Talen räknas med samma BCD och samma
  avrundning, och koden som kompilatorn skriver är densamma. Ingen ny
  uttryckskod tillkommer: `V=V+X` är ett mönster som tolken känner igen
  i den gamla koden. Det som ändras är hur ROM:en tar sig fram till
  samma resultat.
- *Ändringar som var möjliga 1978.* Ingen ny algoritm används, bara HL
  i stället för (IX+n), tabeller i stället för slingor, en prövning
  mindre per sats och mönstret.
- *Proven.* Proven är 16 program ur `ABC80/abc802` i det publika
  förrådet. B1-B8 är Rugg/Feldman (Kilobaud 1977), B8 från PCW 1978 med
  100 varv; NOEL och NOEL1 är oförändrade. De andra är ändrade så att
  ABC80 kan köra dem:
  - `MOD(A,B)` är `A-INT(A/B)*B` och `WHILE` är `IF`/`GOTO`;
  - indragen och tiderna (`TIME$`, `EXTEND`) är borta;
  - variabelnamnen har en bokstav och en siffra;
  - INTER går till 100 i stället för 1000.

  Utskriften är densamma i båda tolkarna. Hur mycket ett annat program
  vinner beror på vad det gör. Ändringarna själva är prövade mot mycket
  mer än proven (se "Att ändringarna inte bara fungerar för proven").

## Filerna

| Fil           | Vad                                                         |
|---------------|-------------------------------------------------------------|
| `allt.sh`     | gör om allt nedan i ordning, utskriften i `ut/allt.txt`     |
| `prof.c`      | värd på emulatorns kärna: skriver in ett program, RUN, mäter T-cykler och instruktioner per adress (`MINNE=fil` sparar minnet efteråt) |
| `kor.sh`      | kör proven i gamla och nya ABC80-ROM:en och i BASIC II      |
| `profil.py`   | summerar en profil per rutin (namnen ur listningarna)       |
| `handelser.py`| räknar rader, satser och uttryckskoder per prov             |
| `lappa.py`    | bygger lappade ROM:ar (och för ABC80 en krets på `$5000`)     |
| `lapp/*.asm`  | ändringarna, en fil var, med en beskrivning överst          |
| `lapptid.sh`  | kör proven med en lappad ROM och jämför tid och skärm        |
| `prov/*.BAS`  | proven                                                       |
| `kantprov.sh` | kantfallen i `kant/prov.txt` i olappad och lappad ROM        |
| `felprov.py`  | medvetna fel i `a_sup` och `b_sup`, körda mot kantfallen     |
| `fprov.c`     | FMUL och FDIV anropade direkt i båda ROM:arna, slumpade tal och kanttal |
| `mulprov.c`   | `b_mul` mot BASIC II:s MUL16, register för register          |
| `bokprov.sh`  | bokens tolkprov (`../../dis/04/host/tests`) med de lappade ROM:arna |

Programmen byggs med emulatorns kärna (`../../emulator/karna`) och
lapparna med `../../dis/build/z80asm`. `./allt.sh` gör om allt, det tar
en god stund.

## Hur fort de är

`./kor.sh`, 2026-10-11 (sekunder; kvoten är ABC80 / BASIC II):

| Prov   | ABC80 (gamla) | ABC80 (nya) | BASIC II | Kvot |
|--------|---------|---------|---------|------|
| B1     |   0,958 |   0,958 |   0,858 | 1,12 |
| B2     |   2,115 |   2,115 |   1,814 | 1,17 |
| B3     |  10,853 |  10,853 |   5,977 | 1,82 |
| B4     |  11,827 |  11,827 |   5,998 | 1,97 |
| B5     |  12,246 |  12,246 |   6,440 | 1,90 |
| B6     |  17,485 |  17,485 |  11,683 | 1,50 |
| B7     |  23,574 |  23,574 |  19,833 | 1,19 |
| B8     |  13,318 |  13,318 |   2,886 | 4,61 |
| FIZZ   |   3,573 |   3,573 |   2,229 | 1,60 |
| FIZZ1  |   1,403 |   1,403 |   1,473 | 0,95 |
| INTER  |  12,929 |  12,929 |   6,728 | 1,92 |
| MANDEL | 169,131 | 169,131 |  92,573 | 1,83 |
| NOEL   |  24,418 |  24,418 |  18,766 | 1,30 |
| NOEL1  |   9,333 |   9,333 |   7,839 | 1,19 |
| PRIMES |  93,562 |  93,564 |  44,262 | 2,11 |
| SIEVE  |  26,312 |  26,312 |  30,655 | 0,86 |

BASIC II vinner mest på flyttal: B8, med `^`, `LOG` och `SIN`, går 4,6
gånger fortare, eftersom ABC80 räknar med BCD. Med heltal är tolkarna
nästan lika, och ABC80 vinner på fält (SIEVE).

## Var tiden går

Ur profilerna (`profil.py`, rutinerna med namn i listningarna):

- *Flyttalen.* I B4 går 27 % av ABC80:s tid i BCD-divisionen
  (FDLOOP) och omkring 30 % till i multiplikationen och skiftningarna;
  i BASIC II 17 % i FDIV och 13 % i FMUL. ABC80:s multiplikation hoppar
  redan över nollbitar men bygger en tabell (2a ... 80a) för varje gång.
- *Uttryckskoderna.* ABC80 går genom EXEXP för varje kod: RST `$28`,
  INC SP två gånger, JP, PUSH HL och tabelluppslaget, omkring 135 T.
  EXEXP är 17-25 % av tiden i heltalsproven. BASIC II:s EXECA är lika
  stor andel, men varje kod gör mer. Där kedjas koderna med RST `$28`
  (EXECNX), omkring 98 T per kod.
- *Raderna och satserna.* ABC80 prövar TRACE för varje rad och
  anropar TSTBRK för varje sats (82 T); RUNLIN och RUNSTM är 20 % av
  NOEL1. BASIC II prövar allt (BREAK, TRACE, CTRL-S) på en flaggbyte,
  35 T.
- *Variablerna.* Ett värde är två koder i ABC80 (VAR0 med adressen,
  sedan GETF eller GETI), en i BASIC II (LDFVAR, LDIVAR).
- *Fälten.* BASIC II multiplicerar index med steget (2 för heltal)
  i MUL16, alltid 16 varv, omkring 1000 T: 17 % av SIEVE. ABC80 är
  snabbare där.
- *FOR/NEXT.* NEXT är 20 % av NOEL1 i BASIC II och 9 % i ABC80 (plus
  FORTSI).

## Ändringarna

Varje ändring är en egen fil i `lapp/`. `lappa.py a|b namn lapp ...`
bygger `ut/namn.rom`, och för ABC80 också `ut/namn.krets`.

| Lapp     | Tolk     | Byte | Vad |
|----------|----------|------|-----|
| `a_brk`  | ABC80    |    8 | BREAK-flaggan läses direkt i RUNSTM (27 T i stället för 82); TSTBRK:s nollställning görs bara när BREAK kommit |
| `a_tab`  | ABC80    | 19+512 | EXEXP slår upp koden i två tabeller på hela sidor (`$5E00`, `$5F00`), 82 T i stället för 91, med samma B, C och CY som förut |
| `a_var`  | ABC80    |   35 | VAR0 ser efter om nästa kod är GETF eller GETI och hämtar värdet direkt; koden som genereras är densamma |
| `a_sup`  | ABC80    |  297 | som `a_var`, och VAR0 känner dessutom igen `V=V+X` och `V=V-X` (X en konstant eller variabel av samma typ) och gör hela satsen själv |
| `a_madd` | ABC80    |   37 | additionen i FMUL (MADD3 och den översta byten) med HL i stället för (IX+n) |
| `a_fswp` | ABC80    |   39 | FMUL byter faktorerna så att den med flest nollbyte i låga änden blir multiplikator |
| `a_ftab` | ABC80    |  262 | FMUL:s tabell (a, 2a ... 80a) utan slingor, med HL och DE |
| `a_fdiv` | ABC80    |   72 | subtraktionerna i FDIV (FDLOOP) med HL och DE i stället för (IX+n) |
| `b_mul`  | BASIC II |   28 | index gånger steget i ARRELM med en slinga som slutar när steget är slut (2-3 varv) i stället för MUL16 |
| `b_nxt`  | BASIC II |   95 | NEXT för en heltalsvariabel med HL genom blocket i stället för (IX+n) |
| `b_sup`  | BASIC II |  260 | satsslingan känner igen `V=V+X` och `V=V-X` (heltal och flyttal) och gör satsen själv |

Uppsättningarna är följande:

- `a_hel` = a_brk, a_tab, a_sup;
- `a_flt` = a_madd, a_fswp, a_ftab, a_fdiv;
- `a_alla` = båda;
- `b_alla` = b_mul, b_nxt, b_sup.

`a_var` ingår i `a_sup` och ingår inte i uppsättningarna för sig.

## Tiderna

`./lapptid.sh namn`, 2026-10-11, vinsten i procent av tiden. Skärmen är
densamma som utan lappen i alla 16 prov och alla uppsättningar.

Heltal, satser och variabler i ABC80:

| Prov   | a_brk | a_tab | a_var | a_sup | a_hel |
|--------|-------|-------|-------|-------|-------|
| B1     |   3,5 |   0,0 |   0,0 |   0,0 |   3,5 |
| B2     |   3,3 |   1,6 |   3,3 |  10,9 |  14,8 |
| B3     |   0,9 |   0,8 |   2,3 |   3,4 |   4,8 |
| B4     |   0,9 |   0,6 |   0,9 |   1,9 |   3,2 |
| B5     |   1,3 |   0,6 |   0,8 |   1,9 |   3,5 |
| B6     |   2,0 |   0,5 |   0,6 |   1,2 |   3,6 |
| B7     |   2,2 |   0,8 |   2,0 |   2,4 |   5,3 |
| B8     |   0,1 |   0,1 |   0,1 |   0,2 |   0,3 |
| FIZZ   |   0,5 |   0,3 |   0,6 |   0,6 |   1,3 |
| FIZZ1  |   1,3 |   0,7 |   1,9 |   1,9 |   3,8 |
| INTER  |   2,0 |   0,7 |   2,0 |   1,3 |   3,9 |
| MANDEL |   1,5 |   0,9 |   2,7 |   2,7 |   4,7 |
| NOEL   |   2,8 |   0,9 |   2,9 |  10,9 |  14,1 |
| NOEL1  |   7,3 |   2,3 |   9,8 |  23,0 |  31,2 |
| PRIMES |   0,8 |   0,4 |   1,3 |   1,3 |   2,5 |
| SIEVE  |   6,4 |   2,4 |   9,0 |  12,7 |  20,6 |

Flyttalen i ABC80, och allt tillsammans:

| Prov   | a_madd | a_fswp | a_ftab | a_fdiv | a_flt | utan a_fswp | a_alla |
|--------|--------|--------|--------|--------|-------|-------------|--------|
| B1     |   0,0  |   0,0  |   0,0  |   0,0  |   0,0 |   0,0       |   3,5  |
| B2     |   0,0  |   0,0  |   0,0  |   0,0  |   0,0 |   0,0       |  14,8  |
| B3     |   1,8  |   3,3  |   2,6  |   7,5  |  14,0 |  11,9       |  18,8  |
| B4     |   0,8  |  −0,8  |   2,4  |  14,2  |  16,6 |  17,3       |  19,8  |
| B5     |   0,7  |  −0,7  |   2,3  |  13,7  |  16,0 |  16,7       |  19,5  |
| B6     |   0,5  |  −0,5  |   1,6  |   9,6  |  11,2 |  11,7       |  14,8  |
| B7     |   0,4  |  −0,4  |   1,2  |   7,1  |   8,3 |   8,7       |  13,6  |
| B8     |   7,1  |   0,4  |   7,7  |   0,0  |  14,5 |  14,8       |  14,8  |
| FIZZ   |   0,7  |  −0,7  |   1,9  |  15,1  |  17,1 |  17,7       |  18,4  |
| FIZZ1  |   0,0  |   0,0  |   0,0  |   0,0  |   0,0 |   0,0       |   3,8  |
| INTER  |   0,0  |   0,0  |   0,0  |  21,6  |  21,6 |  21,6       |  25,5  |
| MANDEL |   6,1  |   3,5  |   4,5  |   0,0  |  12,7 |  10,6       |  17,5  |
| NOEL   |   0,0  |   0,0  |   0,0  |   0,0  |   0,0 |   0,0       |  14,1  |
| NOEL1  |   0,0  |   0,0  |   0,0  |   0,0  |   0,0 |   0,0       |  31,2  |
| PRIMES |   0,7  |  −0,8  |   2,3  |  18,2  |  20,5 |  21,3       |  23,0  |
| SIEVE  |   0,0  |   0,0  |   0,0  |   0,0  |   0,0 |   0,0       |  20,6  |

`utan a_fswp` är `ut/a_flt2`, med a_madd, a_ftab och a_fdiv. Vinsterna
läggs ungefär ihop, eftersom lapparna rör olika rutiner.

BASIC II:

| Prov   | b_mul | b_nxt | b_sup | b_alla |
|--------|-------|-------|-------|--------|
| B1     |   0,0 |  −0,6 |   0,0 |  −0,6  |
| B2     |   0,0 |   0,0 |   4,6 |   4,6  |
| B3     |   0,0 |   0,0 |   1,4 |   1,4  | 
| B4     |   0,0 |   0,0 |   1,4 |   1,4  |
| B5     |   0,0 |   0,0 |   1,3 |   1,3  |
| B6     |   0,0 |  −0,2 |   0,7 |   0,5  |
| B7     |   6,4 |  −0,1 |   0,2 |   6,6  |
| B8     |   0,0 |   0,0 |   0,1 |   0,1  |
| FIZZ   |   0,0 |   0,0 |   0,0 |   0,0  |
| FIZZ1  |   0,0 |   0,1 |   0,0 |   0,1  |
| INTER  |   0,0 |  −0,1 |   0,0 |  −0,1  |
| MANDEL |   0,0 |   0,0 |  −0,0 |  −0,0  |
| NOEL   |   0,0 |  −0,3 |   5,8 |   5,6  |
| NOEL1  |   0,0 |   1,4 |  16,7 |  18,1  |
| PRIMES |   0,0 |  −0,1 |   0,1 |   0,0  |
| SIEVE  |  13,9 |   0,3 |   3,0 |  17,2  |

Båda tolkarna lappade, i sekunder (kvoten är ABC80 / BASIC II, före och
efter):

| Prov   | a_alla | b_alla | Kvot förut | Kvot nu |
|--------|--------|--------|------------|---------|
| B1     |  0,924 |  0,863 | 1,12       | 1,07    |
| B2     |  1,801 |  1,730 | 1,17       | 1,04    |
| B3     |  8,812 |  5,893 | 1,82       | 1,50    |
| B4     |  9,490 |  5,914 | 1,97       | 1,60    |
| B5     |  9,857 |  6,356 | 1,90       | 1,55    |
| B6     | 14,891 | 11,622 | 1,50       | 1,28    |
| B7     | 20,372 | 18,529 | 1,19       | 1,10    |
| B8     | 11,342 |  2,882 | 4,61       | 3,94    |
| FIZZ   |  2,915 |  2,229 | 1,60       | 1,31    |
| FIZZ1  |  1,349 |  1,471 | 0,95       | 0,92    |
| INTER  |  9,636 |  6,733 | 1,92       | 1,43    |
| MANDEL |139,572 | 92,613 | 1,83       | 1,51    |
| NOEL   | 20,981 | 17,722 | 1,30       | 1,18    |
| NOEL1  |  6,421 |  6,417 | 1,19       | 1,00    |
| PRIMES | 72,053 | 44,254 | 2,11       | 1,63    |
| SIEVE  | 20,888 | 25,386 | 0,86       | 0,82    |

### Det som inte är självklart

- *`a_fswp` beror på programmet.* Bytet lönar sig när den ena faktorn
  är ett litet heltal eller har få siffror. I B3 och MANDEL ger det
  omkring 2 procentenheter. När det inte gör det kostar det provet
  0,3-0,8 (B4, B5, PRIMES och andra). Att det är med i `a_alla` är ett
  val för just de här proven.
- *`a_tab` ger lite för sina 512 byte.* Den första versionen, 54 T,
  satte inte B och C som ROM:en och var därför snabbare. Den gav fel i
  alla prov med funktioner. Med samma register som ROM:en återstår 9 T
  per kod, 0,5-2,4 %. Av lapparna är den den sämsta affären räknat i
  byte.
- *`b_nxt` ger nästan inget.* Lappen tar bara heltalsvariabler och
  lämnar flyttalen åt ROM:en. Den vinner i NOEL1 (1,4 %) och nästan
  inget i FIZZ1 och SIEVE. Den förlorar lite i sju prov, mest i B1
  (0,6 %). B1 är bara `FOR K=1 TO 1000` med ett flyttal. Förlusten där är
  omkring 15 T per NEXT: hoppet till lappen och HL i stället för IX fram
  till typen.

  Den första versionen räknade fram blocket och variabelns referens,
  prövade typen och lämnade sedan flyttalen till ROM:en på `$2816`, där
  allt gjordes om. Den förlorade 130 T per NEXT, 5,1 % i B1 och 2,4 % i
  NOEL. Två sätt att undvika det är prövade:
  - typen först, med blocket + 6 räknat i A före allt annat: förlusten
    blir omkring 78 T (3,0 % i B1), men heltalen får 26 T mer och NOEL1
    vinner bara 0,3 %;
  - variabelns adress och VFF36 före typen, och flyttalen till ROM:ens
    flyttalsväg på `$285E` med IX och HL som ROM:en har dem där. Inget
    arbete görs om, och heltalsvägen är densamma. Det är lappen nu.
- *`a_sup` är ett mönster, inte en ny kod.*
  - *Koden.* `C%=C%+1%` kompileras till
    `C1 n b lo hi | C1 n b lo hi D5 | C7 01 00 | F5 | B7`, fem koder
    (fyra med `a_var`). `K=K+1` kompileras till `C0 … | C0 … D4 | C3`
    och fem byte `| F4 | B6`.
  - *Igenkänningen.* När VAR0 ska lägga en adress på stacken, alltså
    för variabeln som tilldelas, ser den efter om resten är just
    mönstret: samma adress med GETI eller GETF, en konstant eller en
    variabel av samma typ med GET, plus eller minus, och tilldelningen.
  - *Satsen.* Då gör VAR0 satsen själv: heltal med ADD eller SBC som
    ADDI och SUBI, flyttal med ROM:ens FADD och FSUB på stacken som
    FARITH. Variabeln skrivs först när de kommit tillbaka, så att fel 4
    lämnar den orörd. VAR0X slutar som ASGNI och ASGNF, med HL, B och DE
    som de. Koderna läses i tur från början av en kod, så mönstret kan
    inte hittas mitt i en annan kods byte.
  - *Vinsten* är stor där satsen står i den inre slingan: NOEL1
    (`S%=S%+J%`) 23,0 % mot 9,8 med `a_var`, NOEL och B2 omkring 11 mot
    3, SIEVE 12,7 mot 9,0.
  - *Kostnaden* är 34 T för en tilldelning där nästa kod inte är en
    variabel, och 104 T när nästa kod är en annan variabel av samma typ,
    som `M=N/K` i INTER. Där blir vinsten 1,3 % i stället för 2,0.
- *`b_sup` är samma mönster i BASIC II.* Det ger mindre än i ABC80,
  eftersom BASIC II redan har en kod per variabel.
  - *Koden.* `S%=S%+J%` kompileras till `C1 r | C1 r' | 9C | B5 r`,
    fyra koder genom EXECA. X kan också vara ett litet heltal (`$DD-$FF`)
    eller INTCON (`C7 lo hi`). `K=K+X` kompileras till
    `C0 r | X | 9B | B4 r` (9E för minus), där X är en variabel eller en
    konstant (`$D5-$DC` och 1-8 byte).
  - *Var den sitter.* Satsslingan anropar EXECA för satsens första kod
    (`$1D1A`). Lappen tar det anropet. Koderna inne i ett uttryck och FOR:s
    tilldelning (RST `$20`) går inte dit och kostar inget.
  - *När mönstret inte stämmer* går lappen direkt till LDIVAR eller
    LDFVAR, för uppslaget i EXECA är redan gjort. Den första versionen
    satt i LDIVAR:s tabellpost och förlorade tid i SIEVE och FIZZ1.
  - *Heltalen* räknas med ADD och SBC och slår runt utan fel, som i
    IADD och ISUB. Lappen slutar som STIVAR.
  - *Flyttalen* läggs på stacken med ROM:ens FLOAD och NUMPSH. FADD
    eller FSUB anropas med samma register och PRGPTR som från FDISP, och
    spill går till FARITH:s fel 130. Satsen slutar i ROM:ens STFVAR,
    så att variabeln skrivs först när additionen gått bra. DOUBLE (8
    byte) går samma väg.
  - *Vinsten* är NOEL1 16,7 %, NOEL 5,8 % och B2 4,6 %.

## Att ändringarna inte bara fungerar för proven

Att proven ger samma skärm säger bara att ändringarna fungerar för det
de 16 proven gör. Ändringarna är därför prövade på fyra sätt till.

- *Rutinerna direkt.*
  - `fprov` anropar FMUL och FDIV i den olappade och den lappade ROM:en
    med samma tal. Den jämför resultatets fem byte, A, F, BC, DE, HL och
    fel 4.
  - Talen är kanttal (exponent 1, 2, `$7F-$82`, `$FE`, `$FF`; mantissor som
    999999, 999995, 100001 och 500005), tal med en till tre siffror, a·a
    och a/a, i båda lägena av avrundningsflaggan.
  - Det blir 300 000 fall per rutin med `a_flt` och lika många med
    `a_alla` och ett annat frö, utan skillnader.
  - `mulprov` gör samma sak för `b_mul` mot MUL16: varje HL med steget
    1-16, 32, 64 och 256 åt båda håll, och 200 000 slumpade par. Det blir
    2,7 miljoner fall utan skillnader i HL och CY. Vid spill jämförs bara
    CY, som är det ARRELM kan få veta.
- *Bokens tolkprov.* `bokprov.sh` kör de 569 proven i
  `../../dis/04/host/tests` med `a_alla` och `b_alla` i stället för
  ROM:arna. 545 ger samma skärm. De 24 andra skiljer bara i det som
  beror på tiden:
  - 21 prov stannar efter ett fast antal instruktioner eller räknar
    dem, och den lappade ROM:en kommer längre eller behöver färre
    (FMUL, FDIV och andra);
  - `random2` har RANDOMIZE, som tar fröet ur R-registret;
  - `bii/step` trycker BREAK efter 100 ms, och `b_alla` hinner skriva
    ett tal till;
  - `iec/standby` trycker BREAK medan IEC-ROM:en väntar i WAITBO, och
    BREAK tappas i CHKBRK (se "Vad som kom fram på vägen"). Med 30
    olika väntetider tappas den en gång i den olappade ROM:en och fem
    gånger med `a_brk`. Det är samma kod i IEC-ROM:en, och var avbrottet
    hamnar i slingan beror på hur lång tid allt före tog.

  Inget prov ger en annan skärm av något annat skäl.
- *Kantfallen.* `kantprov.sh` kör 63 program i `kant/prov.txt` i båda
  tolkarna, olappade och lappade. Fallen täcker följande:
  - *FOR/NEXT:* uppåt och nedåt, lika gränser, steg 0 och stora steg,
    gränserna ±32767, nästlade FOR, NEXT utan variabel, NEXT utan FOR,
    variabeln eller gränsen ändrad i slingan, FOR i GOSUB och
    flyttals-FOR.
  - *Fält:* heltal, strängar, två index, index utanför, negativa och
    flyttalsindex.
  - *Resten av språket:* DEF FN, strängar, jämförelser, ON GOTO och
    BREAK i en slinga.
  - *Tal:* flyttal nära spill och underspill, a/a och 2^.5; heltal vid
    ±32767 och spill i multiplikation.
  - *DIM-felet* nedan.
  - *`V=V±X`:*
    - heltal och flyttal, konstanter och variabler;
    - satser som nästan stämmer (`A%=A%+1%+1%`, `A%=A%+1%*2%`,
      `A%=B%+1%`, `A%=A%+1`, `A%=A%+B%-A%`, `K=K+J-K`);
    - spill och underspill i satsen;
    - V=V+X som FOR-gräns och efter THEN;
    - två variabler med samma låga adressbyte eller referensbyte;
    - för BASIC II även små heltal mot INTCON (34 och 35), konstanter av
      olika längd och DOUBLE.

  Alla 126 körningar (63 fall i två tolkar) ger samma skärm med `a_alla`
  och `b_alla`. Varje BASIC II-lapp ger också för sig samma skärm i
  alla fall. `brk_slinga` (`10 I%=I%+1`, `20 GOTO 10`, BREAK efter en
  fast tid) kan skilja, eftersom den stannar på rad 10 eller rad 20
  beroende på var slingan är när BREAK kommer. Med 200 tider mellan 300
  och 500 ms stannar den olappade ROM:en på rad 20 två gånger och
  `a_alla` tre gånger. Resten av gångerna stannar den på rad 10, utom
  när BREAK tappas (se nedan). Provet visar alltså var tiden råkar
  hamna, och en snabbare tolk hamnar på ett annat ställe.

  Kantfallen i BASIC II väntar 1000 ms efter varje RETURN. abc80host
  matar en tangent var 100:e ms, och BASIC II kompilerar raden när
  RETURN kommer och tappar tangenter som kommer under tiden. Utan
  väntan blev raderna stympade och RUN försvann, så båda ROM:arna gav
  samma felutskrift och provet sade ingenting. Det gällde de första
  körningarna av `b_mul` och `b_nxt`, som är gjorda om.
- *Medvetna fel.* Lapparna har byggts med fel inlagda för att se att
  proven märker det. `felprov.py` gör om det för `a_sup` och `b_sup`;
  felen och förklaringarna står i skriptet. Flyttalslapparna prövas av
  `fprov`. Ett fel som inget prov märker är antingen ofarligt eller ett
  hål i proven, och varje sådant fel är förklarat ur listningen nedan.
  - *`a_var`.* Alla fem felen ger fel i 3-25 kantfall: utan FNPAR,
    GETF och GETI förväxlade, INC DE borta i båda grenarna, och C inte
    satt.
  - *`a_brk`.* Inget av de två felen ger något annat, och listningen
    visar varför:
    - BRKSTP är `RST $10` med kod 0. RST10 och ERROR skriver över HL
      och DE och tar raden ur CURLIN, så EX DE,HL före hoppet spelar
      ingen roll.
    - Fel 0 går alltid till READY, som anropar TSTBRK efter INLINE
      (`$00F0`), så BRKFLG nollställs där ändå.

    Båda instruktionerna är kvar, som i TSTBRK, men behövs inte.
  - *`b_nxt`.*
    - Utan JP PO-rättelsen eller utan bytet vid negativt steg ger fyra
      kantfall fel.
    - I flyttalsvägen ger tre fel var sitt utslag i tre kantfall
      (for_flyt, for_flyt2, for_bland): HL inte bytt mot DE, hopp förbi
      additionen (`$2865`), och IY i stället för IX.
    - Utan skrivningen till VFF36 märks ingenting. VFF36 skrivs på
      `$22D1` och `$2826` men läses ingenstans i ROM:en, så lappen skriver
      den bara för att ROM:en gör det.
  - *`a_tab`.*
    - Med B ≠ 0 blir det sex fel (funktioner, strängar).
    - Med CY = 1 märkte först inget prov något, eftersom alla
      subtraktioner i dem var flyttal (`A%-1` räknas med flyttal).
      `hel_d` och `hel_e`, med heltal mot heltal, lades till och fångar
      det.
    - C sätts av ROM:en men läses inte av någon av de 74 rutinerna i
      EXPTAB1. Ingen av dem hoppar heller på S, Z eller P innan den satt
      dem själv. Det är prövat i listningen, inte bara med proven.
  - *`b_mul`.* Utan CY vid spill märks felet inte, eftersom spill inte
    kan hända: indexet är redan prövat mot fältets storlek.
  - *`a_sup`.* 14 av 20 fel ger något annat i kantfallen:
    - bara den låga eller bara den höga adressbyten prövad;
    - utan prövningen av ASGNI eller ASGNF;
    - subtraktionen räknad som addition, och FSUB i stället för FADD;
    - DE inte flyttad efter satsen;
    - heltalskonstantens höga byte 0, och konstantens exponent en byte
      fel;
    - fyra byte i stället för fem i höger operand;
    - variabeln inte skriven tillbaka.

    De sex som inte märks förklaras av listningen:
    - Fyra tar bort prövningen av GETI eller GETF efter den första
      variabeln eller efter X. PRIMARY skriver alltid `$D4` eller `$D5`
      direkt efter en variabel i ett uttryck, så prövningen är ett skydd
      som aldrig slår till.
    - Två lämnar fel HL eller B efter satsen. Tilldelningen tas bara
      emot av LET, som går tillbaka till RUNSTM, och av FOR, som skriver
      över HL och B med nästa RST `$38`. Ingen läser dem.

    Båda sorterna är kvar, som i ROM:en.
  - *`b_sup`.* 18 av 25 fel ger något annat:
    - bara den låga eller bara den höga referensbyten prövad (heltal);
    - utan prövningen av B5 eller B4 (`A%=A%+B%-A%` och `K=K+J-K` har
      referensen efter operatorn men ingen tilldelning där);
    - subtraktionen som addition, och V och X i omvänd ordning;
    - DE fel efter satsen eller när mönstret inte stämmer;
    - fel i små heltal, INTCON och konstanternas längd till NUMPSH;
    - den höga byten inte skriven;
    - X läst som V;
    - PRGPTR inte satt;
    - spill utan fel 130.

    Sju märks inte:
    - B, C och HL fel efter heltalssatsen och VFF36 inte skriven. FOR
      går aldrig genom `$1D1A`, och satsslingan (L1D1D) skriver över B, C
      och HL innan någon läser dem. VFF36 läses ingenstans.
    - IX inte satt efter en konstant. Inget efter satsen läser IX innan
      det sätts.
    - En konstant med fel längd i prövningen. Då stämmer inte mönstret,
      och ROM:en gör satsen som förut. Det ändrar bara tiden: B2 går
      1,5 % långsammare i stället för 4,6 % fortare.
    - Bara den låga referensbyten prövad i flyttalsvägen. Det är inget
      fel. STFVAR läser målet ur koden, så `A1=Q1+1` med Q1 = `$1108` och
      A1 = `$0108` blir rätt (`sup_b2g`). Prövningen avgränsar bara
      mönstret.

## Hade en omskrivning lönat sig?

Frågan gäller ABC80. Omskrivningen finns redan att titta på: BASIC II
är ABC800:s tolk flyttad till ABC80. Den har binära flyttal, DOUBLE och
en annan kod, och den behöver 30 KB ROM i stället för 16.

*Vad mätningen säger.* Lapparna delar upp skillnaden i två delar.

- *Tolkningen.* Satser, variabler, heltal, slingor och fält. Här når
  den lappade ABC80-BASIC:en BASIC II, också när BASIC II lappas lika
  mycket:
  - NOEL1 6,42 mot 6,42 s;
  - B2 1,80 mot 1,73;
  - FIZZ1 1,35 mot 1,47;
  - SIEVE 20,9 mot 25,4.

  En ny tolk med en annan kod hade inte gett mycket mer än lapparna. Det
  som återstår är 34-104 T när mönstret inte stämmer och de 135 T per
  uttryckskod i EXEXP. En ny kod kunde ta bort det, till exempel med
  adresser i stället för koder eller med `V=V+X` som en egen kod.
- *Talen.* Här ligger BASIC II kvar långt före:
  - B8 11,3 mot 2,9 s;
  - B4, B5, PRIMES och MANDEL 1,5-1,6 gånger.

  Lapparna gör BCD-rutinerna 8-22 % snabbare, men BCD-division och
  -multiplikation siffra för siffra är långsammare än binär aritmetik.
  Det är bara en ny talrepresentation som tar bort skillnaden.

*Vad en omskrivning hade kostat.*

- *Programmen.* SAVE skriver den kompilerade koden, en BAC-fil. Med en
  ny kod går ingen sparad BAC-fil att läsa. BAS-filer, alltså program
  som text, går att läsa in och kompilera om. Med en ny talform ändras
  också talen i BAC-filernas konstanter.
- *Tilläggen.* Tilläggs-ROM:arna anropar ROM:en på fasta adresser.
  DOS-ROM:arna (abcdos80, ufd80) anropar hoppvektorerna `$000B-$0044` och
  tre inre rutiner (`$0727`, `$0901` och LNKSRC `$1074`). SuperBASIC och
  Schoolaid anropar 44 adresser var inne i ROM:en (räknat i listningarna
  i `../../dis/build`). Maskinkodsprogram med CALL gör detsamma. En ny
  tolk måste ha kvar varje sådan ingång, med samma register in och ut,
  annars måste tilläggen skrivas om. Lapparna lämnar ingångarna där de
  var.
- *Talen och utskriften.* BCD räknar decimalbråk som 0,1 exakt, och
  binära flyttal gör det inte. En ABC80 med binära flyttal skriver ut
  andra siffror i en del program. Program som jämför tal med = kan ta en
  annan väg, till exempel FOR med STEP 0.1 eller `IF X=0.3`. Det är en
  ändring av språket, inte av tolken.
- *Utrymmet.* 16 KB ROM är nästan fullt. Lapparna behöver redan en
  krets till på `$5000` för 730 byte kod och 512 byte tabeller. BASIC II
  behöver 30 KB. En tolk med binära flyttal och samma språk i 16 KB hade
  krävt att något annat togs bort. 1978 kostade en ROM-krets till
  pengar i varje maskin.
- *Att pröva den.* Varje lapp här är prövad mot den olappade ROM:en.
  Svaret ska vara exakt detsamma, så varje kantfall, tolkprov och slumpat
  tal har ett facit. En ny tolk har inget facit utom manualen och
  programmen. Bokens bilaga "Fel och egenheter i BASIC II" har 67 poster
  för en tolk som ändå var prövad i ABC800.

*Avvägningarna.* Om ABC80-BASIC:en hade fått en omgång till, hade det
lönat sig att göra som här, men från början och i ROM:en:

- Behåll ingångarna, BAC-koden och BCD-formatet.
- Ta bort prövningarna i satsslingan (`a_brk`, 3,5-7,3 % för 8 byte).
- Gör variabeln och värdet till en kod (`a_var`, upp till 9,8 % för 35
  byte).
- Känn igen `V=V+X` (`a_sup`, upp till 23 % för 297 byte).
- Skriv BCD-divisionen med HL och DE (`a_fdiv`, upp till 22 % för 72
  byte).

Det är 412 byte för det mesta av vinsten. Tabellerna i `a_tab` (512
byte för 0,5-2,4 %) och bytet i `a_fswp`, som beror på programmet, hade
fått stå tillbaka. Det enda som en hel omskrivning hade köpt utöver det
är flyttalen. Det priset betalade BASIC II: en annan kod, andra tal, ett
nytt DOS-gränssnitt och dubbelt så mycket ROM. För en maskin som redan
hade sina program, tillägg och användare var det en ny maskin, inte en
ny omgång.

## Vad som kom fram på vägen

- *DIM-felet i ABC80:s ROM.* När CMPARG går fel i CMPDMI (`$1BD5`,
  t.ex. `DIM A` utan index eller `DIM B$(1,2,3)`) hoppar den till
  CMPERR med AF från `$1BD1` kvar på stacken. POP BC tar AF i stället för
  BC, och RET hoppar till VARNAM:s resultat: bokstaven (`$41-$5D`) som hög
  byte, siffran och typen som låg.
  - På en ABC80 utan något på `$4100-$5DFF` läses `$FF` där. Det blir RST
    `$38`, och felet blir ERR 4 av en slump. En tilläggs-ROM med kod på de
    adresserna kör den koden.
  - Den första kretsen låg på `$4000`, och tabellerna på `$4100` och `$4200`
    gav ERR 5 och ERR 14 i bokens prov dim4.
  - Kretsen ligger nu på `$5000` med lapparna i luckorna `$x93-$xEF`, och
    `lappa.py` vägrar bygga om en kod hamnar på en adress som felet kan
    hoppa till.
- *`$7800-$7FFF` är inte ledigt i BASIC II.* Området är `$FF` i ROM-filen,
  och lapparna låg där först. Men i maskinen är `$7800-$7BFF` RAM för
  skivenheternas DOS, med enhetsposter och buffertar, och `$7C00`- är
  skärmen. abc80host gör likadant med `-F`. Där skrev bokens prov sina
  filbuffertar över lappen, och 13 BASIC II-prov gav fel. Lapparna
  ligger nu i nollorna efter kassettdrivrutinen (`$044A-$06A9`), och
  `lappa.py` vägrar lägga dem någon annanstans.
- *BREAK som tappas i IEC-ROM:en.* CHKBRK (`$7028`) läser BRKFLG, prövar
  den och skriver 0. Kommer tangentavbrottet mellan läsningen och
  skrivningen försvinner BREAK. TSTBRK (`$033E`) i ABC80:s ROM gör
  likadant. Den olappade ROM:en tappar också BREAK med vissa
  instruktionstakter i emulatorn; lappen flyttar bara var det händer.
- *Heltal som slår runt.* I båda tolkarna blir `FOR I%=... TO 32767`
  aldrig klar: I% går från 32767 till −32768. ABC80 multiplicerar heltal
  utan att märka spill (182·182 blir −32412). BASIC II:s IADD och ISUB
  slår runt utan fel.
- *Nollan med teckenbyte.* När FMUL får 0 genom underspill blir
  teckenbyten XOR av två byte som råkar ligga i minnet. Den olappade
  ROM:en ger därför olika teckenbyte mot sig själv. FCMP ser bara på
  exponenten när den är 0, så en jämförelse ser det inte.
- *BASIC II:s referenser.* En variabels referens i koden (VARREF,
  `$0F63`) är inte ett löpnummer. Den beror på namnet: i ett av
  kantfallen har A1 referensen `$0108` och Q1 `$1108`. Två variabler med
  samma siffra delar därför låg byte, och det är så `sup_b2g` prövar
  referensjämförelsen.

## Inte prövat

- *Flyttalen i BASIC II.* FDIV och FMUL är 17 och 13 % av B4. De är
  redan binära och skrivna med register, och tyngdpunkten här är ABC80.
- *En ny BCD-algoritm i ABC80.* Lapparna skriver om de gamla rutinerna
  med andra register och tabeller. En annan divisionsmetod är inte
  prövad.
- *En riktig ABC80.* Tiderna är emulatorns. Bara BASIC II är jämförd
  med en riktig maskin (ABC 802).
