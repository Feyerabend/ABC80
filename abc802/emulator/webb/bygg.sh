#!/bin/sh
# bygg.sh -- gör abc802.html, emulatorn i en enda fil, till ABC 802
# inifrån
#
# Mallen (mall.html) får WebAssembly-koden (abc802.wasm, make webb),
# ROM:en, nät-ROM:en (LUXNET) och teckengeneratorn som base64, skivorna (../prov/skivor,
# gor.py, CP/M-skivan cpm/a.img och Forth, ../forth/forth.img) packade med gzip och som base64, och datavision.js, bbs.js,
# stordator.js och abc802.js efter, där raden
# med FILER står. Det som bara syns i mallen (mellan MALL och /MALL) tas
# bort. Filen skrivs på standard ut:
#     webb/bygg.sh > webb/abc802.html
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
printf '  tidsprov: "%s",\n' "$(gzip -9n < "$SKIVOR/tidsprov.img" | base64 | tr -d '\n')"
printf '  tom: "%s",\n' "$(gzip -9n < "$SKIVOR/tom.img" | base64 | tr -d '\n')"
printf '  cpm: "%s",\n' "$(gzip -9n < cpm/a.img | base64 | tr -d '\n')"
printf '  forth: "%s",\n' "$(gzip -9n < "$FORTH" | base64 | tr -d '\n')"
printf '};\n</script>\n<script>\n'
cat webb/datavision.js webb/bbs.js webb/stordator.js webb/abc802.js
printf '</script>\n'
sida | sed '1,/<!-- FILER -->/d'
