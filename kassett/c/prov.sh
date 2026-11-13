#!/bin/sh
# prov.sh  jämför C-verktygen med Python-verktygen byte för byte.
#
#   sh prov.sh [WAV ...]
#
# Utan argument används påhittade filer. Riktiga band kan ges som
# argument, eller läggas i katalogen $BAND (standard ../band).
set -u
cd "$(dirname "$0")"
PY=../python
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
fel=0
ok()  { echo "  ok    $1"; }
nej() { echo "  FEL   $1"; fel=1; }
lika() { if cmp -s "$1" "$2"; then ok "$3"; else nej "$3"; fi; }

# påhittade filer: text med blanksteg, LF, ÅÄÖ och långa rader, binärt, BAC
python3 - "$T" <<'PYEOF'
import random, sys
d = sys.argv[1]
random.seed(7)
open(d + '/PROV.BAS', 'wb').write(
    b'10 PRINT "HEJ   D]"   \r\n20 FOR I=1 TO 10 : PRINT I : NEXT I\n'
    b'30 REM ' + b' ' * 300 + b'SLUT\r\n')
open(d + '/LANG.BAS', 'wb').write(b''.join(
    b'%d PRINT "RAD %d"   ;X  ,Y\r\n' % (i * 10, i) for i in range(1, 400)))
open(d + '/DATA.BIN', 'wb').write(bytes(random.randrange(256) for _ in range(253 * 3)))
open(d + '/UDDA.BIN', 'wb').write(bytes(random.randrange(256) for _ in range(1000)))
open(d + '/TOM.BIN', 'wb').write(b'')
bac = bytearray(random.randrange(256) for _ in range(512)); bac[0] = 0x82
for s in (253, 509): bac[s:s + 3] = bytes(3)   # når aldrig bandet
open(d + '/SPEL.BAC', 'wb').write(bac)
open(d + '/MÅLARE.BAS', 'wb').write(b'10 END\r\n')
PYEOF

echo "packa"
F="$T/PROV.BAS $T/LANG.BAS $T/DATA.BIN $T/UDDA.BIN $T/TOM.BIN $T/SPEL.BAC $T/MÅLARE.BAS"
python3 $PY/kas.py packa -o $T/p.kas $F 2>$T/p.err
./kas packa -o $T/c.kas $F 2>$T/c.err
lika $T/p.kas $T/c.kas "blockfilen"
lika $T/p.err $T/c.err "meddelandena"
python3 $PY/kas.py packa -t --tk 40 -o $T/p2.kas $T/DATA.BIN 2>/dev/null
./kas packa -t --tk 40 -o $T/c2.kas $T/DATA.BIN 2>/dev/null
lika $T/p2.kas $T/c2.kas "-t och --tk"
python3 $PY/kas.py packa --tk 40 -o $T/p3.kas $T/PROV.BAS $T/LANG.BAS 2>/dev/null
./kas packa --tk 40 -o $T/c3.kas $T/PROV.BAS $T/LANG.BAS 2>/dev/null
lika $T/p3.kas $T/c3.kas "text med --tk (BASIC II)"

echo "lista"
python3 $PY/kas.py lista $T/p.kas | sed "s|$T/||" >$T/p.lst
./kas lista $T/p.kas | sed "s|$T/||" >$T/c.lst
lika $T/p.lst $T/c.lst "listan"

echo "upp"
python3 $PY/kas.py upp -d $T/pu $T/p.kas 2>/dev/null
./kas upp -d $T/cu $T/p.kas 2>/dev/null
if diff -r $T/pu $T/cu >/dev/null; then ok "filerna"; else nej "filerna"; fi
for f in DATA.BIN SPEL.BAC; do lika $T/$f $T/cu/$f "$f tillbaka"; done

echo "wav"
for o in "-r 44100 --bits 16" "-r 22050 --bits 8" "-r 8000 --bits 8 --lead 6.8 --gap 0.1"; do
    python3 $PY/kas2wav.py $o -o $T/p.wav $T/p.kas 2>/dev/null
    ./kas2wav $o -o $T/c.wav $T/p.kas 2>/dev/null
    lika $T/p.wav $T/c.wav "WAV $o"
    ./wav2kas -o $T/cw.kas $T/c.wav >/dev/null
    lika $T/cw.kas $T/p.kas "WAV $o tillbaka (C)"
done

band() {
    for w in "$@"; do
        [ -f "$w" ] || continue
        n=$(basename "$w" .wav)
        mkdir -p $T/pb $T/cb
        python3 $PY/wav2kas.py -r -o $T/pb/band.kas "$w" | sed "s|$T/pb/||" >$T/pb.txt
        ./wav2kas -r -o $T/cb/band.kas "$w" | sed "s|$T/cb/||" >$T/cb.txt
        if cmp -s $T/pb/band.kas $T/cb/band.kas && cmp -s $T/pb.txt $T/cb.txt; then
            ok "$n: $(grep -c ' ok$' $T/cb.txt) block, rapporten lika"
        else
            nej "$n"
            diff $T/pb.txt $T/cb.txt | head -5
        fi
    done
}
if [ $# -gt 0 ]; then
    echo "band"; band "$@"
elif [ -d "${BAND:-../band}" ]; then
    echo "band"; band "${BAND:-../band}"/*.wav
fi

[ $fel = 0 ] && echo "allt lika" || echo "skillnader finns"
exit $fel
