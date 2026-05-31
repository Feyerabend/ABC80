/*
 * zx81.h  ZX81 machine wrapper
 *
 * Memory map
 * ----------
 *   0x0000 – 0x1FFF   ROM (8 KB)
 *   0x2000 – 0x3FFF   ROM mirror (same 8 KB)
 *   0x4000 – 0xBFFF   RAM (32 KB; real machine had 1–16 KB, we give the full space)
 *   0xC000 – 0xFFFF   Unmapped → reads 0xFF
 *
 * Interrupt schedule (SLOW mode, 50 Hz PAL)
 * ------------------------------------------
 *   NMI  every ZX81_STEPS_PER_NMI instructions  for ZX81_DISPLAY_LINES lines
 *   INT  once per frame (keyboard scan), after display area
 *
 * The NMI handler (ROM 0x0066) checks CDFLAG (m[0x4001] bit 7) to decide
 * whether to generate a display line or return immediately, so firing NMI
 * unconditionally is safe in both SLOW and FAST modes.
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

/* ZX81 runs at 3.25 MHz, 50 Hz PAL → 65 000 T-states/frame.
 * NMI (hsync) fires every 207 T-states — one per display scanline. */
#define ZX81_TS_PER_FRAME  65000
#define ZX81_TS_PER_NMI      207

/* Initialise CPU, load ROM, set up memory map. */
void zx81_init(void);

/* Signal start of a new display frame.  Call once per frame before
 * any zx81_run_ts calls. */
void zx81_new_frame(void);

/* Run the Z80 for approximately ts_budget T-states, generating NMI/INT
 * at T-state-accurate intervals (207 T-states per NMI). */
void zx81_run_ts(int ts_budget);

/* Returns true when the NMI flip-flop is set (SLOW/display mode).
 * False means FAST mode: NMI suppressed, CPU runs uninterrupted. */
bool zx81_slow_mode(void);

/* Called after a .p file is loaded over serial.  Writes the initial
 * system-variable bytes (0x4000–0x4008), places a minimal display-file
 * marker at 0x7FFC, resets NMI state, and jumps to ROM address 0x0207
 * (post-load entry) — bypassing the cold-start that would wipe the program. */
void zx81_p_load_launch(void);

/* Launch machine code directly at 'entry'.  Disables NMI, resets CPU
 * registers, and sets PC = entry.  The machine code is responsible for
 * setting up D_FILE and any other state it needs. */
void zx81_game_launch(uint16_t entry);
