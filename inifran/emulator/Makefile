# Den gemensamma emulatorn till ABC80 inifrån.
#   make         bygg abc80 och CP/M: start-PROM:en (cpm/start.bin), CCP och
#                BDOS (cpm/cpm22.bin), BIOS:en (cpm/bios.bin) och systemskivan
#                med Digital Researchs program, editorn SKRIV och
#                Z-maskintolken ZTOLK (cpm/system.dsk), skivan
#                till B: med BBC BASIC (cpm/b.dsk) och skivan till D:
#                på kort 44 med Zork I (cpm/d.dsk)
#   make roms    hämta ROM:arna (roms/hamta.py); webb och prov
#                hämtar dem själva om de saknas
#   make skivor  hämta Luxors systemskiva och demoskiva till FD2
#                (skivor/hamta.py); webb hämtar dem själv om de saknas
#   make prov    kör den nya och den gamla (verktyg/abc80) med samma flaggor
#                och jämför skärmarna
#   make webb    bygg webb/abc80.html, emulatorn i en enda sida som går
#                att öppna direkt i webbläsaren (kräver clang och wasm-ld)
#   make cpmprov provet till start-PROM:en och BIOS:en (cpm/prov.sh)
#   make hr80    grafikkortet i HR-80:s anda utan maskinen, till PNG
#                (vard/hr80fil.c; utanför boken, se karna/hr80.h)
#   make hr80prov  bilderna i hr80prov/prov.sh mot de sparade summorna
#   make clean   ta bort det byggda

CC     ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra

KARNA = karna/z80.c karna/maskin.c karna/tecken.c karna/bussen.c \
        karna/skivkort.c karna/skrivarkort.c karna/ieckort.c karna/ljud.c karna/kassett.c \
        karna/p40.c karna/bild.c karna/uni80.c karna/hr80.c karna/hr80v24.c
VARD  = vard/abc80.c vard/terminal.c vard/batch.c vard/anrop.c vard/tstenhet.c \
        vard/kassettfil.c vard/wav.c vard/png.c vard/hr80bild.c

# WebAssembly utan C-bibliotek: clang och lld (wasm-ld) räcker, t.ex.
# Apple clang och Homebrews lld. Bara kärnan, utan IEC-kortet, med
# webb/libc.c i stället för C-biblioteket.
CLANG   ?= clang
WASM_LD ?= $(firstword $(wildcard /opt/homebrew/opt/lld/bin/wasm-ld /usr/local/opt/lld/bin/wasm-ld) wasm-ld)
WEBB_C   = karna/z80.c karna/maskin.c karna/bussen.c karna/skivkort.c karna/skrivarkort.c \
           karna/ljud.c karna/p40.c karna/kassett.c karna/bild.c karna/uni80.c karna/hr80.c karna/hr80v24.c \
           vard/wav.c webb/webb.c webb/libc.c
WEBB_FLAGGOR = --target=wasm32 -std=c11 -O2 -Wall -Wextra -ffreestanding -nostdlib \
           -isystem webb/libc -fuse-ld=$(WASM_LD) -Wl,--no-entry -Wl,--strip-all \
           -Wl,-z,stack-size=1048576

# Assemblern z80asm ur verktyg/ i det publika förrådet (inifran/verktyg),
# bredvid emulator/ där; den byggs om den inte finns.
VERKTYG ?= $(abspath $(firstword $(wildcard ../verktyg) $(HOME)/Documents/GitHub/ABC80/inifran/verktyg))
Z80ASM ?= $(VERKTYG)/z80asm

alla: abc80 cpm/start.bin cpm/cpm22.bin cpm/bios.bin cpm/system.dsk cpm/b.dsk cpm/d.dsk

$(Z80ASM):
	$(MAKE) -C $(VERKTYG) z80asm

abc80: $(KARNA) $(VARD) karna/*.h vard/*.h
	$(CC) $(CFLAGS) -o $@ $(KARNA) $(VARD) -lm

webb: webb/abc80.html

# ROM:arna ligger inte i det publika förrådet; roms/hamta.py hämtar dem.
ROMAR = roms/abc80old.rom roms/abc80new.rom roms/basicii80.rom roms/abcdos80.rom \
        roms/p40.rom roms/pr9704.rom roms/tkn80-iii.rom

roms: $(ROMAR)

$(ROMAR):
	python3 -I roms/hamta.py

# Luxors systemskiva och demoskiva ligger inte heller i förrådet;
# skivor/hamta.py hämtar dem.
SKIVOR = skivor/systemskiva.dsk skivor/demoskiva.dsk

skivor: $(SKIVOR)

$(SKIVOR):
	python3 -I skivor/hamta.py

webb/abc80.wasm: $(WEBB_C) karna/*.h vard/wav.h webb/libc/*.h
	$(CLANG) $(WEBB_FLAGGOR) -o $@ $(WEBB_C)

# CP/M (start-PROM:en och systemskivan) kommer med. Exempeldisketten,
# Forth-kretsarna, Forthaid och Forthaid II (utanfor/), bokens exempel 1-6, MÅLARE-bandet
# och Genesis-demot kommer med om de finns.
webb/abc80.html: webb/mall.html webb/abc80.js webb/abc80.wasm webb/bygg.sh \
                 roms/abc80new.rom roms/abc80old.rom roms/abcdos80.rom roms/p40.rom \
                 roms/pr9704.rom roms/tkn80-iii.rom $(SKIVOR) \
                 cpm/start.bin cpm/system.dsk cpm/b.dsk cpm/d.dsk \
                 $(wildcard exempel/program/diskett/program.dsk \
                            exempel/krets/forth.bin \
                            exempel/krets/forth_cmos.bin \
                            ../utanfor/forthaid/forthaid.bin \
                            ../utanfor/forthaid2/forthaid2.bin \
                            ../utanfor/forthaid2/forthaid2_b*.bin \
                            ../utanfor/comal/ut/comal.rom \
                            ../utanfor/comal/ut/comal.krets \
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

# Listningen, för adresserna i BIOS:en; bios.bin kan finnas utan den.
cpm/bios.lst: cpm/bios.asm | $(Z80ASM)
	$(Z80ASM) -o /dev/null -l $@ cpm/bios.asm

cpm/system.dsk: cpm/cpm22.bin cpm/bios.bin cpm/systemskiva.py $(wildcard cpm/dr/*.COM) \
                cpm/skriv.com cpm/skriv.txt cpm/ztolk.com
	python3 -I cpm/systemskiva.py cpm/cpm22.bin cpm/bios.bin $@ $(wildcard cpm/dr/*.COM) \
	        cpm/skriv.com cpm/skriv.txt cpm/ztolk.com

# Editorn SKRIV (cpm/skriv/skriv.asm), med BIOS:ens KOLUMNER ur listningen.
cpm/skriv.com: cpm/skriv/skriv.asm cpm/bios.lst | $(Z80ASM)
	awk '$$1 == "=" && $$3 == "KOLUMNER" { print $$3 " EQU $$" $$2 }' \
	    cpm/bios.lst > cpm/skriv/med_bios.asm
	cat cpm/skriv/skriv.asm >> cpm/skriv/med_bios.asm
	$(Z80ASM) -o $@ cpm/skriv/med_bios.asm

# SKRIV:s bruksanvisning SKRIV.TXT, i ABC80:s tecken ({ | } [ \ ] för
# ä ö å Ä Ö Å) och med CR LF.
cpm/skriv.txt: cpm/skriv/handbok.txt
	sed -e 's/ä/{/g; s/ö/|/g; s/å/}/g; s/Ä/[/g; s/Ö/\\/g; s/Å/]/g' \
	    cpm/skriv/handbok.txt | awk '{ printf "%s\r\n", $$0 }' > $@

# Z-maskintolken ZTOLK (cpm/zork/ztolk.asm), också med KOLUMNER.
cpm/ztolk.com: cpm/zork/ztolk.asm cpm/bios.lst | $(Z80ASM)
	awk '$$1 == "=" && $$3 == "KOLUMNER" { print $$3 " EQU $$" $$2 }' \
	    cpm/bios.lst > cpm/zork/med_bios.asm
	cat cpm/zork/ztolk.asm >> cpm/zork/med_bios.asm
	$(Z80ASM) -o $@ cpm/zork/med_bios.asm

# BBC BASIC med vår första sida (cpm/bbc/abc80.asm), som får adresserna
# till BIOS:ens KOLUMNER, RAD och KOL ur listningen.
cpm/bbc/sida.bin: cpm/bbc/abc80.asm cpm/bios.lst | $(Z80ASM)
	awk '$$1 == "=" && ($$3 == "KOLUMNER" || $$3 == "RAD" || $$3 == "KOL") \
	     { print $$3 " EQU $$" $$2 }' cpm/bios.lst > cpm/bbc/sida.asm
	cat cpm/bbc/abc80.asm >> cpm/bbc/sida.asm
	$(Z80ASM) -o $@ cpm/bbc/sida.asm
	test `wc -c < $@` -eq 256

cpm/bbcbasic.com: cpm/bbc/sida.bin cpm/bbc/BBCBASIC.COM
	(cat cpm/bbc/sida.bin; tail -c +257 cpm/bbc/BBCBASIC.COM) > $@

# Skivan till B:, utan system.
cpm/b.dsk: cpm/bbcbasic.com cpm/systemskiva.py
	python3 -I cpm/systemskiva.py - - $@ cpm/bbcbasic.com

# Skivan till D: på kort 44 (640 KB), med Zork I (cpm/zork/).
cpm/d.dsk: cpm/zork/zork1.z3 cpm/systemskiva.py
	python3 -I cpm/systemskiva.py -44 $@ cpm/zork/zork1.z3

prov: abc80 $(ROMAR)
	@./prov.sh

hr80: karna/hr80.c karna/hr80.h karna/teckensnitt.h vard/hr80fil.c vard/hr80bild.c vard/hr80bild.h vard/png.c vard/png.h
	$(CC) $(CFLAGS) -o $@ karna/hr80.c vard/hr80fil.c vard/hr80bild.c vard/png.c

hr80prov: hr80
	@hr80prov/prov.sh

cpmprov: abc80 cpm/cpm22.bin cpm/bios.bin cpm/b.dsk cpm/skriv.com cpm/ztolk.com cpm/d.dsk | $(Z80ASM)
	@Z80ASM=$(Z80ASM) cpm/prov.sh

clean:
	rm -rf abc80 hr80 hr80prov/ut prov webb/abc80.wasm cpm/cpm22.asm cpm/system.dsk cpm/b.dsk \
	       cpm/bios.lst cpm/skriv/med_bios.asm cpm/skriv.com cpm/skriv.txt cpm/bbc/sida.asm cpm/bbc/sida.bin cpm/bbcbasic.com \
	       cpm/zork/med_bios.asm cpm/ztolk.com cpm/d.dsk

.PHONY: roms skivor alla prov cpmprov hr80prov webb clean
