#!/bin/sh
# abc80.sh  prov mot ROM:en i ABC80-emulatorn (11273, abc80new.rom).
#
#   sh prov/abc80.sh [EMULATOR]
#
# EMULATOR är programmet abc80 (standard $ABC80, annars
# ~/Documents/abc80/emulator/abc80). Det spelar in och spelar upp WAV.
#
# 1. ROM:en skriver tre band: ett program med SAVE CAS: (BAC), samma
#    program med LIST CAS: (text) och en datafil med PREPARE och PRINT #.
# 2. wav2kas (Python och C) läser dem; blocken ska vara lika, och filerna
#    packade igen med kas packa (Python och C) ska ge samma block som
#    ROM:en skrev, byte för byte. Textprogrammet packas också direkt ur
#    källan.
#    Ett program till med fler satser och uttryck sparas och listas;
#    bac2bas (Python och C) på BAC-filerna ska ge samma text som LIST CAS:.
# 3. kas2wav (Python 44,1 kHz 16 bitar, C 11 025 Hz 8 bitar) skriver
#    filerna på nya band, och ROM:en läser dem (LOAD och RUN, OPEN och
#    INPUT LINE #). Skärmen ska bli densamma som med ROM:ens egna band.
#
# Det tar några minuter (banden går i realtid för maskinen, men
# emulatorn kör fortare än så).

set -u
here=$(cd "$(dirname "$0")/.." && pwd)
emu=${1:-${ABC80:-$HOME/Documents/abc80/emulator/abc80}}
py="python3 $here/python"
c="$here/c"
[ -x "$emu" ] || { echo "hittar inte emulatorn: $emu" >&2; exit 2; }
[ -x "$c/kas" ] && [ -x "$c/bac2bas" ] || (cd "$c" && make -s) || exit 2

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cd "$tmp"
fel=0
ok()  { echo "  ok    $1"; }
nej() { echo "  FEL   $1"; fel=1; }
lika() { if cmp -s "$2" "$3"; then ok "$1"; else nej "$1"; fi; }

# Programmet: blanksteg inuti strängar ($09 n) och flera block.
{
    echo '10 REM PROV FOR KASSETTEN'
    i=2
    while [ $i -lt 60 ]; do
        echo "$((i * 10)) PRINT \"RAD $i   MED    BLANKSTEG\";$i*$i"
        i=$((i + 1))
    done
    echo '600 PRINT "SLUT"'
} > prov.bas

# Datafilen: blanksteg sist på raden (ett och två), en lång följd,
# svenska tecken, långa rader och en tom rad.
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

# Ett program med fler satser och uttryck, för bac2bas.
cat > uttr.bas <<'EOF'
10 DIM A(10),B$=20
20 A=1.5E-3 : B%=-7 : C$="TEXT  MED  BLANKSTEG"
30 IF A<B% OR NOT A>=2 THEN PRINT SIN(A);COS(A);SQR(4)
40 FOR I=1 TO 10 STEP 2 : X=X+I^2 : A(I)=X/3 : NEXT I
50 PRINT LEFT$(C$,2);RIGHT$(C$,3);CHR$(65);ASC("A");LEN(C$)
60 GOSUB 100 : ON B%+8 GOTO 70,80
70 PRINT TAB(5);INT(-3.7);ABS(B%);SGN(A);"[\]{|}"
80 A=123456789 : B=0.000001 : C=1E30 : D=-.5
90 REM KOMMENTAR MED   BLANKSTEG
100 RETURN
EOF

echo "ROM:en skriver"
"$emu" -f prov.bas -k 'SAVE CAS:PROV\r' -U bac.wav -t 100 > /dev/null
"$emu" -f prov.bas -k 'LIST CAS:PROV\r' -U txt.wav -t 100 > /dev/null
"$emu" -f skriv.bas -k 'RUN\r' -U data.wav -t 100 > /dev/null
"$emu" -f uttr.bas -k 'SAVE CAS:UTTR\r' -U ubac.wav -t 100 > /dev/null
"$emu" -f uttr.bas -k 'LIST CAS:UTTR\r' -U utxt.wav -t 100 > /dev/null

for b in bac txt data; do
    $py/wav2kas.py -o $b.kas $b.wav > /dev/null 2>&1
    "$c/wav2kas" -o c$b.kas $b.wav > /dev/null 2>&1
    lika "$b: wav2kas, Python och C lika" $b.kas c$b.kas
    mkdir -p ut$b
    $py/kas.py upp -d ut$b $b.kas 2> /dev/null
    f=$(ls ut$b)
    $py/kas.py packa -o p$b.kas ut$b/$f 2> /dev/null
    "$c/kas" packa -o q$b.kas ut$b/$f 2> /dev/null
    lika "$b: $f packad igen (Python) = ROM:ens block" p$b.kas $b.kas
    lika "$b: $f packad igen (C) = ROM:ens block" q$b.kas $b.kas
done
$py/kas.py packa -o kalla.kas prov.bas 2> /dev/null
lika "txt: prov.bas packad ur källan = LIST CAS:" kalla.kas txt.kas

# bac2bas: programmet som ROM:en sparade ska bli texten den listade
for b in ubac utxt; do
    $py/wav2kas.py -o $b.kas $b.wav > /dev/null 2>&1
    mkdir -p ut$b
    $py/kas.py upp -d ut$b $b.kas 2> /dev/null
done
grep -q RETURN ututxt/UTTR.BAS 2> /dev/null || nej "UTTR: LIST CAS: gav inget program"
for k in "$py/bac2bas.py" "$c/bac2bas"; do
    n=${k##*/}
    $k --kod 7 --rad cr utubac/UTTR.BAC > u.txt 2> /dev/null
    lika "bac2bas ($n): UTTR.BAC = LIST CAS:" u.txt ututxt/UTTR.BAS
    $k --kod 7 --rad cr utbac/PROV.BAC > p.txt 2> /dev/null
    lika "bac2bas ($n): PROV.BAC = LIST CAS:" p.txt uttxt/PROV.BAS
done

echo "ROM:en läser"
las() {     # band, tangenter, [program]
    if [ -n "${3:-}" ]; then
        "$emu" -f "$3" -k "$2" -T "$1" -t 3000
    else
        "$emu" -k "$2" -T "$1" -t 3000
    fi
}
for b in bac txt data; do
    f=$(ls ut$b)
    $py/kas2wav.py -o py$b.wav ut$b/$f 2> /dev/null
    "$c/kas2wav" -r 11025 --bits 8 -o c$b.wav ut$b/$f 2> /dev/null
    for w in $b py$b c$b; do
        case $b in
            bac)  las $w.wav 'LOAD CAS:PROV\r\w90000\.RUN\r' ;;
            txt)  las $w.wav 'LOAD CAS:PROV.BAS\r\w90000\.RUN\r' ;;
            data) las $w.wav 'RUN\r' las.bas ;;
        esac > $w.skarm
    done
    grep -q SLUT $b.skarm || nej "$b: ROM:ens eget band gick inte att läsa"
    lika "$b: kas2wav (Python) läst av ROM:en, samma skärm" $b.skarm py$b.skarm
    lika "$b: kas2wav (C, 11 kHz 8 bitar) läst av ROM:en, samma skärm" $b.skarm c$b.skarm
done

if [ $fel = 0 ]; then echo "allt lika"; else echo "skillnader"; fi
exit $fel
