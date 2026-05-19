#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "dvi_output.h"
#include "memory.h"
#include "vic_chip.h"
#include "via.h"
#include "vic_kbd.h"

// fake6502 API
extern void    reset6502(void);
extern int     step6502(void);
extern int     irq6502(void);
extern uint8_t getP(void);    // read 6502 status byte (bit 2 = I flag)

// CPU cycles to run per DVI scanline.
// Total frame: VIC_CYCLES_PER_LINE * VIC_LINES_PER_FRAME = 65 * 261 = 16965.
// Spread across DVI_BUF_LINES = 240 lines: 16965 / 240 = 70 cycles/line.
// Remainder: 16965 - 70 * 240 = 165 cycles (run after the scanline loop).
#define CPU_CYCLES_PER_DVI_LINE \
    ((VIC_CYCLES_PER_LINE * VIC_LINES_PER_FRAME) / DVI_BUF_LINES)
#define CPU_CYCLES_REMAINDER \
    ((VIC_CYCLES_PER_LINE * VIC_LINES_PER_FRAME) - CPU_CYCLES_PER_DVI_LINE * DVI_BUF_LINES)

static void core1_entry(void) {
    dvi_output_core1_main();
}

int main(void) {
    dvi_output_init();   // vreg + 252 MHz clock first
    stdio_init_all();

    memory_init();
    vic_reset();
    via_reset();
    vic_kbd_init();
    reset6502();

    multicore_launch_core1(core1_entry);

    // Render the initial VIC state into the back buffer so frame 0 is
    // not garbage, then swap it to front.  Core 1 is now running and
    // waiting for scanlines; the first push_row call will wake it.
    uint16_t *back = dvi_output_back_buf();
    for (int y = 0; y < DVI_BUF_LINES; y++)
        vic_render_dvi_line(y, back + y * DVI_BUF_PIXELS);
    dvi_output_swap();

    // Main loop — interleaved CPU + DVI push + render.
    //
    // Each dvi_output_push_row() blocks for ~63 µs (DVI scanline period).
    // Running CPU and rendering within that 63 µs slot paces the entire
    // loop to exactly one DVI frame (240 * 63 µs ≈ 16.7 ms = 60 Hz).
    // This prevents the TMDS queue from ever running dry (which would
    // produce solid red scanlines — see dvi_output.h for the full explanation).
    while (1) {
        back = dvi_output_back_buf();

        uint32_t total_cycles = 0;

        for (int y = 0; y < DVI_BUF_LINES; y++) {
            // Run 6502 for this scanline's slice of cycles.
            uint32_t done = 0;
            while (done < CPU_CYCLES_PER_DVI_LINE)
                done += (uint32_t)step6502();
            total_cycles += done;

            // Feed the front buffer scanline to DVI (blocks at 60 Hz rate).
            dvi_output_push_row(y);

            // Render this scanline into the back buffer for the next frame.
            vic_render_dvi_line(y, back + y * DVI_BUF_PIXELS);
        }

        // Drain the remaining frame cycles (rounding remainder).
        uint32_t done = 0;
        while (done < CPU_CYCLES_REMAINDER)
            done += (uint32_t)step6502();
        total_cycles += done;

        vic_tick(total_cycles);

        // Pulse VIA1 Timer1 IRQ at 60 Hz — but only when the 6502's I flag
        // is clear (interrupts enabled).  This fake6502 irq6502() does NOT
        // check the I flag itself; calling it with I=1 would hijack the CPU
        // mid-KERNAL-init and corrupt the startup sequence.
        if (!(getP() & 0x04)) {
            via1_fire_timer1();
            irq6502();
        }

        // Scan USB-CDC for keyboard input once per frame.
        vic_kbd_scan();

        // Promote the newly rendered back buffer to front for next iteration.
        dvi_output_swap();
    }
}
