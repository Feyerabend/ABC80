#!/bin/sh
# kantprov.sh -- utanför boken: kantfallen i kant/prov.txt (FOR/NEXT,
# fält, DEF FN, strängar, flyttal, heltal) körs i den olappade och den
# lappade ROM:en; skärmarna jämförs (registerraden och antalet
# instruktioner räknas inte). Fröet till RND och BREAK beror på tiden och
# finns inte med. ./kantprov.sh [a_namn [b_namn]] (standard a_alla b_alla)
# Efter varje RETURN väntar provet 1000 ms: abc80host matar en tangent var
# 100:e ms, och BASIC II kompilerar raden när RETURN kommer och tappar
# tangenter som kommer under tiden (då blev raderna stympade och RUN
# borta, och båda ROM:arna gav samma felutskrift).
cd "$(dirname "$0")"
A=${1:-a_alla}; B=${2:-b_alla}
V=../../dis/04/host/abc80host
fel=0
grep -v '^#' kant/prov.txt | while IFS='|' read -r namn prog; do
    prog=$(printf '%s' "$prog" | sed 's/\\r/\\r\\w1000\\./g')
    for r in a b; do
        if [ $r = a ]; then
            g=$($V -r 11273 -t 2000 -k "$prog")
            n=$($V -r ut/$A.rom -l ut/$A.krets@5000 -t 2000 -k "$prog")
        else
            g=$($V -r basicii80 -m 32 -t 2000 -k "$prog")
            n=$($V -r ut/$B.rom -m 32 -t 2000 -k "$prog")
        fi
        g=$(echo "$g" | grep -v 'instr\.)\|STANNADE')
        n=$(echo "$n" | grep -v 'instr\.)\|STANNADE')
        if [ "$g" = "$n" ]; then echo "lika  $namn $r"
        else echo "OLIKA $namn $r"; echo "$g" > ut/k_g.txt; echo "$n" | diff ut/k_g.txt - | head -10; fi
        echo "$g" | grep -v '^ *$' > ut/kant_$namn.$r.txt
    done
done
