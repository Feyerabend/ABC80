#!/bin/sh
# bokprov.sh -- utanför boken: kör bokens tolkprov (dis/04/host/tests)
# med lappade ROM:ar. -r 11273 blir ut/<a>.rom med kretsen ut/<a>.krets
# på $5000 (utom i prov som själva lägger något där), -r basicii80 blir
# ut/<b>.rom; 9913 körs olappad. Förväntade skärmar som förut.
#   ./bokprov.sh [a_namn [b_namn]]     (standard a_alla och b_alla)
cd "$(dirname "$0")"
HAR=$(pwd)
A=${1:-a_alla}; B=${2:-b_alla}
cat > ut/omslag.sh <<EOF
#!/bin/sh
V=$HAR/../../dis/04/host/abc80host
r=\$2; shift 2
case "\$*" in *@5000*) K= ;; *) K="-l $HAR/ut/$A.krets@5000" ;; esac
case \$r in
    11273)     exec "\$V" -r $HAR/ut/$A.rom \$K "\$@" ;;
    basicii80) exec "\$V" -r $HAR/ut/$B.rom "\$@" ;;
    *)         exec "\$V" -r \$r "\$@" ;;
esac
EOF
chmod +x ut/omslag.sh
(cd ../../dis/04/host && ./build.sh >/dev/null && PROGRAM="$HAR/ut/omslag.sh" ./test.sh)
