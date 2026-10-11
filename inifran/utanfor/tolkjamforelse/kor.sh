#!/bin/sh
# kor.sh -- utanför boken: kör proven i prov/ i ABC80-BASIC:en (gamla och
# nya ROM:en) och i BASIC II, skriver tiderna (s) och sparar skärmen och
# profilen för varje körning i ut/.
#   ./kor.sh [prov ...]
cd "$(dirname "$0")"
LC_ALL=C; export LC_ALL
K=../../emulator/karna
R=../../emulator/roms
[ prof -nt prof.c ] || cc -std=c11 -O2 -I$K -o prof prof.c \
    $K/z80.c $K/maskin.c $K/bussen.c $K/ljud.c $K/kassett.c $K/tecken.c || exit 1
mkdir -p ut
[ $# -gt 0 ] || set -- $(cd prov && ls *.BAS | sed 's/\.BAS$//')
printf '%-8s %9s %9s %9s %7s\n' prov gamla nya "BASIC II" kvot
for p in "$@"; do
    g=$(SKARM=1 ./prof $R/abc80old.rom 16 00CC prov/$p.BAS ut/$p.gamla.txt 2>ut/$p.gamla.skarm | cut -d' ' -f3-)
    n=$(SKARM=1 ./prof $R/abc80new.rom 16 00CC prov/$p.BAS ut/$p.nya.txt 2>ut/$p.nya.skarm | cut -d' ' -f3-)
    b=$(SKARM=1 ./prof $R/basicii80.rom 32 0110 prov/$p.BAS ut/$p.b2.txt 2>ut/$p.b2.skarm | cut -d' ' -f3-)
    echo "$g" > ut/$p.gamla.tid; echo "$n" > ut/$p.nya.tid; echo "$b" > ut/$p.b2.tid
    printf '%-8s %9s %9s %9s %7s\n' $p "$g" "$n" "$b" \
        $(echo "$g $b" | awk '{ printf "%.2f", $1 / $2 }')
done
