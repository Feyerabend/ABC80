#!/bin/sh
# Kör proven: varje prov/N.test är argument till en emulator och körs
# mot 9913 och 11273; skärmen jämförs med N.out (eller N.<rom>.out).
# Finns bokens värdemulator (dis/04/host) används den, annars emulatorn
# i emulator/ med ROM:arna i emulator/roms (hämtas med hamta.py där).
# Där håller tangenterna andra tider, så stegningens prov räknar olika
# långt; N.emu.out går då före N.out. ./kor.sh -u skriver .out.
cd "$(dirname "$0")" || exit 1
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
  args=$(grep -v '^#' "$t")
  for r in 9913 11273; do
    if [ -n "$HOST" ]; then
      out=$(eval "$EMU -r $r -l ../forthaid.bin@4000 $args")
      exp=$n.out; [ -f $n.$r.out ] && exp=$n.$r.out
    else
      rom=$E/roms/abc80new.rom; [ $r = 11273 ] && rom=$E/roms/abc80old.rom
      [ -f $rom ] || { echo "kor.sh: $rom saknas (hamta.py i roms/)"; exit 1; }
      out=$(eval "$EMU -r $rom -l ../forthaid.bin@4000 $args -t 300")
      exp=$n.out; [ -f $n.$r.out ] && exp=$n.$r.out
      [ -f $n.emu.out ] && exp=$n.emu.out
    fi
    [ $upd = 1 ] && [ $r = 9913 ] && [ ! -f $n.$r.out ] && echo "$out" > $exp
    if [ "$out" = "$(cat $exp 2>/dev/null)" ]; then echo "ok   $n $r"
    else echo "FEL  $n $r"; echo "$out" | diff $exp - | head -20; fail=1; fi
  done
done
exit $fail
