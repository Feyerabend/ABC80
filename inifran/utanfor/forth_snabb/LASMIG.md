# En snabbare Forth (utanför boken)

**Inte med i boken.** Ett experiment med Forth-tolken från exempel 7 i
band 2: hur mycket av dess långsamhet kommer från stackprövningen, och
vad kostar det i säkerhet att göra den billigare? Allt annat är som i
boken; jämförelsen gäller tolken som krets på $4000 (`e7ip`, samma som
`emulator/exempel/krets/forth.bin`).

| Fil               | Vad                                                   |
|-------------------|-------------------------------------------------------|
| `gor.sh`          | bygger `forth_snabb.bin` ur en kopia av bokens källor |
| `forth_snabb.bin` | tolken som krets på $4000 med den nya `BRANCH`        |
| `forth_snabb.lst` | listningen från z80asm                                |
| `matning.sh`      | mäter BASIC, bokens tolk och varianten                |
| `TID.BAS`         | BASIC-delen av mätningen                              |

## Ändringen

Bara `BRANCH` i kärnan (`e7k.inc`, i det publika förrådet
`inifran/forth/e7k.inc`) är utbytt, i en
kopia som `gor.sh` gör. Alla slingor går tillbaka via `BRANCH`
(`(LOOP)`, `AGAIN`, `REPEAT`, och `0BRANCH` när den hoppar). I boken
prövar den tre saker varje varv:

1. datastacken inte tömd förbi S0,
2. datastacken inte nere vid BASIC:s variabler (HEAP + 64),
3. returstacken inte nere vid kanariefågeln (som `DOCOL`).

Det kostar omkring 190 T-cykler, mer än hälften av ett tomt `DO LOOP`-varv
(omkring 410 T). Varianten prövar bara den andra, den som skyddar
BASIC-programmet, och bara på den höga byten:

    BRANCH: LD   HL,-64
            ADD  HL,SP
            LD   A,(HEAP+1)
            CP   H
            JP   NC,STKERR      ; SP - 64 på HEAP:s sida eller lägre
            EX   DE,HL          ; som förut: hoppa
            ...

Den slår alltså till upp till 256 bytes för tidigt, men kostar 48 T och
behöver varken DE eller RAM. Det första och tredje fallet fångar
`?STACK` efter varje rad, som i boken; `DOCOL` prövar fortfarande
returstacken vid varje anrop.

## Mätningen

`./matning.sh`, 2026-10-10. Tiden mäts i den emulerade maskinen med
klockan på 65008 (50 Hz, 20 ms upplösning), med den rättelse för den
låga byten som band 1 beskriver; utan den blev en mätning 256 tick
(5,12 s) fel. Det är emulatorns T-cykler; mot en riktig ABC80 är det inte
prövat. 10 000 varv i alla prov utom sållet (primtalen upp till 1000,
168 i alla tre).

| Prov                                   | BASIC  | Forth, boken | Forth, varianten |
|----------------------------------------|--------|--------------|------------------|
| tom slinga, `FOR I%` / `DO LOOP`       | 2,20 s | 1,36 s (1,6×) | 0,78 s (2,8×)   |
| tom slinga med flyttal, `FOR I`        | 9,26 s | (6,8×)       | (11,9×)          |
| `A%=I%+I%-5%` / `I I + 5 - A !`        | 8,94 s | 3,50 s (2,6×) | 2,92 s (3,1×)   |
| `GOSUB` / anrop av ett tomt kolonord   | 5,60 s | 2,48 s (2,3×) | 1,92 s (2,9×)   |
| sållet till 1000                       | 2,82 s | 1,72 s (1,6×) | 1,48 s (1,9×)   |
| `BEGIN 1 - DUP 0= UNTIL`               | –      | 2,08 s       | 1,50 s           |

Inom parentes: hur många gånger snabbare än BASIC. Den tomma slingan
går nästan dubbelt så fort; i prov med mer arbete i varje varv blir
vinsten mindre, eftersom det är `NEXT`, `DOCOL` och orden själva som
kostar där. Forth-sållet är delat i korta ord (radbufferten tar 79
tecken), och tolken saknar `*` och `/`, så proven har bara `+` och `-`.

## Vad det kostar

Provat med en slinga för varje fel, och sedan `1 2 + .` och `BYE`:

| Slinga                            | Boken     | Varianten                  |
|-----------------------------------|-----------|----------------------------|
| `BEGIN 1 AGAIN` (fyller)          | `STACK ?` | `STACK ?`                  |
| `BEGIN DROP AGAIN` (tömmer)       | `STACK ?` | `STACK ?`                  |
| `BEGIN DROP DROP 1 AGAIN`         | `STACK ?` | `STACK ?`, BASIC klarar sig |
| `10 0 DO R> DROP LOOP` (returstacken) | `STACK ?` | hänger sig            |

Den tömda datastacken fångas i varianten först när SP har gått runt
minnet och kommit ned under HEAP:s sida; att BASIC klarade sig i provet
är tur snarare än skydd. En slinga som tömmer returstacken fångas inte
alls: `R>` tar då BASIC:s stack som returadresser.

Att pröva stackarna bara i den yttre tolken är vad fig-FORTH gjorde:
`INTERPRET` anropar `?STACK` efter varje ord som tolkas, men inte inne i
kolonorden. Bokens tolk är försiktigare än så, eftersom ett fel annars kan ta med sig
BASIC-programmet.

## Bygga och mäta

    ./gor.sh          (forth_snabb.bin ur en kopia av Forth-källorna, som inte ändras)
    ./matning.sh

I emulatorn (från dess mapp): `./abc80 -l <väg>/forth_snabb.bin@4000`, sedan
`POKE 65052,0,208`, `NEW` och `Z=CALL(16384,3)`, som med bokens krets.
