#!/usr/bin/env python3
"""cpmfil.py -- en fil ur en CP/M-skiva till ABC 802 (emulator/cpm/skiva.py)

    python3 -I cpmfil.py skiva.img NAMN.EXT ut

Katalogen är block 0-1 i CP/M:s del (från sektor 80), block om 2 KB,
128 poster om 32 bytes, användare 0. Filen blir hela poster om 128
bytes, med det som står efter filens slut i den sista.
"""
import sys

skiva, namn, ut = sys.argv[1:]
d = open(skiva, "rb").read()
data = 80 * 256
n, _, e = namn.upper().partition(".")
nyckel = (n.ljust(8) + e.ljust(3)).encode()
poster = sorted((d[data + k + 12], d[data + k + 15], d[data + k + 16:data + k + 32])
                for k in range(0, 128 * 32, 32)
                if d[data + k] == 0
                and bytes(c & 127 for c in d[data + k + 1:data + k + 12]) == nyckel)
if not poster:
    sys.exit("cpmfil.py: %s finns inte" % namn)
b = b""
for ex, rc, block in poster:
    rader = (ex & 1) * 128 + rc
    b += b"".join(d[data + k * 2048:data + k * 2048 + 2048] for k in block if k)[:rader * 128]
open(ut, "wb").write(b)
