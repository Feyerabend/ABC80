#!/bin/sh
# lapptid.sh -- utanför boken: kör proven med en lappad ROM (ut/<namn>.rom,
# från lappa.py) och jämför tiden och skärmen med den olappade (kor.sh
# måste ha körts först). Namn som börjar på a_ är ABC80, b_ BASIC II.
#   ./lapptid.sh namn [prov ...]
cd "$(dirname "$0")"
LC_ALL=C; export LC_ALL
namn=$1; shift
case $namn in
    a_*) bas=gamla; ram=16; klar=00CC; KRETS=ut/$namn.krets@5000 ;;
    *)   bas=b2;    ram=32; klar=0110; KRETS= ;;
esac
export KRETS
[ $# -gt 0 ] || set -- $(cd prov && ls *.BAS | sed 's/\.BAS$//')
printf '%-8s %9s %9s %7s  %s\n' prov förut "$namn" vinst skärmen
for p in "$@"; do
    f=$(head -1 ut/$p.$bas.tid 2>/dev/null)
    [ -n "$f" ] || { echo "kör kor.sh först"; exit 1; }
    n=$(SKARM=1 ./prof ut/$namn.rom $ram $klar prov/$p.BAS ut/$p.$namn.txt 2>ut/$p.$namn.skarm | cut -d' ' -f3-)
    if cmp -s ut/$p.$bas.skarm ut/$p.$namn.skarm; then s=samma; else s=ANNAN; fi
    printf '%-8s %9s %9s %6s%%  %s\n' $p "$f" "$n" \
        $(echo "$f $n" | awk '{ printf "%.1f", 100 * ($1 - $2) / $1 }') $s
done
