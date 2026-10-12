#!/bin/sh
# prov.sh -- utanför boken: bygger kretsen och kör proven. prov/X.txt
# ska ge skärmen i prov/X.ut; tolkjamforelse-proven ska ge samma skärm
# som abc80old.rom. Tiderna (RUN) skrivs ut.
#   ./prov.sh            alla
#   ./prov.sh -n X       skriv om prov/X.ut (efter att ha läst skärmen)
cd "$(dirname "$0")"
LC_ALL=C; export LC_ALL
K=../../emulator/karna
R=../../emulator/roms
[ kor -nt kor.c ] || cc -std=c11 -O2 -I$K -o kor kor.c $K/*.c || exit 1
python3 bygg.py || exit 1
kor() { ./kor ut/comal.rom "$1" ut/comal.krets@4000; }
if [ "$1" = -n ]; then
    kor prov/$2.txt > prov/$2.ut; cat prov/$2.ut; exit
fi
fel=0
for f in prov/*.txt; do
    p=$(basename $f .txt)
    t=$(kor $f 2>&1 >ut/$p.skarm | tr '\n' ' ')
    if cmp -s ut/$p.skarm prov/$p.ut; then r=rätt; else r=FEL; fel=1; fi
    printf '%-10s %s  %s\n' $p $r "$t"
done
mkdir -p ut/gamla
for f in ../tolkjamforelse/prov/*.BAS; do
    p=$(basename $f .BAS)
    { tr -d '\r' < $f; echo RUN; } > ut/gamla/$p.txt
    a=$(./kor $R/abc80old.rom ut/gamla/$p.txt 2>&1 >ut/gamla/$p.gamla | cut -d' ' -f2)
    b=$(kor ut/gamla/$p.txt 2>&1 >ut/gamla/$p.comal | cut -d' ' -f2)
    if cmp -s ut/gamla/$p.gamla ut/gamla/$p.comal; then r=samma; else r=OLIKA; fel=1; fi
    printf '%-10s %s  %s T, med kretsen %s T\n' $p $r $a $b
done
exit $fel
