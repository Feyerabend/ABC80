#!/bin/sh
# matt.sh -- utanför boken: samma program med GOTO, FOR och GOSUB (.BAS,
# i alla tre tolkarna), med de nya strukturerna (.CML, bara i COMAL-ROM:en)
# och med BASIC II:s WHILE/WEND (.B2). Tiden i sekunder från RUN till
# READY, som i ../tolkjamforelse/kor.sh; skärmen i ut/matt/.
#   ./matt.sh
cd "$(dirname "$0")"
LC_ALL=C; export LC_ALL
T=../tolkjamforelse
R=../../emulator/roms
[ -x $T/prof ] || (cd $T && ./kor.sh B1 >/dev/null) || exit 1
[ -f ut/comal.krets ] || python3 bygg.py || exit 1
mkdir -p ut/matt
tid() { # rom ram klar fil namn [krets]
    KRETS=$6 SKARM=1 $T/prof $1 $2 $3 $4 2>ut/matt/$5.skarm | cut -d' ' -f3
}
printf '%-8s %8s %8s %8s  %s\n' prov gamla COMAL "BASIC II" svaret
for f in matt/*.BAS matt/*.CML matt/*.B2; do
    p=$(basename $f); n=${p%.*}; e=${p##*.}
    g=-; c=-; b=-
    case $e in
    BAS) g=$(tid $R/abc80old.rom 16 00CC $f $n.gamla)
         c=$(tid ut/comal.rom 16 00CC $f $n.comal ut/comal.krets@4000)
         b=$(tid $R/basicii80.rom 32 0110 $f $n.b2)
         s=ut/matt/$n.comal.skarm ;;
    CML) c=$(tid ut/comal.rom 16 00CC $f $n.comal ut/comal.krets@4000)
         s=ut/matt/$n.comal.skarm ;;
    B2)  b=$(tid $R/basicii80.rom 32 0110 $f $n.b2)
         s=ut/matt/$n.b2.skarm ;;
    esac
    printf '%-8s %8s %8s %8s  %s\n' $n $g $c $b \
        "$(grep -v '^ *$' $s | grep -v -E '^(RUN|ABC80|READY|[0-9]+ )' | tail -1)"
done
