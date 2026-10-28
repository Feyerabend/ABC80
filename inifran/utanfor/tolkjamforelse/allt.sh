#!/bin/sh
# allt.sh -- utanför boken: gör om hela jämförelsen i LASMIG.md, i ordning.
#
#  1. kor.sh       proven i prov/ i båda tolkarna, olappade: tiderna och
#                  skärmarna som allt annat jämförs med
#  2. lappa.py     varje lapp för sig och uppsättningarna
#  3. lapptid.sh   tiden och skärmen för varje lapp och uppsättning
#  4. kantprov.sh  kantfallen i kant/prov.txt, olappat mot a_alla, b_alla
#  5. felprov.py   medvetna fel i a_sup och b_sup mot kantfallen
#  6. fprov        FMUL och FDIV, a_flt och a_alla mot den olappade ROM:en
#  7. mulprov      b_mul mot MUL16
#  8. bokprov.sh   bokens tolkprov med a_alla och b_alla
#
# Allt hamnar i ut/ (ut/allt.txt är det som skrivs här). Det tar en
# god stund, mest kantfallen (vänta 1 s efter varje RETURN) och felen.
#   ./allt.sh
cd "$(dirname "$0")"
K=../../emulator/karna
KARNA="$K/z80.c $K/maskin.c $K/bussen.c $K/ljud.c $K/kassett.c $K/tecken.c $K/uni80.c"
for p in fprov mulprov; do
    [ $p -nt $p.c ] || cc -std=c11 -O2 -I$K -o $p $p.c $KARNA || exit 1
done
mkdir -p ut
{
echo "== 1. proven, olappade"; ./kor.sh

echo "== 2. lapparna"
for l in a_brk a_tab a_var a_sup a_madd a_fswp a_ftab a_fdiv; do
    python3 lappa.py a $l $l || exit 1
done
python3 lappa.py a a_hel  a_brk a_tab a_sup
python3 lappa.py a a_flt  a_madd a_fswp a_ftab a_fdiv
python3 lappa.py a a_flt2 a_madd a_ftab a_fdiv
python3 lappa.py a a_alla a_brk a_tab a_sup a_madd a_fswp a_ftab a_fdiv
for l in b_mul b_nxt b_sup; do python3 lappa.py b $l $l || exit 1; done
python3 lappa.py b b_alla b_mul b_nxt b_sup

echo "== 3. tiderna"
for n in a_brk a_tab a_var a_sup a_hel a_madd a_fswp a_ftab a_fdiv \
         a_flt a_flt2 a_alla b_mul b_nxt b_sup b_alla; do
    echo "-- $n"; ./lapptid.sh $n
done

echo "== 4. kantfallen"; ./kantprov.sh a_alla b_alla
echo "== 5. medvetna fel"; python3 felprov.py
echo "== 6. FMUL och FDIV"
./fprov ut/a_flt.rom ut/a_flt.krets 300000
./fprov ut/a_alla.rom ut/a_alla.krets 300000 2
echo "== 7. b_mul"; ./mulprov ut/b_mul.rom
echo "== 8. bokens prov"; ./bokprov.sh
} 2>&1 | tee ut/allt.txt
