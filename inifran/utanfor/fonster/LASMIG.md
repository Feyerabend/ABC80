# Fönster (utanför boken)

**Inte med i boken.** Ett demo som jag gjorde för Olle Wolkert
på 80-talet, textfönster i BASIC, gjort om, och samma tanke förd vidare
till grafikkortet i HR-80:s anda: fönster, en mus och händelser tillbaka
till ABC80 på V24. **Fönstren, musen och frågan `ESC ?` på kortet är
påhittade.** HR-80:s annons nämner inget av dem, och kortets manual är
inte funnen. Planen och vad varje steg gav står i [PLAN.md](PLAN.md).

| Fil                        | Vad                                                 |
|----------------------------|-----------------------------------------------------|
| `README.md`                | programmen, kommandona, händelserna och JSP-diagrammet |
| `PLAN.md`                  | de åtta stegen, med det som blev av vart och ett     |
| `STORLEK.md`, `storlek.sh` | hade tolken med fönstren fått plats på kortet? (steg 7) |

## Var det ligger

Allt annat ligger i emulatorn, där kortet redan fanns. Bokens filer
(`bok/`) är inte ändrade.

| Fil                                           | Ändringen |
|-----------------------------------------------|-----------|
| `emulator/karna/hr80.c`, `hr80.h`              | fönstren (`ESC O`, `V`, `T`, `S`), musen (`hr80_mus`), händelsekön (`hr80_handelse`) och frågan (`ESC ?`, `hr80_svar`); pekaren och den dragna ramen ritas ovanpå i `hr80_rita` |
| `emulator/karna/hr80v24.c`, `.h`, `maskin.c`, `.h` | svaret på `ESC ?` går till maskinens RxD i 9600 baud; `v24_ny` tömmer listan |
| `emulator/vard/hr80fil.c`                      | `hr80 -u dx,dy,k` (musen) och `-e` (händelserna och svaren) |
| `emulator/vard/abc80.c`, `batch.c`, `vard.h`   | `abc80 -Hm fil`: musen ur en fil, `ms dx dy knapp` |
| `emulator/hr80prov/prov.sh`, `summor.txt`      | nya prov e-h; de gamla summorna är oförändrade |
| `emulator/sdl/fonster.c`, `README.md`          | datorns mus i HR-80-fönstret är kortets mus |
| `emulator/webb/webb.c`, `abc80.js`, `mall.html` | musen på kortets bild på webbsidan, och hjälpen |
| `emulator/exempel/program/text/FONSTER.BAS`    | textfönstren i BASIC (steg 1), utan kortet |
| `emulator/exempel/program/text/FONSTER2.BAS`   | fönster på kortet (steg 5) |
| `emulator/exempel/program/hr80in.asm`          | läsrutinen, 64 byte på $FFBB efter `hr80ut.asm`; krockar med UFD-DOS:ens korttabell på $FFC0, inte med ABC-DOS |
| `emulator/exempel/program/fonster2.mus`        | musen till FONSTER2 i proven |

Utan `ESC O` gör kortet som förut: de fyra gamla provbilderna (a-d) har
samma summor, och `HR80.BAS` ritar samma bild. Det som har ändrats för
de gamla kommandona är att makrona får 1 664 byte i stället för 2 048,
eftersom fönstertabellen och kön tar 384 av de 2 kilobyte som blir kvar
bredvid bilden.

## Kortet är större än annonsens

Med fönstren och musen är tolken 2,2-2,5 gånger så stor som utan, och
den hade behövt 16 KB EPROM i stället för 4, och mer arbetsminne än
annonsens 18 KB ([STORLEK.md](STORLEK.md)). C-tolken är inte krympt
för det: emulatorn visar hur kortet kunde ha varit med ett större
EPROM, inte ett kort som gick att köpa 1984.
