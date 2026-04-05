// ABC80 emulator -- Raspberry Pi Pico 2W + Pimoroni Display Pack 2.0
//
// Screen: 40 x 24 character cells, each 8 x 10 px -> 320 x 240 total, exact fit.
// Keyboard: USB CDC serial.  Connect any terminal (any baud - USB CDC ignores it).
// Strobe: 50 Hz repeating hardware timer -> generates the Z80 keyboard interrupt.

#include "pico/stdlib.h"
#include "hardware/timer.h"
#include <stdio.h>
#include <string.h>

#include "display.h"
#include "abc80.h"
#include "monitor.h"

// ---------------------------------------------------------------------------
// White-on-black colour scheme (classic ABC80 monitor look).
// Cursor cells are shown as reverse video.
#define ABC_FG  COLOR_WHITE
#define ABC_BG  COLOR_BLACK

// ---------------------------------------------------------------------------
// Display refresh target: ~30 fps
#define FRAME_US  33333ULL

// CPU-side framebuffer: filled by the CPU, then pushed to the LCD via DMA.
static uint16_t framebuffer[DISPLAY_WIDTH * DISPLAY_HEIGHT];

// ---------------------------------------------------------------------------
// 50 Hz keyboard strobe
//
// The ABC80 hardware generates a 50 Hz strobe that triggers a Z80 interrupt.
// Without it the BASIC input loop does not advance and the screen scrolls
// uncontrolled.  We use a repeating hardware timer; the callback sets a flag
// that the main loop converts into a gen_int() call outside interrupt context.
static volatile bool strobe_pending = false;

static bool strobe_callback(repeating_timer_t *rt) {
    (void)rt;
    strobe_pending = true;
    return true;   // keep repeating
}

// ---------------------------------------------------------------------------
// Render the ABC80 screen RAM to the framebuffer and blit to the display.
//
// The ABC80 screen byte format:
//   bit 7    = cursor attribute -> display as reverse video
//   bits 6-0 = character code (SIS 662241, identical to abcfont.h indices)
//
// abcfont.h already contains the authentic ABC80 character ROM, so Swedish
// characters (ä ö å Ä Ö Å é ü Ü ¤) render correctly without any translation.
static void screen_refresh(void) {
    // Blink state: toggle every 330 ms so bit-7 cells alternate
    // between reverse-video and normal, giving the cursor/blink effect.
    static uint64_t blink_last = 0;
    static bool     blink_on   = true;
    uint64_t now_us = time_us_64();
    if (now_us - blink_last >= 330000ULL) {
        blink_last = now_us;
        blink_on   = !blink_on;
    }

    fb_clear(framebuffer, ABC_BG);

    for (int row = 0; row < 24; row++) {
        bool graphics = false;
        for (int col = 0; col < 40; col++) {
            uint8_t cell = abc80_screen_char(row, col);

            if ((cell & 0x7F) == 0x17) {   // CHR$(151) = 0x97: enter graphics mode
                graphics = true;
                fb_draw_char(framebuffer, col * 8, row * 10, ' ', ABC_FG, ABC_BG);
                continue;
            }
            if ((cell & 0x7F) == 0x07) {   // CHR$(135) = 0x87: exit graphics mode
                graphics = false;
                fb_draw_char(framebuffer, col * 8, row * 10, ' ', ABC_FG, ABC_BG);
                continue;
            }

            if (graphics) {
                // Map character to mosaic pattern per ABC80 video hardware:
                //   0x40-0x5F: uppercase alpha – displayed as text even in graphics mode.
                //   bit5 must be set for a visible block; otherwise blank.
                //   pattern = (ch & 0x1F) | ((ch & 0x40) >> 1)  --> 0..63
                //   charRom index = 0xA0 + pattern
                uint8_t ch = cell & 0x7F;
                if (ch >= 0x40 && ch <= 0x5F) {
                    // Uppercase letters always shown as text characters
                    fb_draw_char(framebuffer, col * 8, row * 10, (char)ch, ABC_FG, ABC_BG);
                } else if (ch & 0x20) {
                    uint8_t pat = (ch & 0x1F) | ((ch & 0x40) >> 1);
                    fb_draw_char(framebuffer, col * 8, row * 10, (char)(0xA0 + pat), ABC_FG, ABC_BG);
                } else {
                    fb_draw_char(framebuffer, col * 8, row * 10, ' ', ABC_FG, ABC_BG);
                }
            } else {
                bool    inverse = (cell & 0x80) && blink_on;
                char    c       = (char)(cell & 0x7F);
                if (c < 0x20) c = ' ';   // control codes --> blank
                uint16_t fg = inverse ? ABC_BG : ABC_FG;
                uint16_t bg = inverse ? ABC_FG : ABC_BG;
                fb_draw_char(framebuffer, col * 8, row * 10, c, fg, bg);
            }
        }
    }

    display_wait_for_dma();
    display_blit_full(framebuffer);
}

// ---------------------------------------------------------------------------
int main(void) {
    stdio_init_all();       // start USB CDC; terminal can connect at any point
    display_pack_init();
    buttons_init();
    display_set_backlight(true);

    abc80_init();

    // 50 Hz strobe: negative delay means "fire every |ms| ms, period,
    // not delay-after-completion", giving a rock-steady 20 ms interval.
    repeating_timer_t strobe_timer;
    add_repeating_timer_ms(-20, strobe_callback, NULL, &strobe_timer);

    uint64_t last_frame = time_us_64();

    while (true) {
        if (!monitor_is_active()) {
            // --- Normal ABC80 operation ---

            // 1. On each 50 Hz strobe: deliver the keyboard interrupt.
            if (strobe_pending) {
                strobe_pending = false;
                abc80_strobe();
            }

            // 2. Poll keyboard every ~500 steps.
            static unsigned poll_div;
            if (++poll_div >= 500) {
                poll_div = 0;
                abc80_keyboard_poll();
            }

            // 3. Execute one Z80 instruction.
            abc80_step();
        } else {
            // --- Monitor mode: Z80 frozen, serial goes to monitor ---
            static unsigned mon_poll_div;
            if (++mon_poll_div >= 500) {
                mon_poll_div = 0;
                monitor_serial_poll();
            }
        }

        // Refresh display at ~30 fps.
        uint64_t now = time_us_64();
        if (now - last_frame >= FRAME_US) {
            if (monitor_is_active()) {
                monitor_render(framebuffer);
                display_wait_for_dma();
                display_blit_full(framebuffer);
            } else {
                screen_refresh();
            }
            buttons_update();
            last_frame = time_us_64();

            // Button X: toggle monitor mode.
            if (button_just_pressed(BUTTON_X)) {
                if (monitor_is_active()) {
                    monitor_exit();
                } else {
                    monitor_enter();
                }
            }

            // Button Y: reset the Z80/ABC80 (only in normal mode).
            if (!monitor_is_active() && button_just_pressed(BUTTON_Y)) {
                abc80_init();
                strobe_pending = false;
            }
        }
    }
}
