"""systemskiva.py -- en CP/M-skiva för FD2, till boken ABC80 inifrån

    python3 systemskiva.py cpm22.bin bios.bin ut.dsk [fil ...]

En FD2-avbild om 80 KB (40 spår à 8 sektorer à 256 bytes) som
cpm/start.asm kan starta och cpm/bios.asm läsa:
    spår 0 sektor 0   startblocket: "CP/M", laddadressen $E000, antalet
                      sektorer och startadressen $F600 (BIOS:ens BOOT)
    spår 0 sektor 1-  CCP och BDOS ($1600 bytes ur cpm22.bin, som börjar
                      på $E000) och BIOS:en (bios.bin, från $F600)
    spår 4-39         CP/M:s del: 72 block om 1 KB; katalogen (64
                      poster) i block 0-1, filerna från block 2
Filerna läggs i katalogen för användare 0, med namnet i versaler.
"""
import os
import struct
import sys

SPAR, SEKTORER, SEKTOR = 40, 8, 256
SPARBYTES = SEKTORER * SEKTOR
SYSTEMSPAR = 4
BLOCK = 1024
BLOCK_ANTAL = (SPAR - SYSTEMSPAR) * SPARBYTES // BLOCK          # 72
KATALOGPOSTER = 64
KATALOGBLOCK = KATALOGPOSTER * 32 // BLOCK                      # 2
LADDA, STARTA = 0xE000, 0xF600

cpm22 = open(sys.argv[1], "rb").read()[:0x1600]
bios = open(sys.argv[2], "rb").read()
assert len(cpm22) == 0x1600, "cpm22.bin ska börja på $E000 och räcka till $F600"
system = cpm22 + bios
antal = -(-len(system) // SEKTOR)
assert antal <= SYSTEMSPAR * SEKTORER - 1, "systemet får inte plats i spår 0-3"
assert LADDA + antal * SEKTOR <= 0xFF70, "systemet når det gemensamma minnet"

skiva = bytearray(SPAR * SPARBYTES)
skiva[0:9] = b"CP/M" + struct.pack("<HBH", LADDA, antal, STARTA)
skiva[SEKTOR:SEKTOR + len(system)] = system

data = SYSTEMSPAR * SPARBYTES                 # block 0
skiva[data:] = b"\xE5" * (len(skiva) - data)

post, block = 0, KATALOGBLOCK
for fil in sys.argv[4:]:
    innehall = open(fil, "rb").read()
    if len(innehall) % 128:
        innehall += b"\x1A" * (128 - len(innehall) % 128)
    namn, _, typ = os.path.basename(fil).upper().partition(".")
    namn = namn[:8].ljust(8).encode("ascii")
    typ = typ[:3].ljust(3).encode("ascii")
    poster = [innehall[k:k + 16 * BLOCK] for k in range(0, len(innehall), 16 * BLOCK)] or [b""]
    for ex, del_ in enumerate(poster):
        if post == KATALOGPOSTER:
            sys.exit("systemskiva.py: katalogen är full")
        block_i_posten = []
        for k in range(0, len(del_), BLOCK):
            if block == BLOCK_ANTAL:
                sys.exit("systemskiva.py: skivan är full")
            bit = del_[k:k + BLOCK]
            skiva[data + block * BLOCK:data + block * BLOCK + len(bit)] = bit
            block_i_posten.append(block)
            block += 1
        rader = len(del_) // 128
        katalogpost = (bytes([0]) + namn + typ + bytes([ex, 0, 0, rader])
                       + bytes(block_i_posten).ljust(16, b"\0"))
        skiva[data + post * 32:data + post * 32 + 32] = katalogpost
        post += 1

open(sys.argv[3], "wb").write(skiva)
