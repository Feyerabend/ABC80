#!/bin/sh
# Skriver PROV.BAS på nålskrivaren P40 i emulatorn: papper.png med
# drivrutinen som den är (roms/p40.rom), och papper_orc.png med en kopia
# där OR B på $791D är utbytt mot OR C (se LASMIG.md). Kretsen i roms/
# ändras inte. Skriver också var raderna börjar, i kolumner från
# vänstra änden. Bilderna packas om (png.c komprimerar inte).
#
# Körs från emulatorns mapp eller härifrån; ../../abc80 måste vara byggd.
cd "$(dirname "$0")" || exit 1
ABC80=../../abc80
ROM=../../roms/abc80new.rom
P40=../../roms/p40.rom

python3 - $P40 p40_orc.rom <<'PY' || exit 1
import sys
d = bytearray(open(sys.argv[1], 'rb').read())
assert d[0x11D] == 0xB0, 'inte OR B på $791D'
d[0x11D] = 0xB1                              # OR C
open(sys.argv[2], 'wb').write(d)
PY
for k in p40:papper p40_orc:papper_orc; do
  krets=${k%%:*} bild=${k#*:}
  [ $krets = p40 ] && fil=$P40 || fil=$krets.rom
  "$ABC80" -r $ROM -l $fil@7800 -CP $bild.png -f PROV.BAS -k 'RUN\r' -t 9000 > /dev/null || exit 1
  printf '%-15s' "$bild.png:"
  python3 - $bild.png <<'PY' || exit 1
# Första svarta punkten på varje rad, i kolumner (4 rutor per kolumn,
# 8 rutors kant, 40 rutor per rad; se karna/p40.c).
import sys, zlib, struct
d = open(sys.argv[1], 'rb').read()
bredd, hojd = struct.unpack('>II', d[16:24])
i, z = 8, b''
while i < len(d):
    n = struct.unpack('>I', d[i:i + 4])[0]
    if d[i + 4:i + 8] == b'IDAT':
        z += d[i + 8:i + 8 + n]
    i += 12 + n
r = zlib.decompress(z)
# Packa om bilden; png.c komprimerar inte.
def stycke(typ, data):
    return (struct.pack('>I', len(data)) + typ + data
            + struct.pack('>I', zlib.crc32(typ + data)))
open(sys.argv[1], 'wb').write(d[:8] + stycke(b'IHDR', d[16:29])
                              + stycke(b'IDAT', zlib.compress(r, 9)) + stycke(b'IEND', b''))
ut = []
for rad in range((hojd - 16) // 40):
    y0 = 8 + rad * 40
    xs = [x for y in range(y0, y0 + 30) for x in range(bredd)
          if r[y * (bredd + 1) + 1 + x] < 128]
    if xs:
        ut.append('%.2f' % ((min(xs) - 8) / 4))
print(' '.join(ut))
PY
done
rm -f p40_orc.rom
