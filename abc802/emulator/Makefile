# Emulatorn till ABC 802 (den publika kopian).
#   make roms    hämta ROM:arna till ../roms/ (../roms/hamta.py)
#   make         bygg abc802 (batchläget, se vard/abc802.c)
#   make webb    bygg webb/abc802.html, emulatorn i en enda sida som går
#                att öppna direkt i webbläsaren (kräver clang och wasm-ld)
#   make clean   ta bort det byggda
# Fönstret med SDL3 byggs i sdl/ (make där).
#
# Skivorna är färdiga: skivor/tidsprov.img (tidsproven ur ../benchmark/
# och Luxors systemprogram), skivor/tom.img, skivor/forth.img och CP/M-
# skivorna cpm/a.img och cpm/b.img. Källorna till CP/M (start.asm,
# bios.asm, cpm22.py, skiva.py) finns i cpm/ att läsa; skivorna byggdes
# med verktyg som inte ligger här.

CC     ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra

KARNA = karna/z80.c karna/sio.c karna/ctc.c karna/abc802.c karna/skarm802.c \
        karna/bussen.c karna/skivkort.c karna/kassett802.c karna/skrivarkort.c \
        karna/natkort.c
VARD  = vard/abc802.c vard/natmapp.c vard/png.c

ROM    = ../roms/abc802rom.rom
TECKEN = ../roms/orig/abc802/ABC802-char.6490191-01.bin

# WebAssembly utan C-bibliotek, som för ABC80: clang och lld (wasm-ld)
# räcker, t.ex. Apple clang och Homebrews lld, med webb/libc.c i stället
# för C-biblioteket.
CLANG   ?= clang
WASM_LD ?= $(firstword $(wildcard /opt/homebrew/opt/lld/bin/wasm-ld /usr/local/opt/lld/bin/wasm-ld) wasm-ld)
WEBB_C   = $(KARNA) webb/webb.c webb/libc.c
WEBB_FLAGGOR = --target=wasm32 -std=c11 -O2 -Wall -Wextra -ffreestanding -nostdlib \
           -isystem webb/libc -fuse-ld=$(WASM_LD) -Wl,--no-entry -Wl,--strip-all \
           -Wl,-z,stack-size=1048576

alla: abc802

abc802: $(KARNA) $(VARD) karna/*.h vard/*.h
	$(CC) $(CFLAGS) -o $@ $(KARNA) $(VARD)

roms: $(ROM) $(TECKEN)

$(ROM) $(TECKEN):
	python3 ../roms/hamta.py

webb: webb/abc802.html

webb/abc802.wasm: $(WEBB_C) karna/*.h webb/libc/*.h
	$(CLANG) $(WEBB_FLAGGOR) -o $@ $(WEBB_C)

webb/abc802.html: webb/mall.html webb/abc802.js webb/datavision.js webb/bbs.js webb/stordator.js \
                  webb/abc802.wasm webb/bygg.sh skivor/tidsprov.img skivor/tom.img skivor/forth.img \
                  cpm/a.img $(ROM) $(TECKEN)
	SKIVOR=skivor FORTH=skivor/forth.img webb/bygg.sh > $@

clean:
	rm -f abc802 webb/abc802.wasm

.PHONY: alla roms webb clean
