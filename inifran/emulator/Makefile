# Den gemensamma emulatorn till ABC80 inifrån.
#   make         bygg abc80 och CP/M: start-PROM:en (cpm/start.bin), CCP och
#                BDOS (cpm/cpm22.bin), BIOS:en (cpm/bios.bin) och systemskivan
#                med Digital Researchs program (cpm/system.dsk)
#   make prov    kör den nya och den gamla (verktyg/abc80) med samma flaggor
#                och jämför skärmarna
#   make webb    bygg webb/abc80.html, emulatorn i en enda sida som går
#                att öppna direkt i webbläsaren (kräver clang och wasm-ld)
#   make cpmprov provet till start-PROM:en och BIOS:en (cpm/prov.sh)
#   make clean   ta bort det byggda

CC     ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra

KARNA = karna/z80.c karna/maskin.c karna/tecken.c karna/bussen.c \
        karna/skivkort.c karna/skrivarkort.c karna/ieckort.c karna/ljud.c karna/kassett.c \
        karna/p40.c karna/bild.c karna/uni80.c
VARD  = vard/abc80.c vard/terminal.c vard/batch.c vard/anrop.c vard/tstenhet.c \
        vard/kassettfil.c vard/wav.c vard/png.c

# WebAssembly utan C-bibliotek: clang och lld (wasm-ld) räcker, t.ex.
# Apple clang och Homebrews lld. Bara kärnan, utan skrivarkortet och
# IEC-kortet, med webb/libc.c i stället för C-biblioteket.
CLANG   ?= clang
WASM_LD ?= $(firstword $(wildcard /opt/homebrew/opt/lld/bin/wasm-ld /usr/local/opt/lld/bin/wasm-ld) wasm-ld)
WEBB_C   = karna/z80.c karna/maskin.c karna/bussen.c karna/skivkort.c karna/ljud.c \
           karna/p40.c karna/kassett.c karna/bild.c karna/uni80.c vard/wav.c webb/webb.c webb/libc.c
WEBB_FLAGGOR = --target=wasm32 -std=c11 -O2 -Wall -Wextra -ffreestanding -nostdlib \
           -isystem webb/libc -fuse-ld=$(WASM_LD) -Wl,--no-entry -Wl,--strip-all \
           -Wl,-z,stack-size=1048576

# Assemblern z80asm ur verktyg/ i det publika förrådet (inifran/verktyg),
# bredvid emulator/ där; den byggs om den inte finns.
VERKTYG ?= $(abspath $(firstword $(wildcard ../verktyg) $(HOME)/Documents/GitHub/ABC80/inifran/verktyg))
Z80ASM ?= $(VERKTYG)/z80asm

alla: abc80 cpm/start.bin cpm/cpm22.bin cpm/bios.bin cpm/system.dsk

$(Z80ASM):
	$(MAKE) -C $(VERKTYG) z80asm

abc80: $(KARNA) $(VARD) karna/*.h vard/*.h
	$(CC) $(CFLAGS) -o $@ $(KARNA) $(VARD) -lm

webb: webb/abc80.html

webb/abc80.wasm: $(WEBB_C) karna/*.h vard/wav.h webb/libc/*.h
	$(CLANG) $(WEBB_FLAGGOR) -o $@ $(WEBB_C)

# CP/M (start-PROM:en och systemskivan) kommer med. Exempeldisketten,
# Forth-kretsarna, bokens exempel 1-6, MÅLARE-bandet och Genesis-demot
# kommer med om de finns.
webb/abc80.html: webb/mall.html webb/abc80.js webb/abc80.wasm webb/bygg.sh \
                 roms/abc80new.rom roms/abc80old.rom roms/abcdos80.rom roms/p40.rom \
                 roms/tkn80-iii.rom cpm/start.bin cpm/system.dsk \
                 $(wildcard exempel/program/diskett/program.dsk \
                            exempel/krets/forth.bin \
                            exempel/krets/forth_cmos.bin \
                            $(wildcard exempel/krets/e*.bin) \
                            exempel/malare/MALARE.wav \
                            exempel/genesis/GenesisProject_ABCDemo.dsk \
                            exempel/genesis/GenesisProject_ABCDemo_webb.wav)
	webb/bygg.sh > $@

cpm/start.bin: cpm/start.asm | $(Z80ASM)
	$(Z80ASM) -o $@ cpm/start.asm

# CCP och BDOS ur Digital Researchs fria CP/M 2.2 (cpm/dr/), för $E000.
cpm/cpm22.asm: cpm/dr/CPM22.Z80 cpm/cpm22.py
	python3 -I cpm/cpm22.py cpm/dr/CPM22.Z80 $@

cpm/cpm22.bin: cpm/cpm22.asm | $(Z80ASM)
	$(Z80ASM) -o $@ cpm/cpm22.asm

cpm/bios.bin: cpm/bios.asm | $(Z80ASM)
	$(Z80ASM) -o $@ cpm/bios.asm

cpm/system.dsk: cpm/cpm22.bin cpm/bios.bin cpm/systemskiva.py $(wildcard cpm/dr/*.COM)
	python3 -I cpm/systemskiva.py cpm/cpm22.bin cpm/bios.bin $@ $(wildcard cpm/dr/*.COM)

prov: abc80
	@./prov.sh

cpmprov: abc80 cpm/cpm22.bin cpm/bios.bin | $(Z80ASM)
	@Z80ASM=$(Z80ASM) cpm/prov.sh

clean:
	rm -rf abc80 prov webb/abc80.wasm cpm/cpm22.asm cpm/system.dsk

.PHONY: alla prov cpmprov webb clean
