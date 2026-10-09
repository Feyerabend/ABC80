#!/bin/sh
# bygg.sh -- gör abc80.html, emulatorn i en enda fil, till boken ABC80
# inifrån
#
# Mallen (mall.html) får WebAssembly-koden (abc80.wasm, make webb), de
# två ROM:arna och ABC-DOS som base64, och abc80.js efter, där raden med
# FILER står. Finns exempel/program/diskett/program.dsk kommer
# exempeldisketten med; annars tas dess knapp och text bort (mellan PROGRAM
# och /PROGRAM i mallen). Finns exempel/krets/forth.bin och
# forth_cmos.bin kommer Forth-kretsarna med (KRETS och /KRETS). Finns exempel/genesis/ kommer Genesis-demot med
# (disketten och bandet, i 8 bitar och halva takten); annars tas demots
# knappar och text bort (mellan GENESIS och /GENESIS). Det som bara syns i
# mallen (mellan MALL och /MALL) tas alltid bort. Filen skrivs på
# standard ut:
#     webb/bygg.sh > webb/abc80.html
# Körs från emulator/ (Makefile gör det).

set -e
cd "$(dirname "$0")/.."

PROGRAM=exempel/program/diskett/program.dsk
KRETS=exempel/krets/forth.bin
KRETS_CMOS=exempel/krets/forth_cmos.bin
SKIVA=exempel/genesis/GenesisProject_ABCDemo.dsk
BAND=exempel/genesis/GenesisProject_ABCDemo_webb.wav

base64_av() {
    base64 < "$1" | tr -d '\n'
}

# Ta bort det som står mellan <!-- NAMN --> och <!-- /NAMN --> om villkoret
# inte gäller, annars bara markeringarna.
avsnitt() {
    if [ "$2" = ja ]; then
        sed -e "/<!-- $1 -->/d" -e "/<!-- \/$1 -->/d"
    else
        sed -e "/<!-- $1 -->/,/<!-- \/$1 -->/d"
    fi
}

finns() {
    for f in "$@"; do [ -f "$f" ] || { echo nej; return; }; done
    echo ja
}

# Mallen utan det som inte ska med.
sida() {
    avsnitt MALL nej < webb/mall.html |
        avsnitt PROGRAM "$(finns "$PROGRAM")" |
        avsnitt KRETS "$(finns "$KRETS" "$KRETS_CMOS")" |
        avsnitt GENESIS "$(finns "$SKIVA" "$BAND")"
}

sida | sed '/<!-- FILER -->/,$d'
printf '<script>\nconst FILER = {\n'
printf '  wasm: "%s",\n' "$(base64_av webb/abc80.wasm)"
printf '  rom_ny: "%s",\n' "$(base64_av roms/abc80new.rom)"
printf '  rom_gammal: "%s",\n' "$(base64_av roms/abc80old.rom)"
printf '  dos: "%s",\n' "$(base64_av roms/abcdos80.rom)"
if [ -f "$PROGRAM" ]; then
    printf '  program: "%s",\n' "$(base64_av "$PROGRAM")"
fi
if [ -f "$KRETS" ] && [ -f "$KRETS_CMOS" ]; then
    printf '  krets: "%s",\n' "$(base64_av "$KRETS")"
    printf '  krets_cmos: "%s",\n' "$(base64_av "$KRETS_CMOS")"
fi
if [ -f "$SKIVA" ] && [ -f "$BAND" ]; then
    printf '  genesis: "%s",\n' "$(base64_av "$SKIVA")"
    printf '  genesis_band: "%s",\n' "$(base64_av "$BAND")"
fi
printf '};\n</script>\n<script>\n'
cat webb/abc80.js
printf '</script>\n'
sida | sed '1,/<!-- FILER -->/d'
