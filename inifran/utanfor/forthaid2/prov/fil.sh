#!/bin/bash
# Provet för SAVEW och LOADW (steg 7): emulatorn i emulator/ med DOS:en
# (abcdos80.rom på $6000) och en kopia av exemplens program.dsk på
# kort 45, mot 9913 och 11273 och med TKN80 i 80-teckensläget (samma
# tecken bortsett från radbrytningen, som i kor.sh). Skärmen jämförs med
# fil.out; ./fil.sh -u skriver den. Bokens värdemulator har inget
# skivminne, så provet ligger utanför kor.sh.
cd "$(dirname "$0")" || exit 1
E=../../../emulator
make -C $E abc80 > /dev/null || exit 1
D=$E/roms/abcdos80.rom
[ -f $D ] || { echo "fil.sh: $D saknas (hamta.py i roms/)"; exit 1; }
b=$(ls ../forthaid2_b[0-9].bin 2>/dev/null | tr '\n' ',' | sed 's/,$//')
[ -n "$b" ] || { echo "fil.sh: inga banker, kör ../gor.sh först"; exit 1; }
T=$(mktemp -d) || exit 1
trap 'rm -rf $T' EXIT
# Ett BASIC-program som inte är de egna orden (fel signatur), orden
# sparas och laddas direkt i BASIC, ett BASIC-program med CALL, sedan
# LOADW efter Z=CALL (bara över STKTOP: samma fil går, en större inte)
# och felen utan namn och med en fil som inte finns (fel 21 till BASIC).
# Skärmen räcker inte till allt: först körs den första halvan (K1) för
# sig, sedan alltihop.
K='\w3000\.10 PRINT 1\r\w300\.SAVE PRG\r\w1500\.NEW\r\w300\.Z=CALL(16384,2)\r\w600\.'
K=$K'FORTH\r\w600\.: SQ DUP * ; : Q4 SQ SQ ;\r\w300\.: P 0< IF ." N" ELSE ." P" THEN ;\r\w300\.'
K=$K'VARIABLE V 7 V !\r\w300\.SAVEW LIB\r\w1500\.SAVEW\r\w300\.LOADW\r\w300\.LOADW PRG.BAC\r\w1500\.BYE\r\w600\.'
K=$K'LOADW LIB\r\w1500\.SYS\r\w800\.'
K1=$K
K=$K'10 Z=CALL(16384,1)\r20 PRINT Z\r\w300\.RUN\r\w900\.3 Q4 . -1 P V @ .\r\w600\.bye\r\w600\.'
K=$K'Z=CALL(16384,1)\r\w900\.LOADW LIB\r\w1500\.: X Q4 1+ ; 2 X . 1 P\r\w600\.SAVEW LIB2\r\w1500\.'
K=$K'LOADW LIB2\r\w1500\.LOADW LIB\r\w1500\.3 Q4 . X\r\w600\.LOADW NIX\r\w1500\.SYS\r\w800\.'
kor() {
  for k in "$K1" "$K"; do
  cp $E/exempel/program/diskett/program.dsk $T/d.dsk
  $E/abc80 -r $1 -l $D@6000 -D $T/d.dsk@45 -DS -l ../forthaid2.bin@4000 -b $b@5000 \
    $2 -k "$k" -t 22000
  done
}
rensa() { sed 's/ *$//' | grep -v '^$'; }
upd=0; [ "$1" = "-u" ] && upd=1
fail=0
for r in 9913 11273; do
  rom=$E/roms/abc80new.rom; [ $r = 11273 ] && rom=$E/roms/abc80old.rom
  out=$(kor $rom | rensa)
  [ $upd = 1 ] && [ $r = 9913 ] && echo "$out" > fil.out
  if [ "$out" = "$(cat fil.out)" ]; then echo "ok   fil $r"
  else echo "FEL  fil $r"; echo "$out" | diff fil.out - | head -20; fail=1; fi
  t80=$(kor $rom "-tkn80 $E/roms/tkn80-iii.rom" | rensa)
  if [ "$(echo "$t80" | tr -d ' \n')" = "$(echo "$out" | tr -d ' \n')" ]; then echo "ok   fil $r TKN80 80"
  else echo "FEL  fil $r TKN80 80"; echo "$t80" | diff <(echo "$out") - | head -20; fail=1; fi
done
exit $fail
