#!/bin/sh
# Gör forthaid.bin: Forth-tolken från exempel 7 som krets på $4000, med
# kommandon i stil med Smartaid (fa.fs, fak.inc). Utanför boken, se
# LASMIG.md, MANUAL.md och ../README.md.
#
# Bokens filer ändras inte. Kärnan (e7k.inc), korskompilatorn (e7fc.py)
# och Forth-källorna kopieras till bygg/. Kopian av kärnan får de nya
# maskinkodsorden i fak.inc sist, och kopian av e7fc.py bygger bara en
# bild, fa.asm, ur e7.fs, e7i.fs och fa.fs med huvuden, på $4000.
# Ord i fa.fs med samma namn som i bokens källor ersätter dem.
#
#     ./gor.sh              (skriver forthaid.bin och forthaid.lst)
#     ./gor.sh -p           (kör också proven i prov/)
cd "$(dirname "$0")" || exit 1
if [ -d ../../bok/band2/exempel ]; then
  BOK=../../bok/band2/exempel
  Z80ASM=../../dis/build/z80asm
  [ -x "$Z80ASM" ] || (cd ../../dis/04 && ./z80asm.sh ../build/z80asm) || exit 1
else
  PUBLIK=1
  BOK=../../forth
  Z80ASM=../../verktyg/z80asm
  [ -x "$Z80ASM" ] || make -C ../../verktyg z80asm > /dev/null || exit 1
fi
rm -rf bygg && mkdir bygg || exit 1
cp $BOK/e7fc.py $BOK/e7.fs $BOK/e7i.fs fa.fs bygg/ || exit 1
# Bokens exempelord (MAIN, TIO och FIND med MATCH och LINE?) och
# ingångarna tas bort ur kopiorna; fa.fs har egna, med FORTH som
# ingång 0, Z=CALL(16384).
python3 - bygg/e7.fs bygg/e7i.fs <<'PY' || exit 1
import re, sys
for p in sys.argv[1:]:
    s = open(p, encoding="utf-8").read()
    s = re.sub(r"(?m)^ENTRY .*\n", "", s)
    s = re.sub(r"(?s): MAIN .*?: TIO  7 3 \+ ;\n", "", s)
    s = re.sub(r"(?s): MATCH .*?REPEAT DROP CR ;\n", "", s)
    open(p, "w", encoding="utf-8").write(s)
PY
# Kärnans BYE heter (BYE) i kopian, så att BYE i fa.fs kan gå tillbaka
# till BASIC på två sätt (etiketten BYE, som BOOT använder, är kvar).
sed 's/^;W BYE BYE$/;W (BYE) BYE/' $BOK/e7k.inc | cat - fak.inc > bygg/e7k.inc || exit 1
grep -q '^;W (BYE) BYE$' bygg/e7k.inc || { echo "gor.sh: BYE finns inte i kärnan"; exit 1; }
# Kärnans (LOOKUP) tas bort ur kopian; fak.inc har ett eget, som inte
# skiljer på versaler och gemener.
python3 - bygg/e7k.inc <<'PY' || exit 1
import re, sys
p = sys.argv[1]
s = open(p, encoding="utf-8").read()
s, n = re.subn(r"(?s);W \(LOOKUP\) XLOOK\n.*?(?=;W )", "", s, count=1)
if n != 1:
    sys.exit("gor.sh: (LOOKUP) finns inte i kärnan")
open(p, "w", encoding="utf-8").write(s)
PY

# Bara en bild: tolken och kommandona på $4000, nya ord i RAM (FORTH i
# fa.fs sätter HERE, SLUT används inte).
python3 - bygg/e7fc.py <<'PY' || exit 1
import re, sys
p = sys.argv[1]
s = open(p, encoding="utf-8").read()
s = re.sub(r"(?s)\nOUTS = \[.*?\]\n\n",
           '\nOUTS = [("fa.asm", 0x4000, ["e7.fs", "e7i.fs", "fa.fs"], True,'
           ' 0xFB50)]\n\n', s, count=1)
open(p, "w", encoding="utf-8").write(s)
PY

(cd bygg && python3 e7fc.py) || exit 1
# Avbrottsrutinen sist, med vektortabellen på $5F34 (sidan $5F).
cat fai.asm >> bygg/fa.asm || exit 1
if [ -n "$PUBLIK" ]; then
  $Z80ASM -l forthaid.lst -o forthaid.bin bygg/fa.asm || exit 1
else
  $Z80ASM -l -o bygg/fa.full bygg/fa.asm > forthaid.lst || exit 1
  tail -c +$((0x4000 + 1)) bygg/fa.full > forthaid.bin || exit 1
fi
rm -rf bygg
[ "$(grep -c 'DEFW LIT,32638\b' forthaid.lst)" = 1 ] || { echo "gor.sh: en variabel på STEPF (\$7F7E), bara PA får nämna den"; exit 1; }
grep -q '^5F34 .*KTAB:' forthaid.lst || { echo "gor.sh: KTAB inte på \$5F34"; exit 1; }
n=$(wc -c < forthaid.bin | tr -d ' ')
echo "gor.sh: forthaid.bin ($n bytes)"
[ "$n" -le 8192 ] || { echo "gor.sh: större än 8 KB"; exit 1; }
[ "$1" = "-p" ] && exec ./prov/kor.sh
exit 0
