#!/usr/bin/env python3
"""abs.py: lägg en binär fil som NAMN.ABS (postformat, som DOS:ens
LOADF läser) på en MF-skiva (320K, kort 44).

    python3 abs.py forth.bin 8000 8007 FORTH tom.img forth.img

Argumenten: bilden, laddadressen och startadressen (hex), namnet,
skivan att utgå från och den nya skivan. En post per sektor: typ 0,
antal, en oanvänd byte, adressen hög och inverterad, låg och
inverterad, data och summan av data. Högst 244 bytes, så att typbyten
efter posten (här $FF, nästa sektor) står kvar i sektorn. Sist en post
med antal 0, vars adress är startadressen.

Med ändelsen .SYS (BASICINI.SYS) blir filen i stället poster om 253
bytes, en per sektor, som DOS:ens DRDREC läser; adresserna används inte.
En annan ändelse (VERK.OVL) ger poster som för .ABS.
"""
import sys

bild, org, start, namn, fran, till = sys.argv[1:7]
data = open(bild, 'rb').read()
org, start = int(org, 16), int(start, 16)
mf = bytearray(open(fran, 'rb').read())


def sektor(n):
    return mf[n * 256:n * 256 + 256]


def post(adr, d):
    return (bytes([0, len(d), 0, adr >> 8, ~adr >> 8 & 255, adr & 255,
                   ~adr & 255]) + d + bytes([sum(d) & 255]))


poster = []
namn, ext = namn.split('.') if '.' in namn else (namn, 'ABS')
if ext.upper() == 'SYS':
    for i in range(0, len(data), 253):
        poster.append(data[i:i + 253])
else:
    for i in range(0, len(data), 244):
        poster.append(post(org + i, data[i:i + 244]))
    poster.append(post(start, b''))

# En ledig katalogplats (16 x $FF) i sektor 16-31.
for ks in range(16):
    kat = sektor(16 + ks)
    k = next((k for k in range(16) if kat[k * 16:k * 16 + 16] == b'\xff' * 16),
             None)
    if k is not None:
        break
dslot = k * 16 + ks
bitkarta = bytearray(sektor(14))
nsek = len(poster) + 1                       # med indexsektorn
nkl = (nsek + 3) // 4


def ledig(c):
    return not bitkarta[c >> 3] & (0x80 >> (c & 7))


# Klustren i en följd.
c0 = next(c for c in range(8, 0xEF * 8 - nkl)
          if all(ledig(c + j) for j in range(nkl)))
assert nkl <= 32
for c in range(c0, c0 + nkl):
    bitkarta[c >> 3] |= 0x80 >> (c & 7)
bitkarta[0xEF + ks] += 1

u = (c0 << 5) | (nkl - 1)
index = bytes([dslot, 0, 0, 0xFF, u >> 8, u & 255, 0xFF, 0xFF])
s0 = c0 * 4
mf[s0 * 256:s0 * 256 + 256] = index.ljust(256, b'\xff')
for j, p in enumerate(poster, 1):
    s = bytes([dslot, j & 255, j >> 8]) + p
    mf[(s0 + j) * 256:(s0 + j + 1) * 256] = s.ljust(256, b'\xff')

namn11 = (namn.upper().ljust(8) + ext.upper()).encode('latin1')
assert len(namn11) == 11
F = c0 << 5                                  # flaggorna 0
e = bytes([F >> 8, F & 0xE0, nsek & 255, nsek >> 8]) + namn11 + b'\xff'
n = (16 + ks) * 256 + k * 16
mf[n:n + 16] = e
mf[14 * 256:15 * 256] = bitkarta
open(till, 'wb').write(mf)
print('abs.py: %s.%s, %d bytes, %d sektorer, kluster %d, katalogplats $%02X'
      % (namn.upper(), ext.upper(), len(data), nsek, c0, dslot))
