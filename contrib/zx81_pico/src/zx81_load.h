/*
 * zx81_load.h  ZX81 .p file loader via USB-CDC serial
 *
 * Protocol
 * --------
 *   1. Ctrl+L in the terminal triggers load mode.
 *   2. Pico prints "LOAD: waiting...\r\n" and enters receive state.
 *   3. Sender writes: [lo] [hi] [data bytes...]
 *      where lo/hi is the file length as a 2-byte little-endian uint16.
 *   4. After receiving all bytes, Pico writes them to m[0x4009], resets
 *      the CPU (cold start from 0x0000), and prints "OK\r\n".
 *
 * Usage (Mac / Linux)
 * -------------------
 *   python3 tools/send_p.py /dev/tty.usbmodem*  game.p   # macOS
 *   python3 tools/send_p.py /dev/ttyACM0         game.p   # Linux
 */

#pragma once
#include <stdbool.h>

/* Call once per frame.  Returns true if a load just completed (CPU was reset). */
bool zx81_load_tick(void);

/* Enter load mode immediately (called when Ctrl+L is received). */
void zx81_load_start(void);
