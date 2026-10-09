#!/bin/sh
# Gör exempelprogrammen färdiga för band och diskett, till ABC80 inifrån.
#
#   1. airfight.asm assembleras med z80asm (verktyg/ i det publika
#      förrådet, se INIFRAN i ../../prov.sh) till airfight.bin.
#   2. airfight.bin blir laddaren text/AIRFIGHT.BAS: koden står i DATA-rader,
#      fyra tecken för tre bytes (ASC(c)-48, sex bitar var, räknat med heltal
#      om högst åtta bitar; flyttalen är inte exakta upp till 2^24), och sist en
#      summa som laddaren prövar innan den gör CALL(57344).
#   3. Varje program i text/ skrivs in i emulatorn och sparas med SAVE CAS:
#      på ett eget band, kassett/NAMN.wav.
#   4. Alla program sparas med SAVE DR0: på en tom ABC-DOS-diskett,
#      diskett/program.dsk: 640 sektorer om 256 bytes, fyllda med $40, bitkartan
#      i sektor 6 (och 7) och katalogen i sektor 16-23 med kopian i 8-15,
#      som på Genesis-disketten (band 2, kapitlet om FD2).
#
# Körs från emulatorns mapp eller härifrån; ../../abc80 måste vara byggd.
cd "$(dirname "$0")" || exit 1
if [ -z "$INIFRAN" ]; then
  if [ -f ../../../verktyg/abc80.c ]; then INIFRAN=../../..
  else INIFRAN=$HOME/Documents/GitHub/ABC80/inifran; fi
fi
Z80ASM=$INIFRAN/verktyg/z80asm
[ -x "$Z80ASM" ] || make -C "$INIFRAN/verktyg" z80asm >/dev/null || exit 1
ABC80=../../abc80
ROM=../../roms/abc80new.rom
DOS=../../roms/abcdos80.rom
PROGRAM="GISSA KURVA MUSIK AIRFIGHT"

"$Z80ASM" -o airfight.bin airfight.asm || exit 1

python3 - airfight.bin > text/AIRFIGHT.BAS <<'EOF' || exit 1
import sys
kod = open(sys.argv[1], 'rb').read()
kod += bytes(-len(kod) % 3)
print("10 REM AIRFIGHT, KRISTIAN LIDBERG OCH SET LONNERT 1981")
print("20 REM SPELET I Z80-KOD (AIRFIGHT.ASM) LÄGGS PÅ $E000 OCH STARTAS")
print("30 DIM A$=72")
print("40 P=57344 : S=0")
print("50 PRINT CHR$(12);\"AIRFIGHT laddas, vänta ...\"")
print("60 READ A$ : IF A$=\".\" THEN 130")
print("70 FOR I%=1% TO LEN(A$) STEP 4%")
print("80 A%=ASC(MID$(A$,I%,1%))-48% : B%=ASC(MID$(A$,I%+1%,1%))-48%")
print("90 C%=ASC(MID$(A$,I%+2%,1%))-48% : D%=ASC(MID$(A$,I%+3%,1%))-48%")
print("100 H%=A%*4%+B%/16% : M%=(B%-B%/16%*16%)*16%+C%/4% : V%=(C%-C%/4%*4%)*64%+D%")
print("110 POKE P,H%,M%,V% : P=P+3 : S=S+H%+M%+V% : NEXT I%")
print("120 GOTO 60")
print("130 READ T : IF S<>T THEN PRINT \"Fel i DATA-raderna.\" : END")
print("140 Z=CALL(57344)")
rad = 1000
for i in range(0, len(kod), 54):
    del_ = kod[i:i + 54]
    text = ''
    for j in range(0, len(del_), 3):
        v = del_[j] << 16 | del_[j + 1] << 8 | del_[j + 2]
        text += ''.join(chr(48 + (v >> s & 63)) for s in (18, 12, 6, 0))
    print('%d DATA "%s"' % (rad, text))
    rad += 10
print('%d DATA ".",%d' % (rad, sum(kod)))
EOF

for p in $PROGRAM; do
  "$ABC80" -r $ROM -f text/$p.BAS -k "SAVE CAS:$p\r" -U kassett/$p.wav -t 100 > /dev/null || exit 1
  # Från 16 bitar och 44 100 prov/s till 8 bitar och 11 025: en fjärdedel
  # av provet och en åttondel av storleken; flankerna hamnar högst 0,1 ms fel.
  python3 - kassett/$p.wav <<'EOF' || exit 1
import sys, wave, array
w = wave.open(sys.argv[1])
prov = array.array('h', w.readframes(w.getnframes()))
w.close()
ut = wave.open(sys.argv[1], 'wb')
ut.setparams((1, 1, 11025, 0, 'NONE', 'not compressed'))
ut.writeframes(bytes((v >> 8) + 128 for v in prov[::4]))
ut.close()
EOF
done

python3 - diskett/program.dsk <<'EOF' || exit 1
import sys
d = bytearray(b'\x40' * 640 * 256)
karta = bytearray(256)
karta[0:3] = b'\xff' * 3                     # sektor 0-23
karta[0x50:0xEF] = b'\xff' * (0xEF - 0x50)   # sektor 640- finns inte
karta[0xEF:0xF7] = b'\x01' * 8               # den första posten i varje katalogsektor
d[6 * 256:7 * 256] = karta
d[7 * 256:8 * 256] = karta
katalog = bytes(16) + b'\xff' * 240
for s in range(8, 24):
    d[s * 256:(s + 1) * 256] = katalog
open(sys.argv[1], 'wb').write(d)
EOF
flaggor=
for p in $PROGRAM; do
  flaggor="$flaggor -f text/$p.BAS -k 'SAVE DR0:$p\r\w3000\.NEW\r\w500\.'"
done
eval "$ABC80 -r $ROM -l $DOS@6000 -D diskett/program.dsk@45 -DS $flaggor -t 1000" > /dev/null || exit 1
