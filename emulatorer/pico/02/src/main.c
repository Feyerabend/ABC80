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
#include "wifi_client.h"

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

// shold add clock updates .. 65010, 65009, 65008 .. here?
// 24 bit binary, decrements every 20ms 
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
    // Blink state: toggle every 330 ms (check?) so bit-7 cells alternate
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
                // ABC80 mosaic encoding in screen RAM (verified from ROM SETDOT/CLRDOT disassembly):
                //   bit7 = TR  (top-right)   — doubles as cursor attribute in text mode
                //   bit6 = TL  (top-left)
                //   bit5 = 1   (graphics flag, always set in any mosaic cell)
                //   bit3 = BR  (bot-right)
                //   bit2 = BL  (bot-left)
                //   bit1 = MR  (mid-right)
                //   bit0 = ML  (mid-left)
                //
                // Font pattern index (char - 0xA0, 0-63):
                //   bit5=BR  bit4=BL  bit3=MR  bit2=ML  bit1=TR  bit0=TL
                //
                // Mapping: pat = TL,TR from bits 6,7  +  ML,MR,BL,BR from bits 0-3

                if (cell & 0x20) {
                    // Map screen RAM byte -> font char (0xA0-0xDF).
                    //
                    // ABC80 graphics encoding (verified against MAME abc80_v.cpp):
                    //   bit0=TL  bit1=TR  bit2=ML  bit3=MR  bit4=BL  bit6=BR
                    //   bit5 unused for dots (always 1 as the "blank" base 0x20)
                    //   bit7 = cursor attribute
                    //
                    // Font pattern index p = char - 0xA0:
                    //   bit0=TL  bit1=TR  bit2=ML  bit3=MR  bit4=BL  bit5=BR
                    //
                    // Bits 0-4 map directly; bit6 (BR) shifts down to bit5.
                    uint8_t pat = (cell & 0x1F) | ((cell & 0x40) >> 1);
                    fb_draw_char(framebuffer, col * 8, row * 10, (char)(0xA0 + pat), ABC_FG, ABC_BG);

                } else {
                    // No graphics flag — render as text (e.g. uppercase letters 0x40-0x5F)
                    char c = (char)(cell & 0x7F);
                    if (c < 0x20) c = ' ';
                    fb_draw_char(framebuffer, col * 8, row * 10, c, ABC_FG, ABC_BG);
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
    // WiFi is NOT auto-connected at boot — use FC in the monitor.
    // This avoids a 15 s blocking delay with no visible feedback.

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
            // Running one step per loop iteration gives ~10-15 MHz equivalent
            // on the RP2350 @ 150 MHz -- already well above the original 3 MHz.
            // For higher throughput, replace with a batch loop, e.g.:
            //   for (int i = 0; i < 500; i++) abc80_step();
            // This avoids the time_us_64() overhead on every single step.
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
