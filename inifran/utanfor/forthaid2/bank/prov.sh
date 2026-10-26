#!/bin/bash
# Provet för fjärrorden (steg 3 i ../PLAN.md, utanför boken). fc2.py
# kompilerar bokens exempel 7 med tolken (e7.fs, e7i.fs) och prov/f*.fs
# till den fasta delen på $4000 och prov/b*.fs till bank 1 och 2 på
# $5000; bank 0 är tom. Emulatorns -b är bankkortet (../kort/prov.sh).
#
# PROV (ingång 5) anropar fram och tillbaka mellan bankerna och den
# fasta delen och skriver vilken bank som är inkopplad i varje ord;
# TID (ingång 6) mäter 10 000 anrop med klockan ($FDF0, 50 Hz). Sedan
# tolken (ingång 3): ett bankat ord körs, ett nytt ord i RAM anropar
# det, och ett IMMEDIATE-ord i bank 2 körs medan ett ord kompileras.
# Körs i 9913 och 11273, och med TKN80 i 80-teckensläget, där skärmen
# ska ha samma tecken. ./prov.sh -u skriver prov.out.
cd "$(dirname "$0")" || exit 1
E=../../../emulator
if [ -d ../../../bok/band2/exempel ]; then
  BOK=../../../bok/band2/exempel
  Z80ASM=../../../dis/build/z80asm
  [ -x "$Z80ASM" ] || (cd ../../../dis/04 && ./z80asm.sh ../build/z80asm) || exit 1
  BAS=0
else
  BOK=../../../forth
  Z80ASM=../../../verktyg/z80asm
  [ -x "$Z80ASM" ] || make -C ../../../verktyg z80asm > /dev/null || exit 1
  BAS=$((0x4000))
fi
make -C $E abc80 > /dev/null || exit 1
B=bygg; rm -rf $B && mkdir $B || exit 1
cp $BOK/e7fc.py $BOK/e7.fs $BOK/e7i.fs prov/*.fs $B/ || exit 1
sed -e 's/^SAVESP: EQU  \$7C78 /SAVESP: EQU  $FB6C /' \
    -e 's/^S0:     EQU  \$7C7A /S0:     EQU  $FB6E /' \
    $BOK/e7k.inc | cat - fb.inc > $B/kärna.inc || exit 1
(cd $B && python3 ../fc2.py -H -n '$C000' -e e7fc.py -k kärna.inc -o p \
   e7.fs e7i.fs f1.fs b1a.fs@1 f2.fs b2.fs@2 b1b.fs@1 f3.fs) || exit 1
# asm namn.asm: assemblera, ut = bytes från $4000 i namn.full
asm() {
  if [ $BAS = 0 ]; then
    $Z80ASM -l -o $B/$1.full $B/$1.asm > $B/$1.lst || { grep -i error $B/$1.lst; exit 1; }
  else
    $Z80ASM -l $B/$1.lst -o $B/$1.full $B/$1.asm || exit 1
  fi
}
asm p
tail -c +$((0x4000 - BAS + 1)) $B/p.full > $B/fast.bin || exit 1
n=$(wc -c < $B/fast.bin | tr -d ' ')
[ "$n" -le 4096 ] || { echo "prov.sh: den fasta delen är $n byte, över 4 KB"; exit 1; }
banker=
while read -r b nord ntab; do
  asm p_b$b
  tail -c +$((0x5000 - BAS + 1)) $B/p_b$b.full > $B/b$b.bin || exit 1
  m=$(wc -c < $B/b$b.bin | tr -d ' ')
  [ "$m" -le 2048 ] || { echo "prov.sh: bank $b är $m byte, över 2 KB"; exit 1; }
  echo "prov.sh: bank $b, $m byte, $nord ord, $ntab i tabellen"
  banker=$banker${banker:+,}$B/b$b.bin
done < $B/p.banker
echo "prov.sh: den fasta delen $n byte"
# Två körningar, så att var och en får plats på skärmen.
K1='Z=CALL(16384,5)\r\w300\.Z=CALL(16384,6)\r'
K2='POKE 65052,0,194\rNEW\rZ=CALL(16384,3)\r\w100\.0 A1 . 1 B2 .\r\w100\.'
K2="$K2"': NY 7 B2 . ; NY\r\w100\.: X IMM2 99 . ; X\r\w100\.BYE\r'
upd=0; [ "$1" = "-u" ] && upd=1
kor() { eval "$E/abc80 -r $1 $2 -l $B/fast.bin@4000 -b $banker@5000 -k '$3' -t $4" | sed 's/ *$//' | grep -v '^$'; }
# Mätraden (TID) jämförs inte: klockan kan skilja en tick mellan
# ROM:arna. Den skrivs i stället om till T-cykler per varv, med det
# tomma varvet (TL) dragit ifrån: T0 ett vanligt anrop i den fasta
# delen, T1 i banken, T2 från bank 1 till bank 2, T3 från den fasta
# delen till bank 1. En tick är 20 ms à 2,9952 MHz, på 10 000 varv.
tid() { echo "$1" | sed -n '/^Z=CALL(16384,6)$/{n;p;}'; }
utan() { echo "$1" | sed '/^Z=CALL(16384,6)$/{n;d;}'; }
cykler() {
  set -- $(tid "$1")
  [ $# -ge 5 ] || { echo "ingen mätning"; return; }
  for k in 2 3 4 5; do
    eval "x=\${$k}"
    printf 'T%d %d  ' $((k - 2)) $(( (x - $1) * 59904 / 10000 ))
  done
  echo "(T per anrop, ±6)"
}
fail=0
T=$E/roms/tkn80-iii.rom
# jfr namn utdata: jämför med namn.out (utan mätraden), med TKN80 utan
# blanktecken och radslut.
jfr() {
  local u; u=$(utan "$(cat $1.out)")
  if [ -n "$t80" ]; then u=$(echo "$u" | tr -d ' \n'); [ "$(utan "$2" | tr -d ' \n')" = "$u" ]
  else [ "$(utan "$2")" = "$u" ]; fi
}
for r in 9913 11273; do
  rom=$E/roms/abc80new.rom; [ $r = 11273 ] && rom=$E/roms/abc80old.rom
  [ -f $rom ] || { echo "prov.sh: $rom saknas (hamta.py i roms/)"; exit 1; }
  for t80 in "" "-tkn80 $T"; do
    if [ -n "$t80" ] && [ ! -f $T ]; then echo "prov.sh: $T saknas, inget prov med TKN80"; continue; fi
    namn="$r${t80:+ TKN80 80}"
    for p in prov tolk; do
      if [ $p = prov ]; then out=$(kor $rom "$t80" "$K1" 16000); else out=$(kor $rom "$t80" "$K2" 500); fi
      [ $upd = 1 ] && [ $r = 9913 ] && [ -z "$t80" ] && echo "$out" > $p.out
      if jfr $p "$out"; then
        echo "ok   $p $namn$([ $p = prov ] && echo ": $(cykler "$out")")"
      else
        echo "FEL  $p $namn"; diff <(utan "$(cat $p.out)") <(utan "$out") | head -20; fail=1
      fi
    done
  done
done
exit $fail
