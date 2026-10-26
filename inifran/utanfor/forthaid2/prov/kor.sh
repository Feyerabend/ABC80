#!/bin/bash
# Kör proven: varje prov/N.test är argument till en emulator och körs
# mot 9913 och 11273; skärmen jämförs med N.out (eller N.<rom>.out).
# Finns bokens värdemulator (dis/04/host) används den, annars emulatorn
# i emulator/ med ROM:arna i emulator/roms (hämtas med hamta.py där).
# Där håller tangenterna andra tider, så stegningens prov räknar olika
# långt; N.emu.out går då före N.out. ./kor.sh -u skriver .out.
# Sedan körs varje prov med TKN80 (emulatorn i emulator/, -tkn80) i båda
# lägena och jämförs med samma prov utan kortet i samma emulator: i
# 40-teckensläget (Z=INP(3) och en rensad skärm först) ska skärmen vara
# densamma, i 80-teckensläget samma tecken bortsett från radbrytningen
# (alla blanktecken och radslut tas bort före jämförelsen).
# Prov vars namn slutar på 80 (ed80) körs bara med TKN80 i
# 80-teckensläget och jämförs med N.t80.out (tomma rader och blanka
# sist på raden borttagna); ./kor.sh -u skriver den.
# Proven körs mot den fasta delen och bankerna (../gor.sh). ./kor.sh -1
# kör dem mot steg1.bin (../gor.sh -1, utan banker): proven för de
# kommandon som inte finns där hoppas över, och steg1/ har de prov och
# utdata som är annorlunda.
cd "$(dirname "$0")" || exit 1
if [ "$1" = "-1" ]; then
  shift
  KRETS="-l ../steg1.bin@4000"
  UTAN=" var var2 change ren ren2 old ed4 ed80 list80 "
  S1=steg1
else
  b=$(ls ../forthaid2_b[0-9].bin 2>/dev/null | tr '\n' ',' | sed 's/,$//')
  [ -n "$b" ] || { echo "kor.sh: inga banker, kör ../gor.sh först"; exit 1; }
  KRETS="-l ../forthaid2.bin@4000 -b $b@5000"
fi
# prov n: argumenten till emulatorn (ur steg1/ om det finns där)
prov() { local t=$1.test; [ -n "$S1" ] && [ -f $S1/$t ] && t=$S1/$t; grep -v '^#' "$t"; }
H=../../../dis/04/host
if [ -d $H ] && [ -z "$EMU" ]; then
  (cd $H && ./build.sh) > /dev/null || exit 1
  EMU=$H/abc80host; HOST=1
else
  E=../../../emulator
  make -C $E abc80 > /dev/null || exit 1
  EMU=$E/abc80
fi
upd=0; [ "$1" = "-u" ] && upd=1
fail=0
for t in *.test; do
  n=${t%.test}
  case "$UTAN" in *" $n "*) continue;; esac
  case $n in *80) continue;; esac
  args=$(prov $n)
  for r in 9913 11273; do
    if [ -n "$HOST" ]; then
      out=$(eval "$EMU -r $r $KRETS $args")
      exp=$n.out; [ -f $n.$r.out ] && exp=$n.$r.out
    else
      rom=$E/roms/abc80new.rom; [ $r = 11273 ] && rom=$E/roms/abc80old.rom
      [ -f $rom ] || { echo "kor.sh: $rom saknas (hamta.py i roms/)"; exit 1; }
      out=$(eval "$EMU -r $rom $KRETS $args -t 300")
      exp=$n.out; [ -f $n.$r.out ] && exp=$n.$r.out
      [ -f $n.emu.out ] && exp=$n.emu.out
    fi
    [ -n "$S1" ] && [ -f $S1/$exp ] && exp=$S1/$exp
    [ $upd = 1 ] && [ $r = 9913 ] && [ ! -f $n.$r.out ] && echo "$out" > $exp
    if [ "$out" = "$(cat $exp 2>/dev/null)" ]; then echo "ok   $n $r"
    else echo "FEL  $n $r"; echo "$out" | diff $exp - | head -20; fail=1; fi
  done
done
E=../../../emulator
T=$E/roms/tkn80-iii.rom
if [ ! -f $T ]; then echo "kor.sh: $T saknas, inga prov med TKN80"; exit $fail; fi
make -C $E abc80 > /dev/null || exit 1
rensa() { sed 's/ *$//' | grep -v '^$'; }
for t in *.test; do
  n=${t%.test}
  case "$UTAN" in *" $n "*) continue;; esac
  args=$(prov $n)
  case $n in *80)
    for r in 9913 11273; do
      rom=$E/roms/abc80new.rom; [ $r = 11273 ] && rom=$E/roms/abc80old.rom
      t80=$(eval "$E/abc80 -r $rom -tkn80 $T $KRETS $args -t 300" | rensa)
      [ $upd = 1 ] && [ $r = 9913 ] && echo "$t80" > $n.t80.out
      if [ "$t80" = "$(cat $n.t80.out 2>/dev/null)" ]; then echo "ok   $n $r TKN80 80"
      else echo "FEL  $n $r TKN80 80"; echo "$t80" | diff $n.t80.out - | head -20; fail=1; fi
    done
    continue;;
  esac
  for r in 9913 11273; do
    rom=$E/roms/abc80new.rom; [ $r = 11273 ] && rom=$E/roms/abc80old.rom
    utan=$(eval "$E/abc80 -r $rom $KRETS $args -t 300" | rensa)
    t40=$(eval "$E/abc80 -r $rom -tkn80 $T -k 'Z=INP(3):PRINT CHR\$(12);\r\w100\.' $KRETS $args -t 300" | rensa)
    t80=$(eval "$E/abc80 -r $rom -tkn80 $T $KRETS $args -t 300" | rensa)
    if [ "$t40" = "$utan" ]; then echo "ok   $n $r TKN80 40"
    else echo "FEL  $n $r TKN80 40"; echo "$t40" | diff <(echo "$utan") - | head -20; fail=1; fi
    if [ "$(echo "$t80" | tr -d ' \n')" = "$(echo "$utan" | tr -d ' \n')" ]; then echo "ok   $n $r TKN80 80"
    else echo "FEL  $n $r TKN80 80"; echo "$t80" | diff <(echo "$utan") - | head -20; fail=1; fi
  done
done
exit $fail
