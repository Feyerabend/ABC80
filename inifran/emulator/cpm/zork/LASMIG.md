# Zork I och tolken ZTOLK

`zork1.z3` är Infocoms Zork I, berättelsefilen för Z-maskinen version 3,
oförändrad ur förrådet <https://github.com/historicalsource/zork1>
(`COMPILED/zork1.z3`, commit 5e9a9ce, 16 april 2019), SHA1
c4f162274869b5433e4b9dfa7ee770fc3b789525. Källan i ZIL finns i samma
förråd. `LICENSE` är förrådets licens, MIT-licensen (Copyright (c) 2025
Microsoft, tillagd i commit 60aef2f, 20 november 2025): filen får
användas, ändras och spridas fritt om licenstexten följer med.

Filen är 86 838 bytes och får inte plats på FD2 (80 KB). Makefile lägger
den på skivan till D: på kort 44 (`../d.dsk`, 640 KB) som `ZORK1.Z3`.

## Tolken

`ztolk.asm` är vår egen tolk för Z-maskinen version 3, skriven för vår
BIOS (`../bios.asm`) i 40 och 80 tecken, utan kod ur andra tolkar.
Makefile sätter BIOS:ens KOLUMNER först och skriver `../ztolk.com`, som
läggs på systemskivan (`../system.dsk`). Spelet startas med

    A>ZTOLK D:ZORK1

(typen Z3 om ingen ges). Det dynamiska minnet ligger fast i TPA; resten
av filen läses i sidor om 256 bytes, så många som får plats i förväg,
och de äldsta sidorna lämnar plats åt nya när minnet inte räcker. Översta raden är
statusraden, och (MER) väntar på en tangent när en skärm fylls utan
inmatning. Med save och restore skrivs och läses spelet i en fil, i
standard `ZORK1.SAV` på samma skiva som spelet.
