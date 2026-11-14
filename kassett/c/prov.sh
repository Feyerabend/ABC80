#!/bin/sh
# prov.sh  jämför C-verktygen med Python-verktygen byte för byte.
#
#   sh prov.sh [WAV ...]
#
# Utan argument används påhittade filer. Riktiga band kan ges som
# argument, eller läggas i katalogen $BAND (standard ../band). BAC-filer
# för bac2bas kan läggas i katalogen $BAC.
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
open(d + '/UTF8.TXT', 'wb').write('Räksmörgås ÅÄÖ é ü É Ü € ×\nrad\r\n'.encode())
open(d + '/DOS.TXT', 'wb').write(b'R\x84ksm\x94rg\x86s \x8e\x99\x8f \x82\r\n')
open(d + '/TRASIG.TXT', 'wb').write(bytes(random.choice(
    b'A\r\n\x80\x9f\xa0\xbf\xc0\xc2\xc3\xe0\xed\xf0\xf4\xf5\xff\x84\x96')
    for _ in range(2000)))
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

echo "wav, ABC 800"
for o in "--tk 40" "--tk 129" "--tk 20 -r 22050 --bits 8" "--tk 60 -r 48000"; do
    python3 $PY/kas2wav.py $o -o $T/p.wav $T/PROV.BAS $T/DATA.BIN 2>/dev/null
    ./kas2wav $o -o $T/c.wav $T/PROV.BAS $T/DATA.BIN 2>/dev/null
    lika $T/p.wav $T/c.wav "WAV $o"
    ./kas packa ${o%% -r*} -o $T/k8.kas $T/PROV.BAS $T/DATA.BIN 2>/dev/null
    python3 $PY/wav2kas.py -v -r -o $T/pw.kas $T/c.wav 2>&1 | sed "s|$T/pw|X|" >$T/pw.txt
    ./wav2kas -v -r -o $T/cw.kas $T/c.wav 2>&1 | sed "s|$T/cw|X|" >$T/cw.txt
    lika $T/pw.kas $T/cw.kas "WAV $o avkodad (C = Python)"
    lika $T/pw.txt $T/cw.txt "WAV $o rapporten"
    lika $T/cw.kas $T/k8.kas "WAV $o tillbaka"
done
python3 $PY/kas2wav.py --abc800 -o $T/p.wav $T/p.kas 2>/dev/null
./kas2wav --abc800 -o $T/c.wav $T/p.kas 2>/dev/null
lika $T/p.wav $T/c.wav "WAV --abc800 ur .kas"
./wav2kas -o $T/cw.kas $T/c.wav >/dev/null
lika $T/cw.kas $T/p.kas "WAV --abc800 ur .kas tillbaka"

echo "bac2bas och abctext"
# samma utdata, meddelanden och slutkod
tre() {
    n=$1; shift
    python3 $PY/$n.py "$@" >$T/p.ut 2>$T/p.err; pe=$?
    ./$n "$@" >$T/c.ut 2>$T/c.err; ce=$?
    cmp -s $T/p.ut $T/c.ut && cmp -s $T/p.err $T/c.err && [ $pe = $ce ]
}
for f in $T/SPEL.BAC ${BAC:+"$BAC"/*.BAC "$BAC"/*.bac}; do
    [ -f "$f" ] || continue
    s=0
    for kod in 7 dos utf8; do for rad in cr crlf lf; do
        tre bac2bas --kod $kod --rad $rad "$f" || s=1
    done; done
    if [ $s = 0 ]; then ok "bac2bas $(basename "$f")"; else nej "bac2bas $(basename "$f")"; fi
done
for f in PROV.BAS DATA.BIN TOM.BIN UTF8.TXT DOS.TXT TRASIG.TXT; do
    s=0
    for r in "" "--abc"; do for kod in "" 7 dos utf8; do for rad in "" cr crlf lf; do
        tre abctext $r ${kod:+--kod $kod} ${rad:+--rad $rad} $T/$f || s=1
    done; done; done
    if [ $s = 0 ]; then ok "abctext $f"; else nej "abctext $f"; fi
done
tre abctext --abc --rad lf $T/UTF8.TXT $T/DOS.TXT $T/PROV.BAS && ok "abctext flera filer" || nej "abctext flera filer"

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
