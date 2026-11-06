# BBC BASIC (Z80)

`BBCBASIC.COM` är R. T. Russells BBC BASIC (Z80) version 5.00, den
allmänna utgåvan för CP/M, oförändrad ur hans förråd
<https://github.com/rtrussell/BBCZ80> (`bin/cpm/BBCBASIC.COM`, commit
410e7fb, 16 mars 2025), SHA1 d36beac248985b99731c5ceff1e3546bbf4e9ffe.
Källan finns i samma förråd (`src/`). `licence.txt` är förrådets licens,
zlib-licensen: programmet får användas, ändras och spridas fritt, men en
ändrad version ska märkas som ändrad.

## Det som är ändrat

Den första sidan, $0100-$01FF, är den del som Russell lämnar åt den som
anpassar BASIC till en dator (`src/DIST.Z80`). Där har den allmänna
utgåvan ingen skärm och läser klockan med ett anrop som bara finns i
RunCPM. `abc802.asm` här är vår version av sidan, efter den till ABC80
(`abc80/emulator/cpm/bbc/abc80.asm`) och för vår BIOS (`../bios.asm`):
CLS och markören med BIOS:ens ADM-3A-koder, POS och VPOS ur BIOS:en,
bredden 80, högerpilen som markören åt höger och en räknad klocka (BIOS:en
har avbrotten avstängda), med väntan avpassad för 3 MHz. Kontrollerna av
Z80 och CP/M 2 är borttagna för att koden ska få plats. Makefile
assemblerar sidan med bokens assembler, lägger den över den första sidan
i `BBCBASIC.COM` och skriver resultatet till `../bbcbasic.com`, som
läggs på systemskivan (`../a.img`); resten av filen, BASIC självt, är
densamma byte för byte.

Namnet BBC BASIC används med BBC:s tillstånd och följer enligt Russells
README inte med till en härledd version eller avgrening. Det här är
hans program med en ny anpassningssida, inte en egen version av
språket.
