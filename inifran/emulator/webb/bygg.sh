#!/bin/sh
# bygg.sh -- gör abc80.html, emulatorn i en enda fil, till boken ABC80
# inifrån
#
# Mallen (mall.html) får WebAssembly-koden (abc80.wasm, make webb), de
# två ROM:arna, ABC-DOS, drivrutinen till P40, skrivarkretsen 9704 och
# TKN80-III som base64, och abc80.js efter, där raden med
# FILER står. Finns exempel/program/diskett/program.dsk kommer
# exempeldisketten med; annars tas dess knapp och text bort (mellan PROGRAM
# och /PROGRAM i mallen). Finns skivor/systemskiva.dsk och demoskiva.dsk
# (skivor/hamta.py) kommer Luxors systemskiva och demoskiva med (SYSTEM
# och /SYSTEM). Finns exempel/krets/forth.bin och
# forth_cmos.bin kommer Forth-kretsarna med (KRETS och /KRETS). Finns
# ../utanfor/forthaid/forthaid.bin (utanför boken) kommer Forthaid med
# (FORTHAID och /FORTHAID). Finns ../utanfor/forthaid2/forthaid2.bin
# och forthaid2_b0.bin med flera (../utanfor/forthaid2/gor.sh) kommer
# Forthaid II med, den fasta delen och bankerna (FORTHAID2 och
# /FORTHAID2). Finns
# exempel/krets/e1.bin till e6.bin (e4 som e4a.bin och e4b.bin) kommer
# bokens exempel 1-6 med (EXEMPEL och /EXEMPEL). Finns
# exempel/malare/MALARE.wav kommer bandet med MÅLARE med (MALARE och
# /MALARE). Finns cpm/start.bin, cpm/system.dsk, cpm/b.dsk och cpm/d.dsk
# (make) kommer CP/M med UNI-80 med (CPM och /CPM); d.dsk, D: på kort 44
# med Zork I, utan de $E5 som avslutar den (abc80.js fyller i dem). Finns exempel/genesis/ kommer Genesis-demot med
# (disketten och bandet, i 8 bitar och halva takten); annars tas demots
# knappar och text bort (mellan GENESIS och /GENESIS). Finns
# roms/basicii80.rom (roms/hamta.py) kommer BASIC II för ABC80 med (B2 och
# /B2), och finns dessutom exempel/program/diskett/program_b2.dsk
# (exempel/program/gor.sh) kommer BASIC II:s exempeldiskett med, som
# Exempeldisketten ger när BASIC II är valt. Det som bara syns i
# mallen (mellan MALL och /MALL) tas alltid bort. Filen skrivs på
# standard ut:
#     webb/bygg.sh > webb/abc80.html
# Körs från emulator/ (Makefile gör det).

set -e
cd "$(dirname "$0")/.."

PROGRAM=exempel/program/diskett/program.dsk
PROGRAM_B2=exempel/program/diskett/program_b2.dsk
SYSTEM=skivor/systemskiva.dsk
DEMO=skivor/demoskiva.dsk
KRETS=exempel/krets/forth.bin
KRETS_CMOS=exempel/krets/forth_cmos.bin
FORTHAID=../utanfor/forthaid/forthaid.bin
FORTHAID2=../utanfor/forthaid2/forthaid2.bin
EXEMPEL="e1 e2 e3 e4a e4b e5 e6"
MALARE=exempel/malare/MALARE.wav
SKIVA=exempel/genesis/GenesisProject_ABCDemo.dsk
BAND=exempel/genesis/GenesisProject_ABCDemo_webb.wav
CPM_START=cpm/start.bin
CPM_SKIVA=cpm/system.dsk
CPM_B=cpm/b.dsk
CPM_D=cpm/d.dsk
B2=roms/basicii80.rom

base64_av() {
    base64 < "$1" | tr -d '\n'
}

# Utan $E5 i slutet.
base64_utan_slutet() {
    python3 -I -c 'import sys; sys.stdout.buffer.write(open(sys.argv[1], "rb").read().rstrip(b"\xe5"))' "$1" |
        base64 | tr -d '\n'
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

forthaid2_banker() {
    ls ../utanfor/forthaid2/forthaid2_b[0-9].bin 2>/dev/null
}

# Forthaid II: den fasta delen och minst en bank.
forthaid2_finns() {
    if [ -f "$FORTHAID2" ] && [ -n "$(forthaid2_banker)" ]; then echo ja; else echo nej; fi
}

finns() {
    for f in "$@"; do [ -f "$f" ] || { echo nej; return; }; done
    echo ja
}

# Mallen utan det som inte ska med.
sida() {
    avsnitt MALL nej < webb/mall.html |
        avsnitt PROGRAM "$(finns "$PROGRAM")" |
        avsnitt SYSTEM "$(finns "$SYSTEM" "$DEMO")" |
        avsnitt KRETS "$(finns "$KRETS" "$KRETS_CMOS")" |
        avsnitt FORTHAID "$(finns "$FORTHAID")" |
        avsnitt FORTHAID2 "$(forthaid2_finns)" |
        avsnitt EXEMPEL "$(finns $(exemplen))" |
        avsnitt MALARE "$(finns "$MALARE")" |
        avsnitt CPM "$(finns "$CPM_START" "$CPM_SKIVA" "$CPM_B" "$CPM_D")" |
        avsnitt GENESIS "$(finns "$SKIVA" "$BAND")" |
        avsnitt B2 "$(finns "$B2")"
}

sida | sed '/<!-- FILER -->/,$d'
printf '<script>\nconst FILER = {\n'
printf '  wasm: "%s",\n' "$(base64_av webb/abc80.wasm)"
printf '  rom_ny: "%s",\n' "$(base64_av roms/abc80new.rom)"
printf '  rom_gammal: "%s",\n' "$(base64_av roms/abc80old.rom)"
if [ -f "$B2" ]; then
    printf '  rom_b2: "%s",\n' "$(base64_av "$B2")"
    if [ -f "$PROGRAM_B2" ]; then
        printf '  program_b2: "%s",\n' "$(base64_av "$PROGRAM_B2")"
    fi
fi
printf '  dos: "%s",\n' "$(base64_av roms/abcdos80.rom)"
printf '  p40: "%s",\n' "$(base64_av roms/p40.rom)"
printf '  pr9704: "%s",\n' "$(base64_av roms/pr9704.rom)"
printf '  tkn80: "%s",\n' "$(base64_av roms/tkn80-iii.rom)"
if [ -f "$PROGRAM" ]; then
    printf '  program: "%s",\n' "$(base64_av "$PROGRAM")"
fi
if [ "$(finns "$SYSTEM" "$DEMO")" = ja ]; then
    printf '  systemskiva: "%s",\n' "$(base64_av "$SYSTEM")"
    printf '  demoskiva: "%s",\n' "$(base64_av "$DEMO")"
fi
if [ -f "$KRETS" ] && [ -f "$KRETS_CMOS" ]; then
    printf '  krets: "%s",\n' "$(base64_av "$KRETS")"
    printf '  krets_cmos: "%s",\n' "$(base64_av "$KRETS_CMOS")"
fi
if [ -f "$FORTHAID" ]; then
    printf '  forthaid: "%s",\n' "$(base64_av "$FORTHAID")"
fi
if [ "$(forthaid2_finns)" = ja ]; then
    printf '  forthaid2: "%s",\n' "$(base64_av "$FORTHAID2")"
    printf '  forthaid2_banker: [\n'
    for f in $(forthaid2_banker); do
        printf '    "%s",\n' "$(base64_av "$f")"
    done
    printf '  ],\n'
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
if [ "$(finns "$CPM_START" "$CPM_SKIVA" "$CPM_B" "$CPM_D")" = ja ]; then
    printf '  cpm_start: "%s",\n' "$(base64_av "$CPM_START")"
    printf '  cpm_skiva: "%s",\n' "$(base64_av "$CPM_SKIVA")"
    printf '  cpm_b: "%s",\n' "$(base64_av "$CPM_B")"
    printf '  cpm_d: "%s",\n' "$(base64_utan_slutet "$CPM_D")"
fi
if [ -f "$SKIVA" ] && [ -f "$BAND" ]; then
    printf '  genesis: "%s",\n' "$(base64_av "$SKIVA")"
    printf '  genesis_band: "%s",\n' "$(base64_av "$BAND")"
fi
printf '};\n</script>\n<script>\n'
cat webb/abc80.js
printf '</script>\n'
sida | sed '1,/<!-- FILER -->/d'
