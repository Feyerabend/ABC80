"""skiva.py -- provskivan till start-PROM:en, till boken ABC80 inifrån

    python3 skiva.py hej.bin prov.dsk

En FD2-avbild om 80 KB (40 spår à 8 sektorer à 256 bytes): startblocket
i sektor 0 (se cpm/start.asm), sektor 1-9 fyllda med sitt nummer och
hej.bin (256 bytes, $FF00-$FFFF) i sektor 10.
"""
import struct
import sys

hej = open(sys.argv[1], "rb").read().ljust(256, b"\0")
assert len(hej) == 256
startblock = b"CP/M" + struct.pack("<HBH", 0xF600, 10, 0xFF80)
skiva = bytearray(40 * 8 * 256)
skiva[0:len(startblock)] = startblock
for n in range(1, 10):
    skiva[n * 256:(n + 1) * 256] = bytes([n]) * 256
skiva[10 * 256:11 * 256] = hej
open(sys.argv[2], "wb").write(skiva)
