// ABC80 emulator -- Raspberry Pi Pico 2 + Pimoroni Pico VGA Demo Base
//
// Screen: 40 x 24 character cells, each 8 x 10 px -> 320 x 240 total, exact fit.
// Keyboard: USB CDC serial.  Connect any terminal (any baud - USB CDC ignores it).
// Strobe: 50 Hz repeating hardware timer -> generates the Z80 keyboard interrupt.
//
// Display: Core 1 runs the VGA scanline loop via pico_scanvideo_dpi.
//          Core 0 renders into the back buffer; buffers swap during vblank.
//
// Buttons (VGA Demo Board, readable during vblank only):
//   A (GPIO 0) — reset ABC80 (not working : use hard reset on board)
//   B (GPIO 6) — toggle monitor mode
//   C (GPIO 11) — unused

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/timer.h"
#include <string.h>

#include "display.h"
#include "abc80.h"
#include "monitor.h"

#define ABC_FG  COLOR_WHITE
#define ABC_BG  COLOR_BLACK

#define FRAME_US  33333ULL   /* ~30 fps */

/* Double-buffered framebuffers.
 * Core 1 reads active_fb each scanline; Core 0 renders into back_fb.
 * Pointer swap is done during vblank (atomic 32-bit write on RP2350). */
static uint16_t fb0[DISPLAY_WIDTH * DISPLAY_HEIGHT];
static uint16_t fb1[DISPLAY_WIDTH * DISPLAY_HEIGHT];
static uint16_t * volatile active_fb = fb0;
static uint16_t *back_fb;   /* initialised in main() */

static void core1_entry(void) {
    display_vga_init();
    display_vga_run(&active_fb);
}

static volatile bool strobe_pending = false;

static bool strobe_callback(repeating_timer_t *rt) {
    (void)rt;
    strobe_pending = true;
    return true;
}

/* Swap buffers — called after rendering into back_fb is complete.
 * Waits for vblank so the swap happens between frames (no tearing). */
static void present_frame(void) {
    uint16_t *new_active = back_fb;
    back_fb = (uint16_t *)active_fb;
    active_fb = new_active;
}

static void screen_refresh(void) {
    static uint64_t blink_last = 0;
    static bool     blink_on   = true;
    uint64_t now_us = time_us_64();
    if (now_us - blink_last >= 330000ULL) {
        blink_last = now_us;
        blink_on   = !blink_on;
    }

    fb_clear(back_fb, ABC_BG);

    for (int row = 0; row < 24; row++) {
        bool graphics = false;
        for (int col = 0; col < 40; col++) {
            uint8_t cell = abc80_screen_char(row, col);

            if ((cell & 0x7F) == 0x17) {
                graphics = true;
                fb_draw_char(back_fb, col * 8, row * 10, ' ', ABC_FG, ABC_BG);
                continue;
            }
            if ((cell & 0x7F) == 0x07) {
                graphics = false;
                fb_draw_char(back_fb, col * 8, row * 10, ' ', ABC_FG, ABC_BG);
                continue;
            }

            if (graphics) {
                if (cell & 0x20) {
                    uint8_t pat = (cell & 0x1F) | ((cell & 0x40) >> 1);
                    fb_draw_char(back_fb, col * 8, row * 10, (char)(0xA0 + pat), ABC_FG, ABC_BG);
                } else {
                    char c = (char)(cell & 0x7F);
                    if (c < 0x20) c = ' ';
                    fb_draw_char(back_fb, col * 8, row * 10, c, ABC_FG, ABC_BG);
                }
            } else {
                bool    inverse = (cell & 0x80) && blink_on;
                char    c       = (char)(cell & 0x7F);
                if (c < 0x20) c = ' ';
                uint16_t fg = inverse ? ABC_BG : ABC_FG;
                uint16_t bg = inverse ? ABC_FG : ABC_BG;
                fb_draw_char(back_fb, col * 8, row * 10, c, fg, bg);
            }
        }
    }

    present_frame();
}

int main(void) {
    stdio_init_all();

    back_fb = fb1;

    /* Start VGA output on Core 1 — active_fb must be valid before this. */
    multicore_launch_core1(core1_entry);

    abc80_init();

    repeating_timer_t strobe_timer;
    add_repeating_timer_ms(-20, strobe_callback, NULL, &strobe_timer);

    uint64_t last_frame = time_us_64();

    /* Button state for edge detection.
     * Start 0xFF (all "pressed") so the first read never triggers a false edge
     * — the VGA PIO drives these pins and can read as low before we get clean
     * button samples during vblank. */
    uint8_t btn_prev = 0xFF;

    while (true) {
        if (!monitor_is_active()) {
            if (strobe_pending) {
                strobe_pending = false;
                abc80_strobe();
            }

            static unsigned poll_div;
            if (++poll_div >= 500) {
                poll_div = 0;
                abc80_keyboard_poll();
            }

            abc80_step();
        } else {
            static unsigned mon_poll_div;
            if (++mon_poll_div >= 500) {
                mon_poll_div = 0;
                monitor_serial_poll();
            }
        }

        uint64_t now = time_us_64();
        if (now - last_frame >= FRAME_US) {
            /* Render into back buffer, then swap */
            if (monitor_is_active()) {
                monitor_render(back_fb);
                present_frame();
            } else {
                screen_refresh();   /* calls present_frame() internally */
            }
            last_frame = time_us_64();

            /* Read buttons during vblank (pins shared with VGA color LSBs) */
            uint8_t btn = display_buttons_read();
            uint8_t pressed = btn & ~btn_prev;
            btn_prev = btn;

            /* Button B — toggle monitor mode.
             * 400 ms cooldown prevents button bounce causing double-toggles. */
            static uint64_t btn_b_ready = 0;
            if ((pressed & (1u << 1)) && now >= btn_b_ready) {
                btn_b_ready = now + 400000;
                if (monitor_is_active()) {
                    /* drain USB FIFO so monitor input doesn't reach ABC80 */
                    while (getchar_timeout_us(0) != PICO_ERROR_TIMEOUT) {}
                    monitor_exit();
                } else {
                    monitor_enter();
                }
            }

            /* Button A (GPIO 0) shares the VGA Blue[0] DAC resistor which
             * pulls it low through the monitor's 75 Ω termination, causing
             * constant false "pressed" readings.  Reset via monitor G 0. */
            (void)btn;
        }
    }
}
