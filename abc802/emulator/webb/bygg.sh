#!/bin/sh
# bygg.sh -- gör abc802.html, emulatorn i en enda fil, till ABC 802
# inifrån
#
# Mallen (mall.html) får WebAssembly-koden (abc802.wasm, make webb),
# ROM:en, nät-ROM:en (LUXNET) och teckengeneratorn som base64, skivorna (../prov/skivor,
# gor.py, CP/M-skivan cpm/a.img och Pascal-skivan cpm/pascal.img, Forth, ../forth/forth.img, och programskivan, ../forth/program.img) packade med gzip och som base64, och datavision.js, bbs.js,
# stordator.js och abc802.js efter, där raden
# med FILER står; L.BAC (webb/lbac.sh) kommer med som base64 om den finns. Det som bara syns i mallen (mellan MALL och /MALL) tas
# bort. Filen skrivs på standard ut:
#     webb/bygg.sh > webb/abc802.html
#     SIDA=forth webb/bygg.sh > webb/abc802-forth.html
#     SIDA=cpm webb/bygg.sh > webb/abc802-cpm.html
# SIDA väljer skivorna: grund (tidsproven och programskivan, standard), forth eller cpm;
# den tomma skivan och nätet finns på alla. Skivorna som inte är med blir
# "", och sidans knappar till dem blir länkar till den andra sidan.
# Körs från emulator/ (Makefile gör det).

set -e
cd "$(dirname "$0")/.."

# Var ROM:arna och skivorna ligger; kan sättas utifrån (det publika
# förrådet har dem på andra ställen, se dess Makefile).
ROM=${ROM:-../roms/abc802rom.rom}
NATROM=${NATROM:-../roms/abc802net.rom}
TECKEN=${TECKEN:-../roms/orig/abc802/ABC802-char.6490191-01.bin}
SKIVOR=${SKIVOR:-../prov/skivor}
FORTH=${FORTH:-../forth/forth.img}
PROGRAM=${PROGRAM:-../forth/program.img}
# Pascal-skivan (make pascal) är med på CP/M-sidan om den finns; den har
# Pascal/MT+, som hämtas och inte ligger i förrådet. Utan den har ingen
# av sidorna knappen Pascal (FILER.harpascal).
PASCAL=${PASCAL:-cpm/pascal.img}
# L.BAC till UFD: (webb/lbac.sh); utan den blir det bara NAT.BAS där.
LBAC=${LBAC:-webb/L.BAC}
SIDA=${SIDA:-grund}
case $SIDA in grund|forth|cpm) ;; *) echo "bygg.sh: SIDA=grund, forth eller cpm" >&2; exit 1;; esac

# Skivan packad och som base64, eller "" om sidan inte har den.
skiva() {
    if [ "$1" = "$SIDA" ]; then
        gzip -9n < "$2" | base64 | tr -d '\n'
    fi
}

base64_av() {
    base64 < "$1" | tr -d '\n'
}

# Mallen utan det som bara syns i mallen.
sida() {
    sed -e '/<!-- MALL -->/,/<!-- \/MALL -->/d' webb/mall.html
}

sida | sed '/<!-- FILER -->/,$d'
printf '<script>\nconst FILER = {\n'
printf '  wasm: "%s",\n' "$(base64_av webb/abc802.wasm)"
printf '  rom: "%s",\n' "$(base64_av "$ROM")"
printf '  natrom: "%s",\n' "$(base64_av "$NATROM")"
printf '  tecken: "%s",\n' "$(base64_av "$TECKEN")"
printf '  sida: "%s",\n' "$SIDA"
printf '  tidsprov: "%s",\n' "$(skiva grund "$SKIVOR/tidsprov.img")"
printf '  tom: "%s",\n' "$(gzip -9n < "$SKIVOR/tom.img" | base64 | tr -d '\n')"
printf '  program: "%s",\n' "$(skiva grund "$PROGRAM")"
printf '  cpm: "%s",\n' "$(skiva cpm cpm/a.img)"
if [ -f "$PASCAL" ]; then
    printf '  pascal: "%s",\n' "$(skiva cpm "$PASCAL")"
    printf '  harpascal: true,\n'
else
    printf '  pascal: "",\n'
    printf '  harpascal: false,\n'
fi
printf '  forth: "%s",\n' "$(skiva forth "$FORTH")"
if [ -f "$LBAC" ]; then
    printf '  lbac: "%s",\n' "$(base64_av "$LBAC")"
else
    printf '  lbac: "",\n'
fi
printf '};\n</script>\n<script>\n'
cat webb/datavision.js webb/bbs.js webb/stordator.js webb/abc802.js
printf '</script>\n'
sida | sed '1,/<!-- FILER -->/d'
