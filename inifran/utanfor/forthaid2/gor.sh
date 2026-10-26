#!/bin/sh
# Gör Forthaid II: Forthaid (../forthaid) flyttad ur TKN80:s väg och
# delad på en fast del och banker. Utanför boken, se LASMIG.md.
#
# Forthaids filer och bokens filer ändras inte. gor.sh kopierar dem till
# bygg/ och ändrar kopiorna:
#   - variablerna ligger inte längre på $7C78-$7F7F, som hamnar mitt i
#     raderna 12-15 i 80-teckensläget, utan i de gömställen som klarar
#     sig i båda lägena ($7CF8-$7EFF; $7FF8- är exempel 1:s) och resten
#     i kassettbufferten efter radbufferten ($FB50-$FB6F); de nya orden
#     börjar därför på $FB70 i stället för $FB50;
#   - med banker (steg 4 i PLAN.md): den fasta delen på $4000-$4FFF med
#     KTAB på $4F34 (I = $4F), och bankerna i fönstret $5000-$57FF.
#     banker.txt säger vilka ord som ligger i vilken bank, och
#     bank/fc2.py kompilerar; bank/fb.inc (fjärrorden) läggs i kärnan;
#   - utan banker (-1, steg 1): kretsen slutar på $57FF, under TKN80:s
#     bildminne ($5800-$5FFF), med KTAB på $5734 (I = $57), och
#     kommandona i TABORT tas bort, med de ord som bara de använder.
#
#     ./gor.sh        (forthaid2.bin, den fasta delen, och forthaid2_bN.bin,
#                      bankerna, med .lst; ordfilerna på exemplens skiva
#                      sparas om, se slutet)
#     ./gor.sh -p     (kör också proven i prov/)
#     ./gor.sh -1     (steg1.bin och steg1.lst, utan banker; -1 -p proven)
#     TABORT="PEEK" ./gor.sh -1   (prövar ett annat urval)
cd "$(dirname "$0")" || exit 1
STEG1=; [ "$1" = "-1" ] && { STEG1=1; shift; }
: "${TABORT=VAR REN OLD CHANGE}"
FA=../forthaid
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
if [ -n "$STEG1" ]; then
  UT=steg1; I=57
else
  UT=forthaid2; I=4F
fi
rm -rf bygg $UT.bin $UT.lst ${UT}_b*.bin ${UT}_b*.lst && mkdir bygg || exit 1
cp $BOK/e7fc.py $BOK/e7.fs $BOK/e7i.fs $FA/fa.fs bygg/ || exit 1
# Som i Forthaid: bokens exempelord och ingångar bort ur kopiorna.
python3 - bygg/e7.fs bygg/e7i.fs <<'PY' || exit 1
import re, sys
for p in sys.argv[1:]:
    s = open(p, encoding="utf-8").read()
    s = re.sub(r"(?m)^ENTRY .*\n", "", s)
    s = re.sub(r"(?s): MAIN .*?: TIO  7 3 \+ ;\n", "", s)
    s = re.sub(r"(?s): MATCH .*?REPEAT DROP CR ;\n", "", s)
    open(p, "w", encoding="utf-8").write(s)
PY
# Kärnan: BYE heter (BYE), SAVESP och S0 flyttas till kassettbufferten;
# med banker också fjärrorden.
FB=; [ -z "$STEG1" ] && FB="bank/fb.inc bank/fil.inc"
sed -e 's/^;W BYE BYE$/;W (BYE) BYE/' \
    -e 's/^SAVESP: EQU  \$7C78 /SAVESP: EQU  $FB6C /' \
    -e 's/^S0:     EQU  \$7C7A /S0:     EQU  $FB6E /' \
    $BOK/e7k.inc | cat - $FA/fak.inc $FB > bygg/e7k.inc || exit 1
grep -q '^;W (BYE) BYE$' bygg/e7k.inc || { echo "gor.sh: BYE finns inte i kärnan"; exit 1; }
grep -q '^SAVESP: EQU  \$FB6C' bygg/e7k.inc || { echo "gor.sh: SAVESP inte flyttad"; exit 1; }
grep -q '^S0:     EQU  \$FB6E' bygg/e7k.inc || { echo "gor.sh: S0 inte flyttad"; exit 1; }
# Kärnans (LOOKUP) bort (Forthaids eget i fak.inc). Med banker: NVSUB,
# som MATCHVAR och SETVARS delar, står i DETOK:s del av fak.inc; den
# flyttas till MATCHVAR:s, så att den hamnar i samma bank som de två.
STEG1="$STEG1" python3 - bygg/e7k.inc <<'PY' || exit 1
import os, re, sys
p = sys.argv[1]
s = open(p, encoding="utf-8").read()
s, n = re.subn(r"(?s);W \(LOOKUP\) XLOOK\n.*?(?=;W )", "", s, count=1)
if n != 1:
    sys.exit("gor.sh: (LOOKUP) finns inte i kärnan")
if os.environ["STEG1"]:
    open(p, "w", encoding="utf-8").write(s)
    sys.exit(0)
m = re.search(r"(?s)\n; NVSUB: .*?(?=\n;W MATCHVAR )", s)
if not m:
    sys.exit("gor.sh: hittar inte NVSUB före MATCHVAR")
nv = m.group(0)
s = s[:m.start()] + s[m.end():]
k = s.index("\n;W SEARCH ")
s = s[:k] + nv + s[k:]
# Editorn med 80 kolumner (steg 5): EDLIN tar radlängden från EDW, 40
# eller 80 efter ROM:ens radtabell (rad 1 på $58xx med TKN80 i
# 80-teckensläget, MYAB u.å.), i stället för 40 och 39.
for gammalt, nytt in [
    ("        DEFW F_CHANGE,F_REN,F_ED,F_START,F_HELP,F_FORTH,F_EX,0\n",
     "        DEFW F_CHANGE,F_REN,F_ED,F_START,F_HELP,F_FORTH,F_EX\n"
     "        DEFW F_SAVEW,F_LOADW,0\n"),
    ("        LD   A,(IX+1)\n        CP   40\n        CALL NC,ECRLF\n",
     "        CALL EDW            ; kolumnen = radlängden: ny rad\n"
     "        DEC  A\n        CP   (IX+1)\n        CALL C,ECRLF\n"),
    ("EDRGT:  LD   A,(IX+1)\n        CP   39\n        JR   NC,ED9\n",
     "EDRGT:  CALL EDW            ; inte förbi radens sista kolumn\n"
     "        SUB  2\n        CP   (IX+1)\n        JR   C,ED9\n"),
    ("; CTRL-D och CTRL-P flyttar resten av skärmraden, 39 - kolumnen tecken.",
     "; CTRL-D och CTRL-P flyttar resten av skärmraden, radlängden - 1 -\n"
     "; kolumnen tecken."),
    ("EDREST: LD   A,39           ; BC = 39 - kolumnen, Z om 0\n",
     "EDREST: CALL EDW            ; BC = radlängden - 1 - kolumnen, Z om 0\n"
     "        DEC  A\n"),
    ("        LD   B,0\n        RET\n\n;W (LOOKUP) ILOOK",
     "        LD   B,0\n        RET\n"
     "EDW:    LD   A,($0375)      ; A = radlängden, 40 eller 80\n"
     "        CP   $58\n        LD   A,40\n        RET  NZ\n        LD   A,80\n        RET\n"
     "\n;W (LOOKUP) ILOOK")]:
    if s.count(gammalt) != 1:
        sys.exit("gor.sh: hittar inte %r i fak.inc" % gammalt)
    s = s.replace(gammalt, nytt)
open(p, "w", encoding="utf-8").write(s)
PY
# fa.fs: STEPF, I och var de nya orden börjar.
TABORT="$TABORT" STEG1="$STEG1" I=$I python3 - bygg/fa.fs <<'PY' || exit 1
import os, re, sys
p = sys.argv[1]
s = open(p, encoding="utf-8").read()
for gammalt, nytt in [("0 $7F7E !  $5F I!", "0 $7EFE !  $%s I!" % os.environ["I"]),
                      ("DUP $FB50 U< 0=", "DUP $FB70 U< 0=")]:
    if s.count(gammalt) != 1:
        sys.exit("gor.sh: hittar inte %r i fa.fs" % gammalt)
    s = s.replace(gammalt, nytt)
# Utan banker: HELP:s lista utan kommandona i TABORT.
if os.environ["STEG1"]:
    a = s.index("\n: HELP\n")
    b = s.index("LAST @", a)
    h = s[a:b]
    for k in os.environ.get("TABORT", "").split():
        h, n = re.subn(r'(\." (?:[^"]*? )?)%s( |")' % re.escape(k),
                       lambda m: m.group(1) + (m.group(2) if m.group(2) == '"' else ""), h)
        if n != 1 and k != "HELP":
            sys.exit("gor.sh: %s inte i HELP:s lista" % k)
    s = s[:a] + h.replace(' "', '"') + s[b:]
# Med banker: LISTLINE räknar skärmraderna med COLS, 40 eller 80, som
# EDW i fak.inc (steg 5).
else:
    gammalt = "\n\\ LIST: raden görs om till text"
    nytt = ("\n\\ COLS: tecken per skärmrad, 80 om radtabellen pekar på TKN80:s\n"
            "\\ bildminne ($58xx).\n"
            ": COLS  ( -- n )  $0375 C@ $58 = IF 80 ELSE 40 THEN ;\n" + gammalt)
    # SAVEW och LOADW (steg 7, bank/fil.fs): i HELP:s lista, och HELP
    # visar också de laddade orden, som ligger under $FB70.
    for g, n in [(gammalt, nytt), ("  40 U/MOD SWAP DROP 1+ ;", "  COLS U/MOD SWAP DROP 1+ ;"),
                 ('EX FORTH" CR\n', 'EX FORTH" CR  ." SAVEW LOADW" CR\n'),
                 ("DUP $FB70 U< 0=", "DUP $8000 U< 0=")]:
        if s.count(g) != 1:
            sys.exit("gor.sh: hittar inte %r i fa.fs" % g)
        s = s.replace(g, n)
open(p, "w", encoding="utf-8").write(s)
PY
# fai.asm: STEPF och KTAB.
sed -e 's/^STEPF:  EQU  \$7F7E /STEPF:  EQU  $7EFE /' \
    -e "s/DEFS \$5F34-\$ /DEFS \$${I}34-\$ /" $FA/fai.asm > bygg/fai.asm || exit 1
grep -q '^STEPF:  EQU  \$7EFE' bygg/fai.asm || { echo "gor.sh: STEPF inte flyttad"; exit 1; }
grep -q "DEFS \$${I}34-\\$" bygg/fai.asm || { echo "gor.sh: KTAB inte flyttad"; exit 1; }

# asm namn: assemblera bygg/namn.asm till bygg/namn.full och namn.lst
asm() {
  if [ -n "$PUBLIK" ]; then
    $Z80ASM -l bygg/$1.lst -o bygg/$1.full bygg/$1.asm || exit 1
    BAS=$((0x4000))
  else
    $Z80ASM -l -o bygg/$1.full bygg/$1.asm > bygg/$1.lst || { grep -i error bygg/$1.lst; exit 1; }
    BAS=0
  fi
}

if [ -n "$STEG1" ]; then
# Korskompilatorn: en bild på $4000 med nya ord från $FB70, variablerna
# i de nya gömställena, och kommandona i TABORT bort.
python3 - bygg/e7fc.py <<'PY' || exit 1
import re, sys
p = sys.argv[1]
s = open(p, encoding="utf-8").read()
def byt(monster, nytt, flaggor=re.S):
    global s
    s, n = re.subn(monster, lambda m: nytt, s, count=1, flags=flaggor)
    if n != 1:
        sys.exit("gor.sh: hittar inte %s i e7fc.py" % monster)
byt(r"\nOUTS = \[.*?\]\n\n",
    '\nOUTS = [("fa.asm", 0x4000, ["e7.fs", "e7i.fs", "fa.fs"], True,'
    ' 0xFB70)]\n\n')
# $7CF8-$7EFD i båda lägena ($7EFE är STEPF), sedan $FB50-$FB6B.
byt(r"\nRAM0 = \[.*?\]\n",
    "\nRAM0 = ([a for b in (0x7CF8, 0x7DF8, 0x7EF8) for a in range(b, b + 8, 2)"
    " if a != 0x7EFE]\n        + list(range(0xFB50, 0xFB6C, 2)))\n")
# Efter need (allt, med huvuden): kommandona i TABORT och de av
# Forthaids egna ord (fa.fs, fak.inc) som bara de använder tas bort.
byt(r"\n        if heads:\n            break                    # need är redan allt\n",
    r"""
        if heads:
            break                    # need är redan allt
    import os
    tabort = set(os.environ.get("TABORT", "").split())
    if heads and tabort:
        egna = set(re.findall(r"(?m)^: (\S+)", open("fa.fs", encoding="utf-8").read()))
        egna |= set(re.findall(r"(?m)^;W (\S+)", open(os.environ["FAK"], encoding="utf-8").read()))
        okanda = tabort - egna
        if okanda:
            sys.exit("e7fc: inga kommandon %s" % " ".join(sorted(okanda)))
        anv = {}
        for w, body in words.items():
            for kind, x in body:
                if kind in ("w", "xw"):
                    anv.setdefault(x, set()).add(w)
                if kind == "xl" and x in labs:
                    anv.setdefault(labs[x], set()).add(w)
        for w, pr in prims.items():
            for d in pr["deps"]:
                anv.setdefault(labs.get(d, d), set()).add(w)
        bort = set(tabort)
        mer = True
        while mer:
            mer = False
            for w in egna - bort:
                a = anv.get(w, set()) - {w}
                if a and a <= bort:
                    bort.add(w)
                    mer = True
        need -= bort
        print("e7fc: bort", " ".join(sorted(bort)))
""")
open(p, "w", encoding="utf-8").write(s)
PY

(cd bygg && TABORT="$TABORT" FAK=../$FA/fak.inc python3 e7fc.py) || exit 1
# CMD? (fak.inc) har en egen tabell över kommandona.
for k in $TABORT; do
  sed -n '/^CMDTAB:/,/,0$/p' bygg/fa.asm | grep -q "F_$k\(,\|\$\)" || { echo "gor.sh: F_$k inte i CMDTAB"; exit 1; }
  sed -i.bak -e "/^CMDTAB:/,/,0\$/{s/F_$k,//;s/,F_$k\$//;}" bygg/fa.asm || exit 1
done
cat bygg/fai.asm >> bygg/fa.asm || exit 1
asm fa
tail -c +$((0x4000 - BAS + 1)) bygg/fa.full > $UT.bin || exit 1
cp bygg/fa.lst $UT.lst || exit 1
GRANS=5734; MAX=6144

else
# Med banker: fc2.py med kartan; huvuden för den fasta delens ord och för
# de bankade ord som anropas utifrån, avbrottsrutinen (fai.asm) sist i
# den fasta delen.
(cd bygg && python3 ../bank/fc2.py -h -n '$FB70' -m ../banker.txt -s fai.asm \
   -e e7fc.py -k e7k.inc -o fa e7.fs e7i.fs fa.fs ../bank/fil.fs) || exit 1
asm fa
tail -c +$((0x4000 - BAS + 1)) bygg/fa.full > $UT.bin || exit 1
cp bygg/fa.lst $UT.lst || exit 1
GRANS=4F34; MAX=4096
while read -r b nord ntab; do
  asm fa_b$b
  tail -c +$((0x5000 - BAS + 1)) bygg/fa_b$b.full > ${UT}_b$b.bin || exit 1
  sed -n '/^5000 /,$p' bygg/fa_b$b.lst > ${UT}_b$b.lst
  m=$(wc -c < ${UT}_b$b.bin | tr -d ' ')
  echo "gor.sh: bank $b, $m byte ($((2048 - m)) lediga), $nord ord, $ntab i tabellen"
  [ "$m" -le 2048 ] || { echo "gor.sh: bank $b är över 2 KB"; exit 1; }
done < bygg/fa.banker
fi

[ -n "$BEHALL" ] || rm -rf bygg
grep -q "^$GRANS .*KTAB:" $UT.lst || { echo "gor.sh: KTAB inte på \$$GRANS"; exit 1; }
if grep -q 'DEFW LIT,\(3[12][0-9][0-9][0-9]\)\b' $UT.lst; then
  for a in $(grep -o 'DEFW LIT,3[12][0-9][0-9][0-9]\b' $UT.lst | sed 's/.*,//' | sort -u); do
    [ $a -ge 31864 ] && [ $a -le 32767 ] && [ $((a & 255)) -lt 248 ] && echo "gor.sh: varning, $a i bildminnet"
  done
fi
grep -q '7C7[89A-F]\|7D7[89A-F]\|7E7[89A-F]\|7F7[89A-F]' $UT.lst && echo "gor.sh: varning, en adress \$7x78-\$7x7F i listningen"
n=$(wc -c < $UT.bin | tr -d ' ')
d=$(grep "DEFS \$$GRANS-\\$" $UT.lst | cut -c1-4)
echo "gor.sh: $UT.bin ($n bytes; lediga: $((0x$GRANS - 0x$d)) före KTAB, $((MAX - n)) sist)"
[ "$n" -le $MAX ] || { echo "gor.sh: större än $MAX byte"; exit 1; }
# Ordfilerna på exemplens skiva (VERKTYG.FTH) hör till den Forthaid II
# de gjordes med (LOADW prövar LATEST), så de sparas om med den nya.
# Webbsidan görs sedan om med make webb i emulator/.
SPARA=../../emulator/exempel/program/forth/spara.sh
if [ -z "$STEG1" ] && [ -x $SPARA ]; then
  $SPARA || { echo "gor.sh: ordfilerna kunde inte sparas om"; exit 1; }
fi
[ "$1" = "-p" ] && exec ./prov/kor.sh ${STEG1:+-1}
exit 0
