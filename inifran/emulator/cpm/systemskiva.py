"""systemskiva.py -- en CP/M-skiva för FD2, till boken ABC80 inifrån

    python3 systemskiva.py cpm22.bin bios.bin ut.dsk [fil ...]
    python3 systemskiva.py - - ut.dsk [fil ...]      (utan system)
    python3 systemskiva.py -44 ut.dsk [fil ...]      (D: på kort 44)

En FD2-avbild om 80 KB (40 spår à 8 sektorer à 256 bytes) som
cpm/start.asm kan starta och cpm/bios.asm läsa:
    spår 0 sektor 0   startblocket: "CP/M", laddadressen $E000, antalet
                      sektorer och startadressen $F600 (BIOS:ens BOOT)
    spår 0 sektor 1-  CCP och BDOS ($1600 bytes ur cpm22.bin, som börjar
                      på $E000) och BIOS:en (bios.bin, från $F600)
    spår 4-39         CP/M:s del: 72 block om 1 KB; katalogen (64
                      poster) i block 0-1, filerna från block 2
Med -44 blir det i stället en avbild till D: på kort 44 (ABC 832), 640
KB (2 560 sektorer à 256 bytes i följd) utan system: 320 block om 2 KB,
katalogen (128 poster) i block 0-1, blocknumren i två bytes.
Filerna läggs i katalogen för användare 0, med namnet i versaler.
"""
import os
import struct
import sys

MF = sys.argv[1] == "-44"
if MF:
    sys.argv[1:2] = ["-", "-"]
SEKTOR = 256
if MF:                                        # kort 44
    SPAR, SEKTORER, SYSTEMSPAR, BLOCK, KATALOGPOSTER = 160, 16, 0, 2048, 128
else:                                         # FD2
    SPAR, SEKTORER, SYSTEMSPAR, BLOCK, KATALOGPOSTER = 40, 8, 4, 1024, 64
SPARBYTES = SEKTORER * SEKTOR
BLOCK_ANTAL = (SPAR - SYSTEMSPAR) * SPARBYTES // BLOCK          # 72 eller 320
KATALOGBLOCK = KATALOGPOSTER * 32 // BLOCK                      # 2
BLOCK_I_POST = 8 if BLOCK_ANTAL > 256 else 16                   # 16 KB per post
LADDA, STARTA = 0xE000, 0xF600

skiva = bytearray(SPAR * SPARBYTES)
if sys.argv[1] != "-":
    cpm22 = open(sys.argv[1], "rb").read()[:0x1600]
    bios = open(sys.argv[2], "rb").read()
    assert len(cpm22) == 0x1600, "cpm22.bin ska börja på $E000 och räcka till $F600"
    system = cpm22 + bios
    antal = -(-len(system) // SEKTOR)
    assert antal <= SYSTEMSPAR * SEKTORER - 1, "systemet får inte plats i spår 0-3"
    assert LADDA + antal * SEKTOR <= 0xFF70, "systemet når det gemensamma minnet"
    skiva[0:9] = b"CP/M" + struct.pack("<HBH", LADDA, antal, STARTA)
    skiva[SEKTOR:SEKTOR + len(system)] = system
else:
    skiva[:] = b"\xE5" * len(skiva)

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
    poster = [innehall[k:k + BLOCK_I_POST * BLOCK] for k in range(0, len(innehall), BLOCK_I_POST * BLOCK)] or [b""]
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
        if BLOCK_I_POST == 8:
            karta = b"".join(struct.pack("<H", b) for b in block_i_posten)
        else:
            karta = bytes(block_i_posten)
        katalogpost = (bytes([0]) + namn + typ + bytes([ex, 0, 0, rader])
                       + karta.ljust(16, b"\0"))
        skiva[data + post * 32:data + post * 32 + 32] = katalogpost
        post += 1

open(sys.argv[3], "wb").write(skiva)
