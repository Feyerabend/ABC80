#!/bin/sh
# bygg.sh -- gör abc80.html, emulatorn i en enda fil, till boken ABC80
# inifrån
#
# Mallen (mall.html) får WebAssembly-koden (abc80.wasm, make webb), de
# två ROM:arna, ABC-DOS, drivrutinen till P40 och TKN80-III som base64, och abc80.js efter, där raden med
# FILER står. Finns exempel/program/diskett/program.dsk kommer
# exempeldisketten med; annars tas dess knapp och text bort (mellan PROGRAM
# och /PROGRAM i mallen). Finns exempel/krets/forth.bin och
# forth_cmos.bin kommer Forth-kretsarna med (KRETS och /KRETS). Finns
# exempel/krets/e1.bin till e6.bin (e4 som e4a.bin och e4b.bin) kommer
# bokens exempel 1-6 med (EXEMPEL och /EXEMPEL). Finns
# exempel/malare/MALARE.wav kommer bandet med MÅLARE med (MALARE och
# /MALARE). Finns cpm/start.bin, cpm/system.dsk och cpm/b.dsk (make)
# kommer CP/M med UNI-80 med (CPM och /CPM). Finns exempel/genesis/ kommer Genesis-demot med
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
EXEMPEL="e1 e2 e3 e4a e4b e5 e6"
MALARE=exempel/malare/MALARE.wav
SKIVA=exempel/genesis/GenesisProject_ABCDemo.dsk
BAND=exempel/genesis/GenesisProject_ABCDemo_webb.wav
CPM_START=cpm/start.bin
CPM_SKIVA=cpm/system.dsk
CPM_B=cpm/b.dsk

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

exemplen() {
    for n in $EXEMPEL; do echo exempel/krets/$n.bin; done
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
        avsnitt EXEMPEL "$(finns $(exemplen))" |
        avsnitt MALARE "$(finns "$MALARE")" |
        avsnitt CPM "$(finns "$CPM_START" "$CPM_SKIVA" "$CPM_B")" |
        avsnitt GENESIS "$(finns "$SKIVA" "$BAND")"
}

sida | sed '/<!-- FILER -->/,$d'
printf '<script>\nconst FILER = {\n'
printf '  wasm: "%s",\n' "$(base64_av webb/abc80.wasm)"
printf '  rom_ny: "%s",\n' "$(base64_av roms/abc80new.rom)"
printf '  rom_gammal: "%s",\n' "$(base64_av roms/abc80old.rom)"
printf '  dos: "%s",\n' "$(base64_av roms/abcdos80.rom)"
printf '  p40: "%s",\n' "$(base64_av roms/p40.rom)"
printf '  tkn80: "%s",\n' "$(base64_av roms/tkn80-iii.rom)"
if [ -f "$PROGRAM" ]; then
    printf '  program: "%s",\n' "$(base64_av "$PROGRAM")"
fi
if [ -f "$KRETS" ] && [ -f "$KRETS_CMOS" ]; then
    printf '  krets: "%s",\n' "$(base64_av "$KRETS")"
    printf '  krets_cmos: "%s",\n' "$(base64_av "$KRETS_CMOS")"
fi
if [ "$(finns $(exemplen))" = ja ]; then
    printf '  exempel: {\n'
    for n in $EXEMPEL; do
        printf '    %s: "%s",\n' $n "$(base64_av exempel/krets/$n.bin)"
    done
    printf '  },\n'
fi
if [ -f "$MALARE" ]; then
    printf '  malare_band: "%s",\n' "$(base64_av "$MALARE")"
fi
if [ -f "$CPM_START" ] && [ -f "$CPM_SKIVA" ] && [ -f "$CPM_B" ]; then
    printf '  cpm_start: "%s",\n' "$(base64_av "$CPM_START")"
    printf '  cpm_skiva: "%s",\n' "$(base64_av "$CPM_SKIVA")"
    printf '  cpm_b: "%s",\n' "$(base64_av "$CPM_B")"
fi
if [ -f "$SKIVA" ] && [ -f "$BAND" ]; then
    printf '  genesis: "%s",\n' "$(base64_av "$SKIVA")"
    printf '  genesis_band: "%s",\n' "$(base64_av "$BAND")"
fi
printf '};\n</script>\n<script>\n'
cat webb/abc80.js
printf '</script>\n'
sida | sed '1,/<!-- FILER -->/d'
