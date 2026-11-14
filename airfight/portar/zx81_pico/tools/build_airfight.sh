#!/usr/bin/env bash
# Build airfight.p from airfight.asm.
# Run from the project root: bash tools/build_airfight.sh [port]
#   Optional: pass serial port to also load the .p file onto the Pico.

set -e
cd "$(dirname "$0")/.."

if [ ! -x asm/z80asm ] || [ asm/z80asm.c -nt asm/z80asm ] || [ asm/z80asm.h -nt asm/z80asm ]; then
    echo "=== Building asm/z80asm ==="
    "${CC:-cc}" -std=c11 -O2 -o asm/z80asm asm/z80asm.c
fi

echo "=== Assembling & building airfight.p ==="
python3 tools/make_p.py games/airfight.asm --header

if [ -n "$1" ]; then
    echo "=== Loading onto Pico at $1 ==="
    python3 tools/send_p_termios.py "$1" games/airfight.p
else
    echo "=== Done ==="
    echo "To load: python3 tools/send_p_termios.py /dev/tty.usbmodem* games/airfight.p"
fi
