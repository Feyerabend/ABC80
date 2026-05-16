/*
 * zx81_kbd.h  ZX81 keyboard via USB-CDC serial
 *
 * Characters received over USB CDC (Mac terminal → Pico) are mapped to the
 * ZX81 8×5 key matrix and held for a few frames so the ROM poll sees them.
 *
 * ZX81 keyboard matrix
 * --------------------
 *   Row 0: SHIFT  Z  X  C  V
 *   Row 1: A      S  D  F  G
 *   Row 2: Q      W  E  R  T
 *   Row 3: 1      2  3  4  5
 *   Row 4: 0      9  8  7  6
 *   Row 5: P      O  I  U  Y
 *   Row 6: ENTER  L  K  J  H
 *   Row 7: SPACE  .  M  N  B
 */

#pragma once
#include <stdint.h>

/* Call once at startup. */
void zx81_kbd_init(void);

/* Call once per frame (Core 0 main loop).
 * Reads one character from USB CDC and updates the key matrix. */
void zx81_kbd_scan(void);

/* Read the key matrix for a port_in call.
 * hi = high byte of the Z80 I/O address (row selector, active-low).
 * Returns column state active-low: bit n = 0 means that column is pressed. */
uint8_t zx81_kbd_read_port(uint8_t hi);
