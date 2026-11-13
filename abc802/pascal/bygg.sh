#!/bin/sh
# Bygg Pascal för ABC 802: tolken pvm.asm till pvm.bin ($8000-) och
# skivan pascal.img med PASCAL.ABS, BASICINI.SYS (basicini.asm, som
# startar tolken när maskinen startar), kompilatorn PC.PK och PC.PAS och
# några av proven som exempel, och FPP.BIN för REAL.
#
#   sh bygg.sh        bygger
#   sh bygg.sh fpp    bygger bara pvm.bin och fpp.bin (för prov.sh)
#   sh bygg.sh prov   bygger och prövar i emulatorn
#   sh bygg.sh cpm    bygger PVM.COM för CP/M och dataskivan cpm.img
#   sh bygg.sh cpmprov  bygger dem och prövar i CP/M i emulatorn
#
# fpp.bin (FPP.BIN på skivan) är preal.asm, tolkens symboler ur pvm.lst
# och fpp/fpp.asm, på FPPADR ($DD00); tolken laddar den när programmet
# använder REAL, och pvm.py kör FPP i den.
#
# Proven i emulatorn: varje prov/*.pa och prov/*.pas utan .kfel körs i
# pvm.asm från en kopia av pascal.img, med prov/*.in som tangenter, och
# skärmen jämförs med prov/*.ut (med prov/*.fel som körfelet). Sist
# kompilerar PC.PK på skivan PC.PAS, och PC.PK ska bli lika byte för
# byte med den från pc.py (omkring 750 sekunder i ABC 802, några
# sekunder i emulatorn).
set -e
cd "$(dirname "$0")"
# Assemblern: ../verktyg/z80asm.c, byggs första gången.
Z80ASM=${Z80ASM:-../verktyg/z80asm}
[ -x "$Z80ASM" ] || cc -O2 -o "$Z80ASM" ../verktyg/z80asm.c
H=$(pwd)
ASM=$Z80ASM
EMU=../emulator/abc802
TOM=../emulator/skivor/tom.img

$ASM -l -o pvm.full pvm.asm > pvm.lst
tail -c +32769 pvm.full > pvm.bin
rm pvm.full
# Tolken ska sluta under p-koden på $9000 (TOLKSLUT i pvm.asm).
slut=$(printf '%X' $((0x8000 + $(wc -c < pvm.bin))))
if [ $((0x$slut)) -gt $((0x9000)) ]; then
    echo "bygg.sh: tolken slutar på $slut, över \$9000" >&2; exit 1
fi
# FPP.BIN: limmet med tolkens symboler, och FPP. Den ska sluta under TOPP.
{ cat preal.asm
  awk '{ for (i = 2; i <= NF && i <= 10; i++) if ($i ~ /^[A-Z_0-9]+:$/) {
           n = substr($i, 1, length($i) - 1)
           if (n ~ /^(TMPIP|MPS|PSP|SAVESP|NEXT|KORFEL|POPA|LASFIL|TITTA|TA|F5|C_WRS)$/)
             printf "%-8sEQU  $%s\n", n, $1 } }' pvm.lst
  cat fpp/fpp.asm; } > fpp.asm.tmp
$ASM -l -o fpp.full fpp.asm.tmp > fpp.lst
rm fpp.asm.tmp
tail -c +$((0xDD00 + 1)) fpp.full > fpp.bin
rm fpp.full
fslut=$(printf '%X' $((0xDD00 + $(wc -c < fpp.bin))))
if [ $((0x$fslut)) -gt $((0xED00)) ]; then
    echo "bygg.sh: FPP.BIN slutar på $fslut, över \$ED00" >&2; exit 1
fi
[ "$1" = fpp ] && exit 0

# CP/M (PASCAL.md, avsnitt 8): pvm.asm med CPM EQU 1, limmet och FPP i
# samma fil, och p-koden efter dem på KOD. cpm.img är en dataskiva (B:)
# till CP/M-skivan ../emulator/cpm/a.img, med PVM.COM, PC.PK, PC.PAS,
# PCS.PK, PCS.PAS och exemplen; textfilerna har CR LF. Proven körs som i prov nedan, med
# PVM TNAMN på B:, och sist kompilerar PC.PK på B: PC.PAS.
crlf() { awk '{ printf "%s\r\n", $0 }' "$1" > "$2"; }
case "$1" in cpm|cpmprov)
  C=prov/ut/cpm
  mkdir -p $C/s
  { sed 's/^CPM     EQU  0 /CPM     EQU  1 /' pvm.asm
    grep -v -E '^(FPPADR +EQU|[[:space:]]+ORG +FPPADR)' preal.asm
    cat fpp/fpp.asm; echo; echo 'KOD:'; } > pvmcpm.asm.tmp
  $ASM -l -o pvm.full pvmcpm.asm.tmp > pvmcpm.lst
  rm pvmcpm.asm.tmp
  tail -c +257 pvm.full > pvm.com
  rm pvm.full
  python3 pc.py pc.pas pc.pk
  python3 pc.py pcs.pas pcs.pk
  rm -f $C/s/*
  cp pvm.com pc.pk pcs.pk $C/s/
  crlf pc.pas $C/s/pc.pas
  crlf pcs.pas $C/s/pcs.pas
  for n in halsa primtal rekursion satser sortera lasa reella strangar; do
    crlf prov/$n.pas $C/s/$(echo $n | cut -c1-8).pas
  done
  (cd $C/s && python3 -I $H/../emulator/cpm/skiva.py - - - $H/cpm.img *) > /dev/null
  echo "bygg.sh: pvm.com \$0100-\$$(printf '%04X' $((0x100 + $(wc -c < pvm.com)))), cpm.img"
  [ "$1" = cpmprov ] || exit 0
  A=../emulator/cpm/a.img
  [ -f $A ] || { echo "bygg.sh: ingen $A (make i emulator/)" >&2; exit 1; }
  set +e
  fel=0
  # prov/stor/*.pas är den stora versionen, som bara går i PVM.COM.
  for k in prov/*.pa prov/*.pas prov/stor/*.pas; do
    d=$(dirname $k)
    case $k in
      *.pas) n=$(basename $k .pas); verktyg=pc.py ;;
      *) n=$(basename $k .pa); verktyg=pasm.py ;;
    esac
    [ -f $d/$n.kfel ] && continue
    N=$(echo $n | cut -c1-7 | tr a-z A-Z)
    rm -f $C/s/*
    python3 $verktyg $k $C/s/t$(echo $n | cut -c1-7).pk || { echo "FEL   $n: $verktyg"; fel=1; continue; }
    cp pvm.com $C/s/
    (cd $C/s && python3 -I $H/../emulator/cpm/skiva.py - - - ../t.img *) > /dev/null
    cp $A $C/a.img
    set -- -k "B:\\r\\w500PVM T$N\\r\\w3000"
    if [ -f $d/$n.in ]; then
      while IFS= read -r r; do set -- "$@" -k "$r\\r\\w800"; done < $d/$n.in
    fi
    ../emulator/abc802 -t 60000 -D $C/a.img -D $C/t.img@44.1 "$@" > $C/$n.skarm
    if python3 - $C/$n.skarm $d/$n.ut "B>PVM T$N" $d/$n.in $d/$n.fel <<'SLUT'
import os, sys
skarm, ut, namn, inn, fel = sys.argv[1:]
abc = str.maketrans('ÄÖÅäöå', '[\\]{|}')
r = open(skarm, encoding='utf-8').read().translate(abc).split('\n')
i = max(j for j, x in enumerate(r) if x.rstrip() == namn)
# En rad om 80 tecken bryts av BIOS:en, och CR LF efter den ger en tom
# rad till.
s = [x for j, x in enumerate(r[i + 1:], i + 1) if x or len(r[j - 1]) != 80]
while s and s[-1].strip() in ('', 'B>'):
    s.pop()
for t in (open(inn).read().split('\n')[:-1] if os.path.exists(inn) else []):
    s.remove(t)
if os.path.exists(fel):
    f = open(fel, encoding='utf-8').read().strip().translate(abc)
    k = [j for j, x in enumerate(s) if x.startswith(f)]
    if not k:
        sys.exit(1)
    s = s[:k[0]]
    while s and s[-1] == '':
        s.pop()
v = [x.rstrip() for x in open(ut, encoding='utf-8').read().split('\n')[:-1]]
sys.exit(0 if [x.rstrip() for x in s] == v else 1)
SLUT
    then echo "rätt  $n i PVM.COM"
    else echo "FEL   $n i PVM.COM: se $C/$n.skarm"; fel=1; fi
  done
  # Självkompileringen: PC.PK på B: kompilerar PC.PAS till PC.PK.
  cp cpm.img $C/sk.img
  cp $A $C/a.img
  ../emulator/abc802 -t 3000 -D $C/a.img -D $C/sk.img@44.1 -DS -k 'B:\r\w500PVM PC\r\w3000PC.PAS\r' -t 900000 > $C/pc.skarm
  rm -f $C/sk.pk
  python3 -I cpmfil.py $C/sk.img PC.PK $C/sk.pk 2>/dev/null
  n=$(wc -c < pc.pk | tr -d ' ')
  if [ -f $C/sk.pk ] && head -c $n $C/sk.pk | cmp -s - pc.pk; then
    echo "rätt  pc.pas kompilerar sig själv i PVM.COM ($n bytes)"
  else echo "FEL   pc.pas i PVM.COM: se $C/pc.skarm"; fel=1; fi
  # pcs.pas, kompilatorn för den stora versionen: PCS.PK kompilerar
  # PCS.PAS och prov/stor/*.pas till samma bytes som pc.py.
  python3 pc.py pcs.pas $C/pcs.pk
  for k in pcs.pas prov/stor/*.pas; do
    n=$(basename $k .pas)
    N=$(echo $n | cut -c1-8 | tr a-z A-Z)
    rm -f $C/s/* $C/ref.pk $C/sk.pk
    cp pvm.com $C/s/
    cp $C/pcs.pk $C/s/pcs.pk
    crlf $k $C/s/$N.PAS
    (cd $C/s && python3 -I $H/../emulator/cpm/skiva.py - - - ../sk.img *) > /dev/null
    cp $A $C/a.img
    ../emulator/abc802 -t 3000 -D $C/a.img -D $C/sk.img@44.1 -DS -k "B:\\r\\w500PVM PCS\\r\\w3000$N.PAS\\r" -t 1500000 > $C/pcs-$n.skarm
    if [ -f prov/stor/$n.kfel ]; then
      if grep -qF "$(cat prov/stor/$n.kfel)" $C/pcs-$n.skarm; then
        echo "rätt  stor/$n med PCS.PK i PVM.COM (kompileringsfelet)"
      else echo "FEL   stor/$n med PCS.PK i PVM.COM: se $C/pcs-$n.skarm"; fel=1; fi
      continue
    fi
    python3 pc.py $k $C/ref.pk
    python3 -I cpmfil.py $C/sk.img $N.PK $C/sk.pk 2>/dev/null
    m=$(wc -c < $C/ref.pk | tr -d ' ')
    if [ -f $C/sk.pk ] && head -c $m $C/sk.pk | cmp -s - $C/ref.pk; then
      echo "rätt  $n med PCS.PK i PVM.COM ($m bytes)"
    else echo "FEL   $n med PCS.PK i PVM.COM: se $C/pcs-$n.skarm"; fel=1; fi
  done
  exit $fel
esac
start=$(awk '/LADDAD:/ {print $1; exit}' pvm.lst)
python3 ../forth/abs.py pvm.bin 8000 $start PASCAL $TOM pascal.img
$ASM -l -o basicini.full basicini.asm > basicini.lst
tail -c +62724 basicini.full > basicini.bin
rm basicini.full
python3 ../forth/abs.py basicini.bin 0 0 BASICINI.SYS pascal.img pascal.img
python3 pc.py pc.pas pc.pk
python3 skiva.py put pascal.img pc.pk PC.PK
python3 skiva.py put pascal.img pc.pas PC.PAS -t
python3 skiva.py put pascal.img fpp.bin FPP.BIN
for n in halsa primtal rekursion satser sortera lasa reella strangar; do
    N=$(echo $n | cut -c1-8 | tr a-z A-Z)
    python3 skiva.py put pascal.img prov/$n.pas $N.PAS -t
done
echo "bygg.sh: pvm.bin \$8000-\$$slut, fpp.bin \$DD00-\$$fslut, pascal.img"

[ "$1" = prov ] || exit 0

E=prov/ut/emu
rm -rf $E && mkdir -p $E
set +e
fel=0
for k in prov/*.pa prov/*.pas; do
  case $k in
    *.pas) n=$(basename $k .pas); verktyg=pc.py ;;
    *) n=$(basename $k .pa); verktyg=pasm.py ;;
  esac
  [ -f prov/$n.kfel ] && continue
  N=$(echo $n | cut -c1-7 | tr a-z A-Z)
  python3 $verktyg $k $E/$n.pk || { echo "FEL   $n: $verktyg"; fel=1; continue; }
  cp pascal.img $E/t.img
  python3 skiva.py put $E/t.img $E/$n.pk T$N.PK > /dev/null
  # Tangenterna: programmets namn och sedan raderna i prov/*.in, när
  # programmet och FPP.BIN har hunnit laddas.
  set -- -k "\\w3000T$N\\r\\w4000"
  if [ -f prov/$n.in ]; then
    while IFS= read -r r; do set -- "$@" -k "$r\\r\\w800"; done < prov/$n.in
  fi
  $EMU -DS -D $E/t.img -K 80,80 "$@" -t 60000 > $E/$n.skarm
  if python3 - $E/$n.skarm prov/$n.ut T$N prov/$n.in prov/$n.fel <<'SLUT'
import os, sys
skarm, ut, namn, inn, fel = sys.argv[1:]
# Skärmen har ABC 802:s Ä Ö Å ä ö å där proven har [ \ ] { | }, och
# körfelen jämförs likadant.
abc = str.maketrans('ÄÖÅäöå', '[\\]{|}')
r = open(skarm, encoding='utf-8').read().translate(abc).split('\n')
i = max(j for j, x in enumerate(r) if x.rstrip() == 'Program: ' + namn)
s = r[i + 1:]
while s and s[-1].strip() in ('', 'ABC802'):
    s.pop()
# Det som skrevs på tangentbordet står också på skärmen.
for t in (open(inn).read().split('\n')[:-1] if os.path.exists(inn) else []):
    s.remove(t)
if os.path.exists(fel):
    f = open(fel, encoding='utf-8').read().strip().translate(abc)
    k = [j for j, x in enumerate(s) if x.startswith(f)]
    if not k:
        sys.exit(1)
    s = s[:k[0]]
    while s and s[-1] == '':
        s.pop()
v = [x.rstrip() for x in open(ut, encoding='utf-8').read().split('\n')[:-1]]
sys.exit(0 if [x.rstrip() for x in s] == v else 1)
SLUT
  then echo "rätt  $n i pvm.asm"
  else echo "FEL   $n i pvm.asm: se $E/$n.skarm"; fel=1; fi
done

# Självkompileringen: PC.PK på skivan kompilerar pc.pas, här som SK.PAS
# till SK.PK så att en gammal PC.PK inte kan räknas som rätt.
cp pascal.img $E/sk.img
python3 skiva.py put $E/sk.img pc.pas SK.PAS -t > /dev/null
$EMU -DS -D $E/sk.img -K 80,80 -k '\w3000PC\r\w2000SK.PAS\r' -t 900000 > $E/pc.skarm
python3 skiva.py get $E/sk.img SK.PK $E/pc.pk 2>/dev/null
if cmp -s $E/pc.pk pc.pk; then
  echo "rätt  pc.pas kompilerar sig själv i pvm.asm ($(wc -c <pc.pk | tr -d ' ') bytes)"
else echo "FEL   pc.pas i pvm.asm: se $E/pc.skarm"; fel=1; fi
exit $fel
