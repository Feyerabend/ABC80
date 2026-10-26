# Forthaid: en öppen Smartaid i Forth (utanför boken)

**Inte med i boken.** Ett försök att pröva vad band 2 säger i slutet av
exempel 7: att en Forth på `$4000` hade varit en annan väg än Smartaids.
Forthaid är en verktygskrets i stil med Smartaid 3, skriven i Forth på
tolken från exempel 7 och byggd för `$4000`–`$5FFF` (8 KB, utan
bankning). Till skillnad från Smartaid är den öppen: kommandona är
Forth-ord, och användaren kan skriva egna. Hur den används står i
`MANUAL.md`.

Ingen kod är lånad från Smartaid. Kapitlet om Smartaid i band 2 är bara
använt för att veta *vad* kommandona gör och hur kretsarna tar över
tangentbordet.

| Fil            | Vad                                                               |
|----------------|-------------------------------------------------------------------|
| `gor.sh`       | bygger `forthaid.bin` ur kopior av bokens källor; `-p` kör proven |
| `fa.fs`        | kommandona i Forth, inkopplingen (`PA`) och radläsningen (`KBD`)  |
| `fak.inc`      | de nya maskinkodsorden                                            |
| `fai.asm`      | tangentbordets avbrott och stegningen, i assembler                |
| `MANUAL.md`    | manualen: inkopplingen, kommandona, editorn, stegningen och Forth |
| `prov/`        | 25 prov, vart och ett i 9913 och 11273 (`prov/kor.sh`)            |
| `matning.sh`   | tiderna för FIND, VAR och REN, och storleken per kommando         |
| `forthaid.bin` | kretsen, 8 176 byte                                               |


## Så används den

```
    Z=CALL(16384)       kommandona i Forth-tolken (ingång 0, FORTH)
    Z=CALL(16384,1)     samma, med de egna ord som redan finns
    Z=CALL(16384,2)     kopplar in kretsen: kommandona direkt i BASIC
```

Efter `Z=CALL(16384,2)` läser kretsen raderna i direktläge med en egen
skärmeditor. En rad som börjar med något av kommandona körs av kretsen,
och andra rader går till ROM:en som förut. Under körning ger CTRL-C en
paus, och CTRL-T och CTRL-S stegar programmet. `EX` kopplar ur kretsen.

I emulatorn laddar `emulator/abc80` och `emulator/abc80f` kretsen med
`-l utanfor/forthaid/forthaid.bin@4000`, och webbsidan har den bland
kretsarna (`?krets=forthaid`). På en riktig ABC80 är den inte prövad.

Kommandona: SYS, LIST, PEEK, FIND, VAR, DEL, AUTO, OLD, ED, START,
CHANGE, REN, HELP, FORTH och EX; i Forth-tolken dessutom WORDS, BYE och
de egna orden. Varje kommando, editorns tangenter, stegningen och hur
man skriver egna ord står i `MANUAL.md`.


## Vad som är ändrat mot boken

`gor.sh` kopierar bokens `e7k.inc`, `e7fc.py`, `e7.fs` och `e7i.fs`
till `bygg/` och bygger där; bokens filer ändras aldrig. I kopiorna:

- bokens exempelord (`MAIN`, `TIO`, `FIND` med `MATCH` och `LINE?`) och
  ingångarna tas bort; `fa.fs` har egna, med FORTH som ingång 0;
- kärnans `BYE` heter `(BYE)`, så att `BYE` i `fa.fs` kan gå tillbaka på
  två sätt: som förut, eller till ROM:ens programslut (`ENDPRG`, CLEAR
  och ABC80) när ett kommando har ändrat programmet, eftersom raden med
  `CALL` och dess variabel då kan vara borta;
- kärnans `(LOOKUP)` tas bort; `fak.inc` har ett eget som inte skiljer
  på versaler och gemener (2026-10-26), så att `list`, `List` och
  `LIST` är samma kommando, både i BASIC och vid OK. Det som står efter
  kommandot ändras inte (`find a` söker efter ett litet a). Kretsen blev
  17 byte större, och stegningens prov hinner ett varv kortare
  (`steg.out`, `steg2.emu.out`); `prov/gemen` prövar det nya.

Nya maskinkodsord i `fak.inc`: `*`, `U/MOD`, `DETOK` (ett nyckelord ur
ROM:ens tabell), `MATCHVAR` (nästa förekomst av en variabel i en rad),
`SETVARS` (radens variabler i VAR:s tabell), `SEARCH` (en text i en
annan, för FIND), `J`, `CMOVE`,
`ROMCALL`, `ENDPRG`, `(INS)` (en rad in i programmet), `I@` och `I!`
(I-registret), `TOROM` (tillbaka in i kommandoloopen), `CMD?` (är ordet
ett kommando), `(LOOKUP)` (ordlistan utan skillnad på versaler och gemener, i
stället för kärnans) och `EDIT` med skärmeditorn `EDLIN`. Tangentbordets
avbrott och stegningen är skrivna i assembler i `fai.asm`, som läggs
sist; vektortabellen måste ligga på `$5F34` (I = `$5F`), vilket
`gor.sh` prövar.

RAM:

| Var             | Vad                                                            |
|-----------------|----------------------------------------------------------------|
| `$7C78`–`$7EFE` | tolkens och kommandonas variabler, i bildminnets osynliga byte |
| `$7F7E`         | `STEPF`: skilt från 0 medan CTRL-T eller CTRL-S hålls ned      |
| `$FB00`–`$FB4F` | radbufferten                                                   |
| `$FB50`–`$FC7F` | de egna orden, 304 byte                                        |
| `$FC80`–`$FCFF` | stackarna medan kretsen läser en rad och kör ett kommando      |

`$FB00`–`$FCFF` är kassettens blockbuffertar, som ROM:en bara använder
medan en fil läses eller skrivs. Stackarna (`KBDSP` = `$FD00` i
`fai.asm`) används bara medan kretsen läser en rad och kör ett av sina
kommandon, och då läses eller skrivs ingen fil; går raden till ROM:en
är de lediga. Från början låg de under BASIC:s SP, och när programmet
fyllde minnet skrev de över dess slut (provat med ett program på 15 KB:
`STACK ?` om och om igen). Sedan tog `PA` 128 byte från BASIC under
STKTOP, som `EX` gav tillbaka. Nu lånar kretsen ingenting av BASIC: med
och utan kretsen tar programmet slut på samma adress (`$FA92` med
raderna `N A=1`), och LIST, FIND, VAR (`?`), REN och SYS fungerar
med fullt minne. Med 128 byte är det marginal: på programmet om 12 KB
gick returstacken som djupast 20 av 64 byte (LIST, VAR och de flesta
kommandon) och datastacken 46 av 62 (FIND, och LIST med 44), mätt
genom att fylla området med `$AA` före kommandot. I `gor.sh` prövas
att ingen variabel hamnar på `$7F7E`.


## Uppskattning mot mätning

`./matning.sh`, 2026-10-20, i bokens emulator med 9913. Tiderna är den
emulerade maskinens, i ms, för ett program med programmets längd
utöver ett med två rader (se huvudet i `matning.sh` för hur de mäts).
FIND letar efter en variabel som bara står på den sista raden, VAR utan
namn visar alla variabler (81 stycken), VAR Z9 visar raden där Z9 står
(bara i Forthaid), REN 10,10 numrerar om allt och CHANGE byter B1, som
står på var åttonde rad, mot Q1 (`CHANGE B1 TO Q1` i Smartaid 3).

| Program  | Kretsen    | FIND Z9 | VAR    | VAR Z9 | REN 10,10 | CHANGE B1 |
|----------|------------|---------|--------|--------|-----------|-----------|
| 2 060 B  | Forthaid   | 0,35 s  | 0,60 s | 0,18 s | 0,41 s    | 0,18 s    |
|          | Smartaid 3 | 0,08 s  | 0,21 s | –      | 0,23 s    | 0,14 s    |
| 8 180 B  | Forthaid   | 1,50 s  | 1,05 s | 0,74 s | 2,21 s    | 0,72 s    |
|          | Smartaid 3 | 0,42 s  | 0,74 s | –      | 1,60 s    | 0,62 s    |
| 12 266 B | Forthaid   | 2,29 s  | 1,36 s | 1,12 s | 3,90 s    | 1,09 s    |
|          | Smartaid 3 | 0,66 s  | 1,10 s | –      | 3,00 s    | 0,93 s    |

Forthaid är alltså 3,5–4,5 gånger långsammare med FIND, 1,2–3 gånger
med VAR, 1,3–1,8 gånger med REN och 1,2–1,3 gånger med CHANGE. Skillnaden
krymper med programmets längd, eftersom en del av den är fast. REN går
fortare än FIND, eftersom det mesta arbetet görs av ROM:ens egna
rutiner, LNKREF och REN:s tredje varv.

FIND var först 10 gånger långsammare (0,98, 4,19 och 6,37 s). Den gör
varje rad till text med `DETOK` (ROM:ens tabell, i maskinkod) och
jämförde sedan texten med det sökta i en `DO`-slinga i Forth
(`CONTAINS`, som anropade `SAME` en gång för varje läge i raden). Nu gör
maskinkodsordet `SEARCH` hela jämförelsen, och FIND blev 2,8 gånger
snabbare och 23 byte kortare. Det som är kvar är mest `DETOK` och
slingan över raderna.

VAR var först 15 gånger långsammare (3,06, 10,77 och 16,30 s): för
varje förekomst letade den igenom en tabell över de namn den redan hade
sett (`SEEN?`, i Forth), så tiden växte med antalet förekomster gånger
antalet namn. Nu har varje tänkbar variabel en bit i en tabell på
heapen: 26 bokstäver, 11 siffror (ingen och 0–9), 3 typer och med eller
utan index, 1 716 bitar i 215 byte. Maskinkodsordet `SETVARS` går
igenom en rad och sätter biten för varje variabel, som räknas ut ur
namnbyten och bokstaven, så att varje förekomst kostar lika lite
oavsett hur många namn det finns. Sedan skrivs tabellen ut i bitarnas
ordning, med tomma byte överhoppade, och listan kommer sorterad efter
bokstav, siffra, typ och index utan någon sortering (A, A(, A1 i stället
för som förut i den ordning de först stod). Tabellen behöver 256 byte
mellan HEAP och STKTOP; annars svarar VAR `?` (prov/var2).

CHANGE och VAR med namn gick igenom raden med `NEXTVAR`, som gav
tillbaka varje variabel till Forth, där `VMATCH?` jämförde den med den
sökta. Nu letar maskinkodsordet `MATCHVAR` upp nästa förekomst av den
sökta variabeln direkt, och båda blev omkring 25 procent snabbare (1,47
s till 1,10 s på 12 KB). `MATCHVAR` och `SETVARS` går igenom raden med
samma rutin, `NVSUB`, som hoppar över elementen och uttryckskoden med
ROM:ens SKPELM och SKPEXP. Den genomgången är nu det mesta av tiden: VAR
Z9 och CHANGE tar ungefär lika lång tid och VAR utan namn bara lite
längre, och Smartaid 3:s VAR tar ungefär den tiden.

Storleken per kommando (`matning.sh`; ett ord räknas till det kommando
som ensamt använder det, också maskinkodsorden i `fak.inc`):

| Del                          | Byte  |
|------------------------------|-------|
| SYS                          |   194 |
| LIST                         |    96 |
| PEEK                         |   433 |
| FIND                         |   255 |
| VAR                          |   513 |
| DEL                          |   129 |
| AUTO                         |   145 |
| OLD                          |   322 |
| CHANGE                       |   295 |
| REN                          |   445 |
| ED                           |   119 |
| START                        |    56 |
| HELP                         |   159 |
| EX, BYE                      |    65 |
| ord som flera kommandon använder     | 1 791     |
| inkopplingen (`PA`, `KBD`)   |   261 |
| avbrottet och stegningen (`fai.asm`) | 212       |
| bokens tolk, utan exempelorden       | 2 611     |
| ledigt före vektortabellen på `$5F34` | 75       |
| **summa**                            | **8 176** |

Före ED och START räknades editorn (`EDIT`) till inkopplingen, som då
var 500 byte, och `+V` till REN; nu delar ED och START dem med `KBD` och
REN. ED blev 119 byte och START 56, med maskinkodsordet `(START)`;
uppskattningen var 40–70 byte för ED, men `LN?` (raden med ett visst
nummer) och huvudena kom till.

Den första versionen av `matning.sh` räknade nio av maskinkodsorden
(`*`, `U/MOD`, `DETOK`, `J`, `ENDPRG`, `(INS)`, `I@`, `I!`, `CMD?`) till
tolken, eftersom de heter något annat i listningen; tolken är 2 672
byte, inte 2 958.

Mot uppskattningen före bygget (2026-10-20):

|                                    | Uppskattat              | Mätt                                                                          |
|------------------------------------|-------------------------|-------------------------------------------------------------------------------|
| ROM, allt                          | 6–8 KB                  | 8 084 byte, med editorn och stegningen                                        |
| tolken                             | 3 167 byte              | 2 672 byte (utan bokens exempelord)                                           |
| kommandona i trådad kod            | 3–4 KB                  | 4 939 byte, med de delade orden och maskinkodsorden (4 508 före ED och START) |
| fast RAM                           | 150–300 byte            | de osynliga byten i bildminnet (som förut), och kassettens buffertar, där också stackarna ligger medan kretsen är inkopplad |
| FIND över 8–10 KB                  | 0,5–1 s                 | 4,2 s (8 KB), 1,5 s med `SEARCH`                                              |
| REN, ett stort program             | några sekunder          | 3,9 s (12 KB)                                                                 |
| Forth mot assembler i byte-slingor | 5–10 gånger långsammare | FIND 10 gånger, 3,5–4,5 med `SEARCH`; VAR 15 gånger, 1,2–3 med tabellen och `SETVARS`; CHANGE 1,2–1,3 med `MATCHVAR` (mot Smartaid 3) |

Tiden för FIND var alltså uppskattad 4–8 gånger för kort. Uppskattningen
räknade med en sökning byte för byte i programmet, men FIND söker i
raden så som den listas och gör om varje rad till text först.

Smartaid 3 är också 8 KB, men har plats för arton kommandon och en
skrivarrutin. Forthaid saknar RESUME, DIR, LIB, `FOR` i direktläge och
PR:; i stället har den VAR med namn och en tolk där
kommandona går att bygga ut.


## Begränsningarna

- **OLD** kräver att kretsen startas med `PRINT CALL(16384)`:
  `Z=CALL(16384)` lägger Z mitt i den första raden, som OLD ska hämta
  tillbaka. Efter inkopplingen (`Z=CALL(16384,2)`) gäller det inte, för
  då körs OLD direkt.
- **CHANGE** byter inte parametrarna i `DEF FN`.
- **Ingen automatisk start.** Smartaid 3 kopplar in sig själv genom
  DOS-kretsens plats för kallstart (`$404B`); Forthaid har ingen sådan
  ingång, så kretsen kopplas in med `Z=CALL(16384,2)`.
- **Kommandoloopen känns igen på stacken.** Avbrottet tar över bara när
  `$FE24` är 1 och de fyra byten under STKTOP är `$FE40` och `$00F0`, som
  ROM:ens kommandoloop lägger dit innan den anropar INLINE. En annan
  ROM-version med en annan återhoppsadress känns inte igen; 9913 och
  11273 har samma.
- **Pausen.** Tangenten som avslutar pausen går inte vidare till
  programmet (till exempel till `GET`); kretsen väntar tills den släpps,
  så att den inte ger en ny paus.
- **304 byte till egna ord**, `$FB50`–`$FC7F` (`TAK`, som SYS visar
  under ORDEN). Kompilatorn prövar inte gränsen; ord som går förbi den
  skrivs över av stackarna när kretsen är inkopplad (som de tidigare
  skrev över systemvariablerna på `$FD00`).
- **`STEPF` på `$7F7E`–`$7F7F`**, de sista osynliga byten i
  bildminnet. Ett program som själv använder dem (som Luxors
  skrivarkrets använder andra) krockar med kretsen.
- **START** behåller variablerna bara så länge programmet inte är
  ändrat: ROM:en nollställer dem när en rad läggs in eller tas bort, och
  när programmet tar slut (som efter `RUN`).
- **ED** ger ROM:en raden bara fram till markören; det som står efter på
  skärmen måste tas med högerpilen, som mallen med TAB i ROM:ens ED.
- **8 101 av 8 192 byte** är använda: 75 byte är lediga före
  vektortabellen och 16 efter `fai.asm`. Det som ska läggas till måste
  rymmas där eller ersätta något.


## Ett fel i stegningen

Med ED och START flyttades tiderna i provet `steg` en aning, och i 11273
kom CTRL-C i pausen aldrig fram: programmet gick vidare i stället för
att stanna med STOP. Felet fanns redan förut, vid andra tider; med
hålltiderna 40–200 ms för CTRL-S föll 3–4 av 161. Släpps tangenten mitt
i ROM:ens TSTBRK (`$033E`), mellan läsningen av BRKFLG och nollningen,
skriver TSTBRK efter pausen över den flagga som CTRL-C har satt. Nu
prövar pausen återhoppet (`PSBRK` i `fai.asm`, 22 byte före
vektortabellen): ligger det i det fönstret börjar TSTBRK om. Med det
stannar programmet vid alla 161 hålltider i båda ROM:arna. I 11273
hinner programmet nu ett varv mindre (`I` blir 309), så `steg` har en
egen utdatafil där.


## Proven

`prov/kor.sh` kör varje prov i 9913 och 11273 och jämför skärmen med
den sparade. Stegningen behövde två saker som emulatorerna inte hade,
och båda fick dem: bokens värdemulator (`dis/04/host`, där proven först
kördes) och emulatorn i `emulator/`, vars kärna också SDL-fönstret och
webbsidan har. Deras egna prov går som förut.

- PIO A:s avbrott följer bit 5 i styrordet: `$B7` ger avbrott när
  tangenten trycks ned, som ROM:en ställer det, och `$97` när den
  släpps, som stegningen ställer det;
- `\hN` i `-k`: så länge hålls nästa tangent nere, i ms.

Finns inte värdemulatorn kör `kor.sh` proven i `emulator/abc80`, med
ROM:arna i `emulator/roms` (hämtas med `hamta.py` där). Den räknar
tiden i T-cykler och värdemulatorn i instruktioner (6 000 per 20 ms), så
programmen i `steg` och `steg2` hinner olika långt; de har egna utdata, `steg.emu.out`
och `steg2.emu.out`. Alla 25 prov går i båda emulatorerna.


## Slutsatserna

Det går. En Smartaid i Forth får plats på `$4000`–`$5FFF`, med samma
sätt att ta över tangentbordet, en skärmeditor och stegning, och den är
öppen: kommandona är ord i en tolk som användaren kan lägga till egna ord
i. Påståendet i slutet av exempel 7 håller alltså i den meningen att
vägen fanns.

Priset är plats och tid. Tolken tar omkring 2,7 KB av de 8, och det som
Smartaid 3 får plats med utöver Forthaid (fyra kommandon och skrivaren)
ryms inte. Där arbetet är slingor i Forth över programmets byte var
Forthaid 10–15 gånger långsammare: FIND och VAR tog sekunder där
Smartaid 3 tar tiondelar. Där ROM:en gör arbetet, som i REN, är
skillnaden liten.

Botemedlet var det som uppskattningen räknade med: maskinkodsord för de heta
slingorna. Tabellen med en bit per variabel och `SETVARS` gjorde VAR
omkring 10 gånger snabbare, `SEARCH` gjorde FIND 2,8 gånger snabbare
och `MATCHVAR` CHANGE och VAR med namn 25 procent snabbare. Alla blev
dessutom kortare, sammanlagt 199 byte, eftersom ett maskinkodsord tar
mindre plats än de Forth-ord det ersätter. Nu är Forthaid 1,2–1,8 gånger
långsammare än Smartaid 3 på ett program om 12 KB, utom med FIND (3,5
gånger), där det mesta av tiden troligen går åt till ROM:ens `DETOK`;
det är inte mätt. Ett Forth-verktyg med sina tyngsta slingor i maskinkod
kommer alltså nära ett skrivet helt i assembler.

Mätningen är gjord i emulatorn; mot en riktig ABC80 eller en riktig
Smartaid 3 är den inte prövad.
