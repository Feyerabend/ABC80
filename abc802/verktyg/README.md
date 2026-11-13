# Verktyg

`z80asm.c` är assemblern som Forth (`../forth/gor.sh`) och Pascal
(`../pascal/bygg.sh`) byggs med. Skripten kompilerar den själva första
gången med `cc -O2 -o z80asm z80asm.c`. Den skriver hela minnesavbilden
från adress 0, och skripten klipper ut programmet med `tail -c`.

`z80asm.c` is the Z80 assembler used by the Forth and Pascal build
scripts, which compile it on first use.
