#!/usr/bin/env bash
# Build airfight.p from airfight.asm.
# Run from the project root: bash tools/build_airfight.sh [port]
#   Optional: pass serial port to also load the .p file onto the Pico.

set -e
cd "$(dirname "$0")/.."

echo "=== Compiling make_airfight ==="
cc -std=c11 -Wall -O2 -o tools/make_airfight tools/make_airfight.c

echo "=== Assembling & building airfight.p ==="
./tools/make_airfight games/airfight.asm games/airfight.p

if [ -n "$1" ]; then
    echo "=== Loading onto Pico at $1 ==="
    python3 tools/send_p_termios.py "$1" games/airfight.p
else
    echo "=== Done ==="
    echo "To load: python3 tools/send_p_termios.py /dev/tty.usbmodem* games/airfight.p"
fi
