#!/bin/sh
# abc802.sh  prov mot ROM:en i ABC 802-emulatorn (BASIC II).
#
#   sh prov/abc802.sh [EMULATOR]
#
# EMULATOR är programmet abc802 (standard $ABC802, annars
# ~/Documents/abc802/emulator/abc802). Dess band är tecknen som går förbi
# huvudet (emulator/karna/kassett802.h), inte en signal, så skriptet gör
# om mellan dem och blockfiler: $00 i pausen (700 tecken, som när ROM:en
# skriver), $16 $02, blocket, ETX, summan och $FF $FF.
#
# 1. ROM:en skriver tre band: ett program med SAVE CAS: (BAC), samma
#    program med LIST CAS: (text) och en datafil med PREPARE och PRINT #.
# 2. Filerna packas igen med kas packa --tk 40 (Python och C). BAC och
#    LIST ska ge samma block som ROM:en, byte för byte, också när
#    programmet packas direkt ur källan. Datafilen ska ge samma text;
#    blocken kan skilja, för ROM:en packar inte blanksteg över gränsen
#    mellan två utskrifter (PRINT #1,"RAD";I;"  " skickar tre).
# 3. ROM:en läser de packade filerna (LOAD och RUN, OPEN och INPUT
#    LINE #). Skärmen ska bli densamma som med ROM:ens egna band.

set -u
here=$(cd "$(dirname "$0")/.." && pwd)
emu=${1:-${ABC802:-$HOME/Documents/abc802/emulator/abc802}}
py="python3 $here/python"
c="$here/c"
[ -x "$emu" ] || { echo "hittar inte emulatorn: $emu" >&2; exit 2; }
[ -x "$c/kas" ] || (cd "$c" && make -s) || exit 2

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cd "$tmp"
fel=0
ok()  { echo "  ok    $1"; }
nej() { echo "  FEL   $1"; fel=1; }
lika() { if cmp -s "$2" "$3"; then ok "$1"; else nej "$1"; fi; }

# emulatorns band <-> blockfil
cat > band.py <<'EOF'
import sys
riktning, fran, till = sys.argv[1:]
d = open(fran, 'rb').read()
ut = bytearray()
if riktning == 'in':                    # blockfil -> band
    ut += bytes(600)
    for i in range(0, len(d), 256):
        b = d[i:i + 256]
        s = (3 + sum(b)) & 0xFFFF
        ut += bytes(700) + b'\x16\x02' + b + bytes([3, s & 255, s >> 8, 255, 255])
    ut += bytes(100)
else:                                   # band -> blockfil
    i = 0
    while True:
        j = d.find(b'\x16\x02', i)
        if j < 0:
            break
        b = d[j + 2:j + 258]
        s = (3 + sum(b)) & 0xFFFF
        if d[j + 258] != 3 or d[j + 259] | d[j + 260] << 8 != s:
            sys.exit('fel block vid %d' % j)
        ut += b
        i = j + 261
open(till, 'wb').write(ut)
EOF

{
    echo '10 REM PROV FOR KASSETTEN'
    i=2
    while [ $i -lt 60 ]; do
        echo "$((i * 10)) PRINT \"RAD $i   MED    BLANKSTEG\";$i*$i"
        i=$((i + 1))
    done
    echo '600 PRINT "SLUT"'
} > prov.bas

cat > skriv.bas <<'EOF'
10 PREPARE "CAS:DATA" AS FILE 1
20 PRINT #1,"ETT BLANKSTEG SIST "
30 PRINT #1,"TVA SIST  "
40 PRINT #1,"MANGA          INUTI"
50 PRINT #1,"[\]{|}@"
60 FOR I=1 TO 40
70 PRINT #1,"RAD";I;"  ";STRING$(I*2,65)
80 NEXT I
90 PRINT #1,"   INLEDANDE"
100 PRINT #1,""
110 CLOSE 1
EOF
cat > las.bas <<'EOF'
10 DIM A$=200 : OPEN "CAS:DATA" AS FILE 1
20 FOR N=1 TO 46
30 INPUT LINE #1,A$
40 S=S+LEN(A$)
50 PRINT N;LEN(A$);
60 NEXT N
70 PRINT "SLUT";S
EOF

echo "ROM:en skriver"
"$emu" -f prov.bas -k 'SAVE CAS:PROV\r' -U bac.band -t 90000 > /dev/null
"$emu" -f prov.bas -k 'LIST CAS:PROV\r' -U txt.band -t 90000 > /dev/null
"$emu" -f skriv.bas -k 'RUN\r' -U data.band -t 90000 > /dev/null

for b in bac txt data; do
    python3 band.py ut $b.band $b.kas || nej "$b: bandet gick inte att läsa"
    mkdir -p ut$b
    $py/kas.py upp -d ut$b $b.kas 2> /dev/null
    f=$(ls ut$b)
    $py/kas.py packa --tk 40 -o p$b.kas ut$b/$f 2> /dev/null
    "$c/kas" packa --tk 40 -o q$b.kas ut$b/$f 2> /dev/null
    lika "$b: Python och C packar lika" p$b.kas q$b.kas
    if [ $b = data ]; then
        mkdir -p igen
        $py/kas.py upp -d igen p$b.kas 2> /dev/null
        lika "$b: $f packad igen ger samma text" ut$b/$f igen/$f
    else
        lika "$b: $f packad igen = ROM:ens block" p$b.kas $b.kas
    fi
done
$py/kas.py packa --tk 40 -o kalla.kas prov.bas 2> /dev/null
lika "txt: prov.bas packad ur källan = LIST CAS:" kalla.kas txt.kas

echo "ROM:en läser"
for b in bac txt data; do
    for k in $b p$b; do
        python3 band.py in $k.kas $k.in
        case $b in
            bac)  "$emu" -k 'LOAD CAS:PROV\r\w90000RUN\r' -T $k.in -t 5000 ;;
            txt)  "$emu" -k 'LOAD CAS:PROV.BAS\r\w90000RUN\r' -T $k.in -t 5000 ;;
            data) "$emu" -f las.bas -k 'RUN\r' -T $k.in -t 90000 ;;
        esac | grep -v '^ *$' > $k.skarm
    done
    grep -q SLUT $b.skarm || nej "$b: ROM:ens eget band gick inte att läsa"
    lika "$b: packad igen läst av ROM:en, samma skärm" $b.skarm p$b.skarm
done

if [ $fel = 0 ]; then echo "allt lika"; else echo "skillnader"; fi
exit $fel
