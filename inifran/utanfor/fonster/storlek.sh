#!/bin/bash
# storlek.sh -- steg 7: hur stor blir grafikkortets tolk (emulator/karna/
# hr80.c) med fönstren och musen, och hade den fått plats i kortets 4 KB
# EPROM och 2 KB arbetsminne? Ingen kompilator ger det som skrevs för
# hand 1984, så tolken kompileras för fyra processorer och jämförs med
# tolken före fönstren (4a0221c, steg 1): Z80 med SDCC, och AVR (8 bitar),
# MSP430 (16 bitar) och Cortex-M0 (Thumb) med LLVM:s clang, alla -Oz
# eller --opt-code-size. hr80_rita räknas inte: den gör värdens bild, och
# på kortet gör bildutmatningen det (pekaren som överlagring).
# Slutsatserna står i STORLEK.md.
#
# Kräver clang från LLVM med målen avr, msp430 och thumb (Homebrew llvm;
# Apples clang har inte avr och msp430) och sdcc (Homebrew sdcc); det som
# saknas hoppas över. Före-tolken tas ur git och hoppas över utanför
# abc80-förrådet.
cd "$(dirname "$0")" || exit 1
K=../../emulator/karna
LIBC=$(cd ../../emulator/webb/libc && pwd)
CLANG=${CLANG:-$(ls /opt/homebrew/opt/llvm/bin/clang 2>/dev/null || command -v clang)}
NM=${NM:-$(dirname "$CLANG")/llvm-nm}
T=$(mktemp -d) || exit 1
trap 'rm -rf "$T"' EXIT

mkdir -p "$T/nu" "$T/fore"
cp $K/hr80.c $K/hr80.h $K/teckensnitt.h "$T/nu/"
for f in hr80.c hr80.h teckensnitt.h; do
  git show 4a0221c:emulator/karna/$f > "$T/fore/$f" 2>/dev/null || { rm -rf "$T/fore"; break; }
done

# Koden i byte utan hr80_rita: katalog, mål, flaggor.
clang_kod() {
  local x=""; [ "$2" = avr ] && x=-mmcu=atmega128
  "$CLANG" --target=$2 $x $3 -ffreestanding -isystem "$LIBC" -c "$1/hr80.c" -o "$1/$2.o" 2>/dev/null || return 1
  "$NM" -S --radix=d "$1/$2.o" | awk 'NF == 4 && $3 ~ /[tTrR]/ && $4 !~ /hr80_rita/ { s += $2 } END { print s }'
}
# Z80 med SDCC: funktionernas storlek ur listningen, en rad "namn byte".
z80_funktioner() {
  ( cd "$1" && sdcc -mz80 --opt-code-size --max-allocs-per-node 3000 -c hr80.c 2>/dev/null ) || return 1
  local slut=$(awk '$2 == "_CODE" { print $4; exit }' "$1/hr80.sym") namn adr forra="" fadr
  { awk '
      /\.area/ { area = $NF }
      area == "_CODE" && $NF ~ /^_[A-Za-z0-9_]+::?$/ && $1 ~ /^[0-9A-F]+$/ {
        n = $NF; sub(/^_/, "", n); sub(/:+$/, "", n); print n, $1 }' "$1/hr80.lst"
    echo "- $slut"; } |
  while read -r namn adr; do
    [ -n "$forra" ] && echo "$forra $((16#$adr - 16#$fadr))"
    forra=$namn fadr=$adr
  done
}

echo "Koden i byte, utan hr80_rita (före fönstren / med fönster och mus):"
for mal in z80 avr msp430 thumbv6m-none-eabi; do
  rad="  $mal"
  for d in fore nu; do
    [ -d "$T/$d" ] || { rad="$rad -"; continue; }
    if [ $mal = z80 ]; then
      command -v sdcc > /dev/null || { rad="$rad -"; continue; }
      b=$(z80_funktioner "$T/$d" | awk '$1 !~ /^hr80_rita/ { s += $2 } END { print s }')
    else
      b=$(clang_kod "$T/$d" $mal -Oz) || b=-
    fi
    rad="$rad $b"
  done
  echo "$rad"
done

# Grupperna, med Z80-listningen och MSP430 utan inlining.
FONSTER="i_fonster synlig plats klipp yta valj rita_fonster hela_fonstret snittar stang lyft oppna oppna_klart stang_alla fyll_ruta"
MUS="ko_lagg hr80_handelse fraga hr80_svar flytta fonster_vid drag_hamnar tryck hr80_mus"
TECKEN="teckensnitt teckensnitt_under"
gruppera() {
  awk -v f=" $FONSTER " -v m=" $MUS " -v t=" $TECKEN " '
    $1 ~ /^hr80_rita/ || $1 ~ /^(__|\.)/ { next }
    { g = index(f, " " $1 " ") ? "fönstren" : index(m, " " $1 " ") ? "musen och frågan" : index(t, " " $1 " ") ? "teckensnittet" : "tolken"; s[g] += $2 }
    END { printf "    tolken %d, teckensnittet %d, fönstren %d, musen och frågan %d\n", s["tolken"], s["teckensnittet"], s["fönstren"], s["musen och frågan"] }'
}
echo "Med fönster och mus, per del:"
if command -v sdcc > /dev/null; then
  echo "  z80"; z80_funktioner "$T/nu" | gruppera
fi
if "$CLANG" --target=msp430 -Oz -fno-inline -ffreestanding -isystem "$LIBC" -c "$T/nu/hr80.c" -o "$T/nu/m.o" 2>/dev/null; then
  echo "  msp430"; "$NM" -S --radix=d "$T/nu/m.o" | awk 'NF == 4 && $3 ~ /[tTrR]/ { print $4, $2 }' | gruppera
fi

# Arbetsminnet i C-tolken (16 bitars int, MSP430), utöver bildminnet.
cat > "$T/nu/s.c" <<'EOF'
#include <stddef.h>
#include "hr80.h"
const int allt = sizeof(Hr80) - HR80_MINNE;
const int makron = HR80_MAKROMINNE;
const int index = offsetof(Hr80, makro_slut) - offsetof(Hr80, makro_start);
const int fonster = sizeof(((Hr80 *)0)->fonster) + sizeof(((Hr80 *)0)->ordning);
const int ko = sizeof(((Hr80 *)0)->ko);
EOF
if "$CLANG" --target=msp430 -O2 -ffreestanding -isystem "$LIBC" -S "$T/nu/s.c" -o "$T/nu/s.s" 2>/dev/null; then
  echo "Arbetsminnet i C-strukturen utöver bildminnet (MSP430):"
  awk '/^[a-z]+:$/ { n = $1 } /\.short/ && n { printf "  %s %d", n, $2; n = "" } END { print "" }' "$T/nu/s.s"
fi
f=$(sed -n 's/^#define FYLLSTACK *\([0-9]*\).*/\1/p' $K/hr80.c)
echo "  fyllningens stack: $f poster om 4 byte = $((4 * f)) byte, utanför strukturen"
