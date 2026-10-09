#!/bin/sh
# Mäter samma prov i ABC80:s BASIC (TID.BAS), i bokens Forth-krets
# (emulator/exempel/krets/forth.bin, e7ip) och i forth_snabb.bin (gor.sh).
# Utanför boken, se LASMIG.md.
#
# Tiden mäts i maskinen med klockan på 65008 (ROM:en räknar den ned 50
# gånger per sekund), så den gäller den emulerade ABC80 och inte hur fort
# emulatorn går. Står den låga byten på 0 har de övre redan räknats ned
# (band 1, maskinvaran), så då läggs 256 till; annars kan en mätning bli
# 256 tick fel. BASIC skriver ms; Forth skriver tick (20 ms).
# Kolonorden hålls under 79 tecken, tolkens radbuffert.
#
#     ./matning.sh
# Emulatorn finns i ../../emulator (bokens arkiv) eller ../emulator (det
# publika förrådet).
cd "$(dirname "$0")" || exit 1
EMU=../../emulator
[ -d $EMU/karna ] || EMU=../emulator
ABC80=$EMU/abc80
ROM=$EMU/roms/abc80new.rom
[ -x $ABC80 ] || make -C $EMU abc80 > /dev/null || exit 1

echo "BASIC (ms): tom slinga I%, tom slinga I, A%=I%+I%-5%, GOSUB, sållet, primtal"
$ABC80 -r $ROM -f TID.BAS -k 'RUN\r' -t 60000 | sed -n '/^RUN/,/^ABC80/p' | sed '1d;$d' | tr '\n' ' '
echo

forth() {
  K='POKE 65052,0,208\rNEW\rZ=CALL(16384,3)\r\w500\.'
  for r in ': T 65008 @ 65008 C@ 0= IF 256 + THEN ;' ': TID T - . ;' \
    'VARIABLE A  VARIABLE F  VARIABLE C' 'HERE F ! 1001 ALLOT' \
    ': B1 T 10000 0 DO LOOP TID ;' \
    ': B2 T 10000 0 DO I I + 5 - A ! LOOP TID ;' \
    ': NOLL ;' ': B3 T 10000 0 DO NOLL LOOP TID ;' \
    ': RENSA 1001 0 DO 0 F @ I + C! LOOP ;' ': SETT F @ + 1 SWAP C! ;' \
    ': STRYK DUP DUP + BEGIN DUP 1001 < WHILE DUP SETT OVER + REPEAT DROP DROP ;' \
    ': P? F @ + C@ 0= ;' ': SK 1001 2 DO I P? IF C @ 1+ C ! I STRYK THEN LOOP ;' \
    ': B4 RENSA T 0 C ! SK TID C @ . ;' \
    ': B5 T 10000 BEGIN 1 - DUP 0= UNTIL DROP TID ;'; do
    K="$K$r"'\r\w300\.'
  done
  K="$K"'B1 B2 B3 B4 B5\r\w30000\.'
  $ABC80 -r $ROM -l $1@4000 -k "$K" -t 1000 | grep '^B1 B2' | sed 's/^B1 B2 B3 B4 B5//;s/ OK$//'
}
echo "Forth (tick): tom DO LOOP, I I + 5 - A !, anrop, sållet, primtal, BEGIN UNTIL"
printf 'boken  '; forth $EMU/exempel/krets/forth.bin
printf 'snabb  '; forth forth_snabb.bin
