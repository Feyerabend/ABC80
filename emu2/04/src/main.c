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
//   A (GPIO 0)   unusable: VGA Blue MSB DAC resistor holds pin permanently low
//   B (GPIO 6)   hold ~130 ms to toggle between emulator and monitor
//   C (GPIO 11)  hold ~130 ms to exit monitor (backup; try on your board)

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

/* Swap buffers:   called after rendering into back_fb is complete.
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
                bool inverse = (cell & 0x80) && blink_on;
                char c       = (char)(cell & 0x7F);
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

    /* Start VGA output on Core 1: active_fb must be valid before this. */
    multicore_launch_core1(core1_entry);

    abc80_init();

    repeating_timer_t strobe_timer;
    add_repeating_timer_ms(-20, strobe_callback, NULL, &strobe_timer);

    uint64_t last_frame = time_us_64();

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
            if (abc80_check_break())
                monitor_enter();
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

            /* Read buttons during vblank (pins shared with VGA color LSBs).
             *
             * Button A (GPIO 0, Blue MSB): the small DAC resistor + 75 Ω
             *   termination permanently pulls it below the input threshold —
             *   always reads as pressed, cannot be used.
             *
             * Button B (GPIO 6, Green[1]): toggle monitor / emulator.
             * Button C (GPIO 11): exit monitor only (backup; try on your board).
             *
             * Multi-frame hold debounce: require BTN_HOLD_FRAMES consecutive
             * pressed readings (~130 ms at 30 fps) before acting.  Real presses
             * hold far longer; single-frame DAC-noise glitches are filtered out.
             * A shared cooldown prevents immediate re-trigger after a transition. */
#define BTN_HOLD_FRAMES  4
#define BTN_COOLDOWN_US  500000ULL

            static uint8_t  btn_b_hold = 0, btn_c_hold = 0;
            static uint64_t btn_cooldown = 0;

            uint8_t btn = display_buttons_read();

            if (now >= btn_cooldown) {
                /* Button B: toggle monitor on / off */
                if (btn & (1u << 1)) {
                    if (++btn_b_hold >= BTN_HOLD_FRAMES) {
                        btn_b_hold = btn_c_hold = 0;
                        btn_cooldown = now + BTN_COOLDOWN_US;
                        if (monitor_is_active()) monitor_exit();
                        else                     monitor_enter();
                    }
                } else {
                    btn_b_hold = 0;
                }

                /* Button C: exit monitor only.
                 * Gated to monitor-active so a constantly-low GPIO 11 can't
                 * trigger the cooldown (and block Button B) outside the monitor. */
                if (monitor_is_active() && (btn & (1u << 2))) {
                    if (++btn_c_hold >= BTN_HOLD_FRAMES) {
                        btn_c_hold = 0;
                        btn_cooldown = now + BTN_COOLDOWN_US;
                        monitor_exit();
                    }
                } else {
                    btn_c_hold = 0;
                }
            } else {
                /* Reset hold counters during cooldown to prevent carry-over. */
                btn_b_hold = btn_c_hold = 0;
            }
        }
    }
}
