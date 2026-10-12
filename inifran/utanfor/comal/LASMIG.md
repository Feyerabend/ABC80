# COMAL 0,5: ABC80-BASIC med COMAL:s strukturer (utanför boken)

*Inte med i boken.* Ett kontrafaktiskt försök. Antag att ABC80-BASIC:en
(1978) hade fått COMAL:s styrstrukturer i stället för att ersättas av
BASIC II: hur mycket hade det kostat i bytes och i tid? Försöket bygger
strukturerna i en lappad kopia av den gamla ROM:en (abc80old) och en
krets på `$4000`, och mäter dem i emulatorn mot GOTO och GOSUB och mot
BASIC II:s WHILE/WEND. Bokens filer och ROM:arna i `emulator/roms/`
ändras inte. Bakgrunden, COMAL och skoldatorn Compis, står i
`BAKGRUND.md`.

## Det viktigaste

- *Strukturerna ryms i 2215 bytes plus 23 bytes i ROM:en.* De är WHILE,
  REPEAT, block-IF med ELIF och ELSE, PROC/EXEC med parametrar och
  LOCAL, CASE/WHEN/OTHERWISE och LIST med indrag. BASIC II behövde 30 KB,
  men gav också långa variabelnamn, binära flyttal och mycket annat.
- *Gamla program går som förut.* De 16 proven ur `../tolkjamforelse`
  ger samma skärm, samma fel och samma tal. Varje RUN tar omkring 450 T
  längre, för blocken länkas före körningen. Själva körningen går lika
  fort.
- *Strukturerna kostar lite där de ersätter GOTO.* WHILE är 5 % långsammare
  än samma slinga med IF/GOTO, REPEAT 3 %, block-IF 2,5 % och CASE
  4,4 % mot en kedja av IF. BASIC II:s WHILE/WEND kostar 4 % mot dess
  egen GOTO-slinga.
- *Procedurerna kostar mer.* EXEC med en parameter tar 35 % längre än
  GOSUB med en tilldelning, och LOCAL lägger till en fjärdedel till.
  Där byts variablernas värden på stacken, både in och ut.
- *CASE räknar väljaren en gång.* Den ligger sedan på stacken, och
  varje WHEN-värde räknas direkt ur programmet och jämförs med den: två
  bytes för heltal, ROM:ens FCMP för flyttal, längden och tecknen för
  strängar. Det första försöket byggde koden "väljaren värdet =" på
  stacken för varje värde och var 18 % långsammare än IF-kedjan.
- *Fel i strukturen syns före körningen.* Blocken paras vid RUN, som
  FOR och NEXT, och ett block som aldrig stängs ger fel 70 innan första
  raden körs.

## Förutsättningar

- *Inte COMAL.* Det är ABC80-BASIC med COMAL:s strukturer. Radnumren,
  GOTO och GOSUB, alla gamla satser, de korta variabelnamnen och
  BCD-talen står kvar. Långa variabelnamn, FUNC på flera rader och
  REF-parametrar är inte med. Strängar går inte som parametrar.
- *Emulatorn, inte en riktig maskin.* Tiden är emulatorns T-cykler vid
  2,9952 MHz, från CR efter `RUN` till `READY` (`$00CC` i ABC80, `$0110` i
  BASIC II), mätt med `../tolkjamforelse/prof`. ABC80 har 16 KB RAM
  och BASIC II 32 KB.
- *Platsen.* Kretsen ligger på `$4000-$48A6`, där Superbasic låg
  (`$4000-$57FF`). ROM:en får bara hoppen dit: KWSRCH letar i kretsens
  ordlista, PRELNK länkar blocken och LIST drar in. Det är en krets till
  i maskinen, som i tolkjamforelse.
- *Bara det som gick 1978.* Det är vanlig Z80-kod med ROM:ens egna
  rutiner för uttryck, tal och fel. Ingen ny algoritm används.

## Filerna

| Fil          | Vad                                                          |
|--------------|--------------------------------------------------------------|
| `comal.asm`  | lapparna i ROM:en och kretsen, med satsernas form överst      |
| `bygg.py`    | assemblerar och skriver `ut/comal.rom` och `ut/comal.krets`  |
| `kor.c`      | värd på emulatorns kärna: skriver in rader ur en fil, mäter RUN |
| `prov.sh`    | bygger, kör `prov/*.txt` mot `prov/*.ut` och tolkjamforelse-proven mot abc80old |
| `matt.sh`    | mätningen: `matt/*.BAS` i alla tre tolkarna, `*.CML` i COMAL-ROM:en, `*.B2` i BASIC II |
| `BAKGRUND.md` | COMAL:s historia, Compis och ABC 80, med källor            |

Programmen byggs med emulatorns kärna (`../../emulator/karna`) och
lapparna med `../../dis/build/z80asm`.

## Satserna

| Sats                         | Fel                                       |
|------------------------------|-------------------------------------------|
| `WHILE v` ... `ENDWHILE`     | 70: blocket stängs aldrig                 |
| `REPEAT` ... `UNTIL v`       | 71: en stängare utan öppnare, eller av fel slag |
| `IF v THEN` ... `ELIF v THEN` ... `ELSE` ... `ENDIF` | |
| `PROC namn(a,b%)` ... `ENDPROC`, `EXEC namn(x,y%)` | 72: PROC saknas, fel antal eller typ, LOCAL på fel ställe |
| `LOCAL a,b%`                 |                                           |
| `CASE v OF` ... `WHEN a,b` ... `OTHERWISE` ... `ENDCASE` | 73: ingen WHEN stämmer; 17: sträng mot tal |

Ett IF-block är ett IF med ingenting efter THEN; med en sats efter THEN
är det ett vanligt IF, som förut. LOCAL ska stå först på raden efter
PROC. Parametrarna och LOCAL har grund bindning: de gamla värdena läggs
på stacken vid EXEC och tillbaka vid ENDPROC, så variablerna behåller
sina adresser och rekursion fungerar. Typen ska stämma redan i EXEC
(5 till en N%-parameter är fel 72; skriv 5%).

## Hur det är byggt

En sats som behöver en adress får en eller två platser, `$B9 lo hi`,
i koden. Kompilatorn skriver tomma platser. Vid RUN går PRELNK igenom
programmet med en stack av öppna block (toppen på EOFA+1) och skriver in
var nästa del står, som den gör för FOR och NEXT. Under körningen följer
satserna platserna och behöver ingen stack. Därför kan GOTO hoppa ut ur
WHILE, REPEAT, IF och CASE utan att något blir kvar. PROC lägger
återhoppet på stacken som GOSUB.

Kretsens 2215 bytes:

| Del                                      | Bytes |
|------------------------------------------|------:|
| ordlistan och tabellerna (med ROM:ens tabell 2 kopierad) | 369 |
| kompileringen                            | 313   |
| länkningen vid RUN                       | 574   |
| körningen (varav CASE och WHEN 317)      | 725   |
| LIST med indrag                          | 234   |

## Tiderna

Sekunder från RUN till READY. Programmen står i `matt/`. S-programmen
räknar till 10000 med heltal, A-programmen lägger ihop 1..2000 genom
ett anrop, och V-programmen väljer mellan fyra fall 2000 gånger.

| Prov                       | ABC80 | COMAL-ROM:en | BASIC II |
|----------------------------|------:|-------------:|---------:|
| SFOR (FOR/NEXT)            |  3,01 |  3,01 |  2,90 |
| SGOTO (IF ... THEN 20)     | 11,52 | 11,52 |  9,35 |
| SWHILE                     |       | 12,10 |  9,76 (WEND) |
| SREPEAT                    |       | 11,91 |       |
| AGOSUB (X=I, GOSUB)        |  6,94 |  6,94 |  5,51 |
| AEXEC (EXEC ADD(I))        |       |  9,36 |       |
| ALOCAL (med LOCAL T)       |       | 11,76 |       |
| VIF (IF ... : GOTO)        | 27,30 | 27,30 | 14,75 |
| VBLOCK (IF/ELIF/ELSE)      |       | 27,98 |       |
| VCASE (CASE/WHEN)          |       | 28,50 |       |

VIF går nästan dubbelt så fort i BASIC II för att `I-INT(I/4)*4` räknas
med binära flyttal, inte för att IF är snabbare (se tolkjamforelse).

### Det som inte är självklart

- *ENDWHILE räknar WHILE:s villkor själv.* Det första försöket hoppade
  tillbaka till WHILE, och då var WHILE 14 % långsammare än GOTO (13,1 s).
  Nu går ENDWHILE till satsen efter WHILE när villkoret är sant och
  vidare utan hopp när det är falskt: 12,1 s.
- *Platsen först.* I CASE, WHEN och ELIF står platsen före uttrycket.
  När en del följer kedjan till nästa del behöver den då inte gå förbi
  villkoren och värdena. Med platsen efter tog VCASE 37,0 s och VBLOCK
  29,6 s.
- *CASE utan kopior.* Det första CASE byggde koden "väljaren värdet ="
  på stacken för varje värde. Det krävde uttryckens längder, och ENDTOK
  gick igenom uttryckskoden för att hitta dem varje gång (32,1 s).
  Längderna kunde ha skrivits in vid länkningen, men det behövs inte
  när väljaren räknas en gång och värdet räknas där det står: `RST $38`
  lämnar DE efter värdet (28,5 s). Ett heltal och ett flyttal jämförs
  genom att heltalet görs till flyttal (INTTOF) i fem bytes i CASE:s
  ram. En sträng som räknas fram (`CHR$`, +) ligger kvar på stacken tills
  den är jämförd. prov/case2.
- *CASE:s rest.* Ett flyttalsvärde i WHEN slutar med `$B9`, och ENDF
  flyttar då återhoppet under talet; IF:s jämförelse slutar med ett
  heltal. Det är det mesta av de 4,4 %. Snabbare än IF-kedjan blir
  CASE när väljaren är ett längre uttryck, som räknas en gång i stället
  för en gång per IF. En hopptabell för heltalsvärden vore snabbast
  men kräver att alla värden är konstanter.
- *TRACE följer hoppen.* Ett hopp till en annan rad går genom samma väg
  som RUNLIN, så TRACE skriver radnumret som för GOTO.

## Vad som kom fram på vägen

- *ROM:ens tabell 2 slutar på `$95`.* STKW2, STTAB2 och STEXE2 har 22
  satser, `$86 $80` till `$86 $95`, men koden räcker till `$FF` om
  tabellerna flyttas. Kretsen har
  kopior av dem med de nya satserna efter, och ROM:en pekar dit.
- *DIM-felet.* CMPDMI hoppar vid ett fel in i `$41xx-$5Dxx` med AF kvar på
  stacken (se `../tolkjamforelse/lappa.py`). Här lagas det med DIMFIX, så
  kretsen behöver inga förbjudna bytes.
- *LIST pausar efter 22 rader.* Proven listar därför bara korta program.

## Inte prövat

- En riktig ABC80.
- BAC-filer med de nya satserna. Platserna skrivs om vid varje RUN, så
  de borde gå.
- LIST till en fil (indraget räknas, men bara skärmen är prövad).
