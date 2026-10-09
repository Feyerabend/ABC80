#!/bin/sh
# Gör bandet MALARE.wav av MALARE.BAS, ritprogrammet MÅLARE av Mikael
# Bonnier 1982 (GPL-3.0, se COPYING och LASMIG.md).
#
# Programmet skrivs in i emulatorn och sparas med SAVE CAS:M]LARE, som
# på ABC80 är MÅLARE: originalbandets namn (MÅLARE.BAC). Inspelningen
# blir 8 bitar och 11 025 prov/s, som exempelprogrammens band.
#
# Körs från emulatorns mapp eller härifrån; ../../abc80 måste vara byggd.
cd "$(dirname "$0")" || exit 1
ABC80=../../abc80
ROM=../../roms/abc80new.rom

"$ABC80" -r $ROM -f MALARE.BAS -k 'SAVE CAS:M]LARE\r' -U MALARE.wav -t 100 > /dev/null || exit 1
# Från 16 bitar och 44 100 prov/s till 8 bitar och 11 025.
python3 - MALARE.wav <<'EOF' || exit 1
import sys, wave, array
w = wave.open(sys.argv[1])
prov = array.array('h', w.readframes(w.getnframes()))
w.close()
ut = wave.open(sys.argv[1], 'wb')
ut.setparams((1, 1, 11025, 0, 'NONE', 'not compressed'))
ut.writeframes(bytes((v >> 8) + 128 for v in prov[::4]))
ut.close()
EOF
echo "gor.sh: MALARE.wav"
