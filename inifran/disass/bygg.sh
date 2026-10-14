#!/bin/sh
# Bygg en kommenterad disassemblering ur mallen och ROM:en:
#   ./bygg.sh abc80 [ROM]      ->  ut/abc80rom.pdf   (ABC80:s BASIC-ROM, 11273)
#   ./bygg.sh basicii80 [ROM]  ->  ut/basicii80.pdf  (BASIC II för ABC80)
# Mallen (<namn>-mall.tex) har namnen och kommentarerna men inte koden; den
# fylls i ur ROM:en (fyll.py), som måste ha mallens kontrollsumma. Utan ROM
# hämtas den: ABC80:s fyra kretsar ZA3506-ZA3509 från abc80.net:s arkiv,
# BASIC II:s två halvor därifrån också. Kräver python3, curl och tectonic.
# I abc80-förrådet hämtas band 1:s kapitel ur ../../band1 och stilen ur
# ../../gemensamt; i det publika förrådet ligger allt här (inifran.sh).
set -e
cd "$(dirname "$0")"
namn=${1:?"./bygg.sh abc80|basicii80 [ROM]"}
rom=$2
mkdir -p ut/listningar
hamta() {      # hamta MÅL URL...: URL:erna efter varandra i MÅL
  [ -f "$1" ] && return
  m=$1; shift
  : > "$m.tmp"
  for u in "$@"; do curl -fsSL "$u" >> "$m.tmp"; done
  mv "$m.tmp" "$m"
}
case $namn in
abc80)
  if [ -z "$rom" ]; then
    rom=ut/abc80old.rom
    u=https://www.abc80.net/archive/luxor/Prom/fw/ABC80
    hamta $rom $u/ABC80-ZA3506-11273.bin $u/ABC80-ZA3507-11273.bin \
               $u/ABC80-ZA3508-11273.bin $u/ABC80-ZA3509-11273.bin
  fi
  python3 fyll.py abc80-mall.tex "$rom" > ut/listningar/fylld.tex
  # kod i citat i skrivmaskinsstil, som i boken
  kw=kwfix.py; [ -f $kw ] || kw=../kwfix.py
  python3 $kw ut/listningar/fylld.tex ut/listningar/abc80.tex
  rm ut/listningar/fylld.tex
  hv=abc80rom.tex; [ -f $hv ] || hv=../abc80rom.tex
  sp="-Z search-path=ut"
  for d in ../../band1 ../../gemensamt; do [ -d $d ] && sp="$sp -Z search-path=$d"; done
  pdf=abc80rom ;;
basicii80)
  if [ -z "$rom" ]; then
    rom=ut/basicii80a.rom
    u=https://www.abc80.net/archive/luxor/Prom/fw/ABC80
    hamta $rom $u/basicii_abc80_1.bin $u/basicii_abc80_2.bin
  fi
  python3 fyll.py basicii80-mall.tex "$rom" > ut/basicii80.tex
  hv=ut/basicii80.tex sp=
  pdf=basicii80 ;;
*)
  echo "okänt namn: $namn (abc80 eller basicii80)"; exit 1 ;;
esac
tectonic -o ut $sp $hv 2>&1 | grep -v '^note:\|^warning: .*\(Overfull\|Underfull\)' || true
[ -f ut/$pdf.pdf ] && echo "klart: ut/$pdf.pdf"
