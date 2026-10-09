#!/bin/sh
# Kör den nya emulatorn och den gamla (verktyg/abc80 i det publika
# förrådet) med samma flaggor i batchläget och jämför utdata. Varje rad
# i fallen nedan är flaggorna till ett prov. INIFRAN pekar på förrådet:
# mappen ovanför, när emulatorn ligger i det (inifran/emulator/), annars
# ~/Documents/GitHub/ABC80/inifran.
# VANTADE är fall som får skilja: 4 (labplattan, skärmen rullar hela
# tiden), eftersom den gamla räknar en ms som 2 995 T-cykler och den nya
# som 2 995,2.
VANTADE=" 4 "
cd "$(dirname "$0")"
if [ -z "$INIFRAN" ]; then
  if [ -f ../verktyg/abc80.c ]; then INIFRAN=..
  else INIFRAN=$HOME/Documents/GitHub/ABC80/inifran; fi
fi
GAMLA=$INIFRAN/verktyg/abc80
ROM=$INIFRAN/disass/abc80/abc80new.rom
GAMMAL_ROM=$INIFRAN/disass/abc80/abc80old.rom
BII=$INIFRAN/disass/basicii/basicii80.rom
EX=$INIFRAN/exempel
[ -x "$GAMLA" ] || make -C "$INIFRAN/verktyg" abc80 >/dev/null || exit 1
mkdir -p prov
fel=0; nr=0
while IFS= read -r flaggor; do
  case "$flaggor" in ''|'#'*) continue ;; esac
  nr=$((nr + 1))
  eval "./abc80 $flaggor" > prov/$nr.ny 2>&1
  eval "$GAMLA $flaggor" > prov/$nr.gammal 2>&1
  if cmp -s prov/$nr.ny prov/$nr.gammal; then printf "lika   %s\n" "$nr"
  elif case "$VANTADE" in *" $nr "*) true ;; *) false ;; esac; then printf "väntat %s (olika)\n" "$nr"
  else printf "OLIKA  %s: %s\n" "$nr" "$flaggor"; diff prov/$nr.gammal prov/$nr.ny | head -10; fel=1; fi
done <<FALL
-r $ROM -t 200 -k 'PRINT 6*7\r'
-r $GAMMAL_ROM -t 200 -k 'PRINT 6*7\r'
-r $ROM -W 7 -t 300 -k 'PRINT INP(58)\rOUT 58,24\rPRINT INP(58)\r'
-r $ROM -W '7,3@1500,5@2500' -t 3000 -k '10 PRINT INP(58);\r20 GOTO 10\rRUN\r'
-r $ROM -t 500 -k 'FOR I=1 TO 2000:NEXT I:PRINT "KLAR"\r'
-r $ROM -t 300 -k 'PRINT CHR\$(151);"7#7#77 ";CHR\$(135);"ÅÄÖåäö"\r'
-r $ROM -t 500 -k '10 GET A\$:PRINT ASC(A\$)\r20 GOTO 10\rRUN\r' -s 'AB\x01'
-r $ROM -S 300 -t 500 -k '10 GET A\$:PRINT A\$;\r20 GOTO 10\rRUN\r' -s 'HEJ'
-r $ROM -K 20,30 -t 200 -k 'PRINT 1\rPRINT 2\r'
-r $ROM -t 200 -k '\w500\.PRINT 3\x03\r' -d C000,32 -d 7C00
-r $ROM -m 32 -t 200 -k 'PRINT FRE(0)\r'
-r $ROM -l $EX/e2.bin@4000 -k 'Z=CALL(16384)\rprint 6*7\r' -t 300
-r $ROM -b $EX/e4a.bin,$EX/e4b.bin@4000 -k 'Z=CALL(16384)\r' -t 500
-r $ROM -f $EX/README.md -t 100
-r $BII -m 32 -t 300 -k 'PRINT 6*7\r'
FALL
exit $fel
