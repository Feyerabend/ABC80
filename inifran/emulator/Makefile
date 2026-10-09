# Den gemensamma emulatorn till ABC80 inifrån.
#   make         bygg abc80
#   make prov    kör den nya och den gamla (verktyg/abc80) med samma flaggor
#                och jämför skärmarna
#   make webb    bygg webb/abc80.html, emulatorn i en enda sida som går
#                att öppna direkt i webbläsaren (kräver clang och wasm-ld)
#   make clean   ta bort det byggda

CC     ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra

KARNA = karna/z80.c karna/maskin.c karna/tecken.c karna/bussen.c \
        karna/skivkort.c karna/skrivarkort.c karna/ieckort.c karna/ljud.c karna/kassett.c
VARD  = vard/abc80.c vard/terminal.c vard/batch.c vard/anrop.c vard/tstenhet.c \
        vard/kassettfil.c vard/wav.c

# WebAssembly utan C-bibliotek: clang och lld (wasm-ld) räcker, t.ex.
# Apple clang och Homebrews lld. Bara kärnan, utan skrivarkortet och
# IEC-kortet, med webb/libc.c i stället för C-biblioteket.
CLANG   ?= clang
WASM_LD ?= $(firstword $(wildcard /opt/homebrew/opt/lld/bin/wasm-ld /usr/local/opt/lld/bin/wasm-ld) wasm-ld)
WEBB_C   = karna/z80.c karna/maskin.c karna/bussen.c karna/skivkort.c karna/ljud.c \
           karna/kassett.c karna/bild.c vard/wav.c webb/webb.c webb/libc.c
WEBB_FLAGGOR = --target=wasm32 -std=c11 -O2 -Wall -Wextra -ffreestanding -nostdlib \
           -isystem webb/libc -fuse-ld=$(WASM_LD) -Wl,--no-entry -Wl,--strip-all \
           -Wl,-z,stack-size=1048576

abc80: $(KARNA) $(VARD) karna/*.h vard/*.h
	$(CC) $(CFLAGS) -o $@ $(KARNA) $(VARD) -lm

webb: webb/abc80.html

webb/abc80.wasm: $(WEBB_C) karna/*.h vard/wav.h webb/libc/*.h
	$(CLANG) $(WEBB_FLAGGOR) -o $@ $(WEBB_C)

# Exempeldisketten och Genesis-demot kommer med om de finns.
webb/abc80.html: webb/mall.html webb/abc80.js webb/abc80.wasm webb/bygg.sh \
                 roms/abc80new.rom roms/abc80old.rom roms/abcdos80.rom \
                 $(wildcard exempel/program/diskett/program.dsk \
                            exempel/genesis/GenesisProject_ABCDemo.dsk \
                            exempel/genesis/GenesisProject_ABCDemo_webb.wav)
	webb/bygg.sh > $@

prov: abc80
	@./prov.sh

clean:
	rm -rf abc80 prov webb/abc80.wasm

.PHONY: prov webb clean
