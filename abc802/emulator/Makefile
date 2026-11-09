# Emulatorn till ABC 802 inifrån.
#   make         bygg abc802 (batchläget, se vard/abc802.c) och CP/M:
#                laddaren i BASICINI.SYS (cpm/start.bin), CCP och BDOS
#                (cpm/cpm22.bin), BIOS:en (cpm/bios.bin), systemskivan med
#                Digital Researchs program, editorn SKRIV, BBC BASIC och
#                Zork I med tolken ZTOLK (cpm/a.img) och en tom skiva till
#                B: (cpm/b.img)
#   make cpmprov provet till CP/M (cpm/prov.sh)
#   make webb    bygg webb/abc802.html, emulatorn i en enda sida som går
#                att öppna direkt i webbläsaren (kräver clang och wasm-ld)
#   make clean   ta bort det byggda
#
# karna/ har också ABC80:s filer ur abc80/emulator (maskin.c med flera),
# som grund; de byggs inte här, utom bussen.c, skivkort.c och skrivarkort.c
# (skrivet om för ABC 802).

CC     ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra

KARNA = karna/z80.c karna/sio.c karna/ctc.c karna/abc802.c karna/skarm802.c \
        karna/bussen.c karna/skivkort.c karna/kassett802.c karna/skrivarkort.c
VARD  = vard/abc802.c vard/png.c

# WebAssembly utan C-bibliotek, som för ABC80: clang och lld (wasm-ld)
# räcker, t.ex. Apple clang och Homebrews lld, med webb/libc.c i stället
# för C-biblioteket.
CLANG   ?= clang
WASM_LD ?= $(firstword $(wildcard /opt/homebrew/opt/lld/bin/wasm-ld /usr/local/opt/lld/bin/wasm-ld) wasm-ld)
WEBB_C   = $(KARNA) webb/webb.c webb/libc.c
WEBB_FLAGGOR = --target=wasm32 -std=c11 -O2 -Wall -Wextra -ffreestanding -nostdlib \
           -isystem webb/libc -fuse-ld=$(WASM_LD) -Wl,--no-entry -Wl,--strip-all \
           -Wl,-z,stack-size=1048576

# Assemblern ur ../dis (dis/src/z80asm.c); -g ger utfilen från ORG.
Z80ASM = ../dis/build/z80asm

# Bokens assembler (ABC80 inifrån, verktyg/z80asm.c), som ZTOLK och sidan
# till BBC BASIC är skrivna för; utfilen börjar på den lägsta adressen.
VERKTYG ?= $(abspath $(HOME)/Documents/GitHub/ABC80/inifran/verktyg)
BOKASM  ?= $(VERKTYG)/z80asm

alla: abc802 cpm/start.bin cpm/cpm22.bin cpm/bios.bin cpm/a.img cpm/b.img

abc802: $(KARNA) $(VARD) karna/*.h vard/*.h
	$(CC) $(CFLAGS) -o $@ $(KARNA) $(VARD)

webb: webb/abc802.html

webb/abc802.wasm: $(WEBB_C) karna/*.h webb/libc/*.h
	$(CLANG) $(WEBB_FLAGGOR) -o $@ $(WEBB_C)

webb/abc802.html: webb/mall.html webb/abc802.js webb/datavision.js webb/bbs.js webb/stordator.js webb/abc802.wasm webb/bygg.sh \
                  ../prov/skivor/tidsprov.img ../prov/skivor/tom.img cpm/a.img ../forth/forth.img \
                  ../roms/abc802rom.rom ../roms/orig/abc802/ABC802-char.6490191-01.bin
	webb/bygg.sh > $@

$(Z80ASM): ../dis/src/z80asm.c
	../dis/z80asm.sh $@

$(BOKASM): $(VERKTYG)/z80asm.c
	$(MAKE) -C $(VERKTYG) z80asm

cpm/start.bin: cpm/start.asm | $(Z80ASM)
	$(Z80ASM) -g 0xF503 -o $@ cpm/start.asm

# CCP och BDOS ur Digital Researchs fria CP/M 2.2 (cpm/dr/), för $E000.
cpm/cpm22.asm: cpm/dr/CPM22.Z80 cpm/cpm22.py
	python3 -I cpm/cpm22.py cpm/dr/CPM22.Z80 $@

cpm/cpm22.bin: cpm/cpm22.asm | $(Z80ASM)
	$(Z80ASM) -g 0xE000 -o $@ cpm/cpm22.asm

cpm/bios.bin: cpm/bios.asm | $(Z80ASM)
	$(Z80ASM) -g 0xF600 -o $@ cpm/bios.asm

# Listningen (på standard ut), för adresserna i BIOS:en.
cpm/bios.lst: cpm/bios.asm | $(Z80ASM)
	$(Z80ASM) -g 0xF600 -o /dev/null -l cpm/bios.asm > $@

# Editorn SKRIV, ur ABC80-emulatorn (abc80/emulator/cpm/skriv/, som den
# är), med KOLUMNER som adressen till BIOS:ens BREDDEN ur listningen.
cpm/skriv.com: cpm/skriv/skriv.asm cpm/bios.lst | $(Z80ASM)
	awk '$$3 == "BREDDEN:" { print "KOLUMNER EQU $$" $$1 }' \
	    cpm/bios.lst > cpm/skriv/med_bios.asm
	cat cpm/skriv/skriv.asm >> cpm/skriv/med_bios.asm
	$(Z80ASM) -g 0x100 -o $@ cpm/skriv/med_bios.asm

# SKRIV:s bruksanvisning SKRIV.TXT, i ABC 802:s tecken ({ | } [ \ ] för
# ä ö å Ä Ö Å) och med CR LF.
cpm/skriv.txt: cpm/skriv/handbok.txt
	sed -e 's/ä/{/g; s/ö/|/g; s/å/}/g; s/Ä/[/g; s/Ö/\\/g; s/Å/]/g' \
	    cpm/skriv/handbok.txt | awk '{ printf "%s\r\n", $$0 }' > $@

# Z-maskintolken ZTOLK, ur ABC80-emulatorn (abc80/emulator/cpm/zork/),
# med KOLUMNER som för SKRIV och bokens assembler.
cpm/ztolk.com: cpm/zork/ztolk.asm cpm/bios.lst | $(BOKASM)
	awk '$$3 == "BREDDEN:" { print "KOLUMNER EQU $$" $$1 }' \
	    cpm/bios.lst > cpm/zork/med_bios.asm
	cat cpm/zork/ztolk.asm >> cpm/zork/med_bios.asm
	$(BOKASM) -o $@ cpm/zork/med_bios.asm

# BBC BASIC med vår första sida (cpm/bbc/abc802.asm). KOLUMNER är
# adressen till BIOS:ens BREDDEN; RAD och KOL är EQU i BIOS:en och står
# inte i listningen, så de tas ur assemblerns symboltabell (-v).
cpm/bbc/sida.bin: cpm/bbc/abc802.asm cpm/bios.asm | $(Z80ASM) $(BOKASM)
	$(Z80ASM) -v -g 0xF600 -o /dev/null cpm/bios.asm | \
	    awk '$$1 == "BREDDEN" { print "KOLUMNER EQU $$" $$2 } \
	         $$1 == "RAD" || $$1 == "KOL" { print $$1 " EQU $$" $$2 }' > cpm/bbc/sida.asm
	cat cpm/bbc/abc802.asm >> cpm/bbc/sida.asm
	$(BOKASM) -o $@ cpm/bbc/sida.asm

# Sidan över den första sidan i BBCBASIC.COM, resten som den är.
cpm/bbcbasic.com: cpm/bbc/sida.bin cpm/bbc/BBCBASIC.COM
	(cat cpm/bbc/sida.bin; tail -c +257 cpm/bbc/BBCBASIC.COM) > $@

# Skivorna skrivs delvis av emulatorns DOS (BASICINI.SYS), se cpm/skiva.py.
cpm/a.img: abc802 cpm/start.bin cpm/cpm22.bin cpm/bios.bin cpm/skiva.py \
           ../prov/skivor/gor.py ../prov/skivor/tom.img $(wildcard cpm/dr/*.COM) \
           cpm/skriv.com cpm/skriv.txt cpm/bbcbasic.com cpm/ztolk.com cpm/zork/zork1.z3
	python3 -I cpm/skiva.py cpm/start.bin cpm/cpm22.bin cpm/bios.bin $@ $(wildcard cpm/dr/*.COM) \
	        cpm/skriv.com cpm/skriv.txt cpm/bbcbasic.com cpm/ztolk.com cpm/zork/zork1.z3

cpm/b.img: cpm/skiva.py
	python3 -I cpm/skiva.py - - - $@

cpmprov: alla
	cpm/prov.sh

clean:
	rm -f abc802 webb/abc802.wasm webb/abc802.html cpm/cpm22.asm cpm/a.img cpm/b.img \
	      cpm/bios.lst cpm/skriv.com cpm/skriv.txt cpm/skriv/med_bios.asm \
	      cpm/bbc/sida.asm cpm/bbc/sida.bin cpm/bbcbasic.com cpm/zork/med_bios.asm cpm/ztolk.com
	rm -rf cpm/prov/ut

.PHONY: alla clean webb cpmprov
