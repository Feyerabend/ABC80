#!/bin/bash
# Provet för bankkortet (steg 2 i ../PLAN.md, utanför boken): emulatorns
# -b med fyra banker à 2 KB på $5000 och en fast del på $4000, som
# Forthaid II ska ligga. Kortet är påhittat; hur det fungerar står i
# ../LASMIG.md. Bank n har n på $5000 och $A0+n på $57FF. BASIC väljer
# bank n med POKE 20544+n ($5040+n) och läser tillbaka båda ändarna och
# den fasta delen; en skrivning i fönstret ska inte gå igenom, och
# $5044 (ingen bank 4) ska inte byta bank. Körs mot 9913 och 11273, och
# med TKN80 i 80-teckensläget, där skärmen ska ha samma tecken
# (blanktecken och radslut tas bort före jämförelsen). ./prov.sh -u
# skriver bank.out.
cd "$(dirname "$0")" || exit 1
E=../../../emulator
make -C $E abc80 > /dev/null || exit 1
B=bygg; rm -rf $B && mkdir $B || exit 1
python3 - $B <<'PY' || exit 1
import sys
d = sys.argv[1]
open(d + "/fast.bin", "wb").write(bytes([0x5A]) * 4096)
for n in range(4):
    b = bytearray([0xFF]) * 2048
    b[0], b[-1] = n, 0xA0 + n
    open("%s/b%d.bin" % (d, n), "wb").write(b)
PY
K='10 FOR N=3 TO 0 STEP -1\r20 POKE 20544+N,0\r30 PRINT N;PEEK(20480);PEEK(22527);PEEK(16384)\r40 NEXT N\r'
K="$K"'50 POKE 20480,99:PRINT PEEK(20480)\r60 POKE 20546,0:POKE 20548,0:PRINT PEEK(20480)\rRUN\r'
upd=0; [ "$1" = "-u" ] && upd=1
kor() { eval "$E/abc80 -r $1 $2 -l $B/fast.bin@4000 -b $B/b0.bin,$B/b1.bin,$B/b2.bin,$B/b3.bin@5000 -k '$K' -t 500" | sed 's/ *$//' | grep -v '^$'; }
fail=0
T=$E/roms/tkn80-iii.rom
for r in 9913 11273; do
  rom=$E/roms/abc80new.rom; [ $r = 11273 ] && rom=$E/roms/abc80old.rom
  [ -f $rom ] || { echo "prov.sh: $rom saknas (hamta.py i roms/)"; exit 1; }
  out=$(kor $rom)
  [ $upd = 1 ] && [ $r = 9913 ] && echo "$out" > bank.out
  if [ "$out" = "$(cat bank.out)" ]; then echo "ok   bank $r"
  else echo "FEL  bank $r"; echo "$out" | diff bank.out - | head -20; fail=1; fi
  if [ ! -f $T ]; then echo "prov.sh: $T saknas, inget prov med TKN80"; continue; fi
  t80=$(kor $rom "-tkn80 $T")
  if [ "$(echo "$t80" | tr -d ' \n')" = "$(tr -d ' \n' < bank.out)" ]; then echo "ok   bank $r TKN80 80"
  else echo "FEL  bank $r TKN80 80"; echo "$t80" | diff bank.out - | head -20; fail=1; fi
done
exit $fail
