#!/bin/sh
# lbac.sh -- gör L.BAC, Dataindustriers L (X.19, 1985) som listar UFD:
# på nätet, av texten L.BAS, med emulatorn själv:
#     webb/lbac.sh ../roms/luxnet/l.bas webb/L.BAC
# Körs från emulator/ (Makefile gör det) och kräver ./abc802 och
# nät-ROM:en (NATROM, som i bygg.sh).
#
# Texten blir poster som DOS:en läser (CR efter varje rad, $03 när
# posten är slut, 253 byte i posten) i en tillfällig UFD:-mapp; BASIC II
# läser in den (LOAD tar ett par minuter i maskinens tid, därför -t) och
# sparar den som L.BAC.

set -e
cd "$(dirname "$0")/.."

NATROM=${NATROM:-../roms/abc802net.rom}
IN=$1
UT=$2
[ -n "$IN" ] && [ -n "$UT" ] || { echo "så här: webb/lbac.sh l.bas L.BAC" >&2; exit 1; }

MAPP=$(mktemp -d)
trap 'rm -rf "$MAPP"' EXIT

python3 -I -c '
import sys
rader = open(sys.argv[1], "rb").read().replace(b"\r\n", b"\n").split(b"\n")
ut = bytearray(); post = bytearray()
for r in rader:
    if not r.strip():
        continue
    r = r.rstrip(b"\r") + b"\r"
    if len(post) + len(r) > 252:
        ut += post + b"\x03" + bytes(253 - len(post) - 1)
        post = bytearray()
    post += r
ut += post + b"\x03" + bytes(253 - len(post) - 1)
open(sys.argv[2], "wb").write(ut)
' "$IN" "$MAPP/L.BAS"

./abc802 -r "$NATROM" -N "$MAPP" -t 20000 \
    -k '\w2500LOAD UFD:L.BAS\r\w150000SAVE UFD:L\r' > "$MAPP/skarm.txt"

if [ ! -s "$MAPP/L.BAC" ]; then
    echo "lbac.sh: ingen L.BAC; skärmen:" >&2
    cat "$MAPP/skarm.txt" >&2
    exit 1
fi
cp "$MAPP/L.BAC" "$UT"
