/*
 * main.c  ZX81 emulator on Raspberry Pi Pico 2 with DVI output
 *
 * Core assignment
 * ---------------
 *   Core 0  -  ZX81 emulation loop + keyboard scan + scanline feeding
 *   Core 1  -  TMDS encoder (dvi_out_core1_main, never returns)
 *
 * Render loop (core 0, 60 Hz DVI / ~50 Hz ZX81)
 * -----------------------------------------------
 *   zx81_new_frame()        — reset NMI/INT counters for new frame
 *   zx81_screen_prepare()   — scan D_FILE, cache row pointers (~5 µs)
 *   dvi_out_swap_buffers()  — promote back buffer to front
 *   for each DVI scanline:
 *     zx81_run_ts()         — run CPU slice + NMI/INT generation
 *     dvi_out_push_row()    — feed scanline to DVI queue (blocks at 60 Hz)
 *     zx81_screen_render_line() — render that scanline into back buffer
 *   zx81_run_ts()           — drain remainder T-states
 *
 * The render is spread across the 16.7 ms DVI frame (~6 µs per scanline,
 * well within the 63 µs DVI slot).  The inter-frame gap contains only
 * prepare() + swap() — < 10 µs — so push_row(0) fires immediately after
 * swap and the DVI queue never starves.
 *
 * GPIO wiring
 * -----------
 *   DVI Sock (Pico end)  :  GPIO 12 - 19  (TMDS lanes + clock)
 *   Keyboard rows (out)  :  GPIO  0 -  7  (driven HIGH one at a time)
 *   Keyboard cols (in)   :  GPIO  8 - 11  (pull-down; HIGH when pressed)
 *   Keyboard col 4 (in)  :  GPIO 20       (pull-down; skips DVI range)
 *
 * ZX81 keyboard layout
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

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include <stdio.h>

#include "dvi_out.h"
#include "color.h"
#include "zx81.h"
#include "zx81_screen.h"
#include "zx81_kbd.h"
#include "zx81_load.h"
#include "z80_api.h"

/* T-states to run per DVI scanline.
 * ZX81_TS_PER_FRAME / GFX_H = 65000 / 240 = 270 (remainder handled below). */
#define TS_PER_LINE  (ZX81_TS_PER_FRAME / GFX_H)
#define TS_REMAINDER (ZX81_TS_PER_FRAME - TS_PER_LINE * GFX_H)

/* ---- Core 1 entry -------------------------------------------------------- */

static void core1_entry(void) {
    dvi_out_core1_main();   /* never returns */
}

/* ---- Main ---------------------------------------------------------------- */

int main(void)
{
    /* dvi_out_init() must be first: raises sys_clock to 252 MHz.
       USB stdio must be initialised after the PLL settles. */
    dvi_out_init();
    stdio_init_all();

    multicore_launch_core1(core1_entry);

    zx81_init();

    uint32_t last_report = 0;

    while (true) {
        /* zx81_load_tick() consumes serial bytes while a transfer is in
         * progress; skip kbd_scan during that window so keystrokes don't
         * interfere with the incoming binary data. */
        if (!zx81_load_tick())
            zx81_kbd_scan();
        zx81_new_frame();

        /* Scan the display file and cache row pointers (~5 µs).
         * This is the only work in the inter-frame gap; push_row(0) fires
         * immediately after swap with < 10 µs delay — no DVI starvation. */
        zx81_screen_prepare();
        dvi_out_swap_buffers();

        /* Interleave CPU execution with DVI scanline feeding.
         * push_row() blocks at the DVI rate (~63 µs/line at 60 Hz).
         * render_line() writes the BACK buffer one scanline at a time while
         * push_row() feeds the FRONT buffer to Core 1 — the two buffers are
         * independent so there is no conflict. */
        for (int y = 0; y < GFX_H; y++) {
            zx81_run_ts(TS_PER_LINE);
            dvi_out_push_row(y);
            zx81_screen_render_line(dvi_out_back_buffer(), y);
        }
        zx81_run_ts(TS_REMAINDER);

        /* Serial heartbeat — once per second.
         * Ctrl+D in terminal triggers a display-file dump (see zx81_kbd.c). */
        uint32_t fc = dvi_out_frame_count();
        if (fc - last_report >= 60) {
            uint16_t d_file = (uint16_t)m[0x400C] | ((uint16_t)m[0x400D] << 8);
            printf("frame=%-6u  pc=0x%04X  d_file=0x%04X  %s\n",
                   (unsigned)fc, (unsigned)pc, (unsigned)d_file,
                   zx81_slow_mode() ? "SLOW" : "FAST");
            last_report = fc;
        }
    }
}
