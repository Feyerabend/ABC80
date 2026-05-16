/*
 * dvi_out.h  double-buffered DVI output driver for Pico 2 (RP2350)
 *
 * Uses libdvi (PicoDVI, Wren6991) with the Pico DVI Sock pin configuration.
 *
 * Pin mapping - Pico DVI Sock (castellated pads, end of board):
 *   GPIO 12, 13 : TMDS lane 0 (blue + sync) D+/D-
 *   GPIO 14, 15 : TMDS clock CK+/CK-
 *   GPIO 16, 17 : TMDS lane 2 (red)   D+/D-
 *   GPIO 18, 19 : TMDS lane 1 (green) D+/D-
 *
 * Mode   :  640 x 480 p 60 Hz  (DVI / HDMI - no audio, no info-frame)
 * Canvas :  320 x 240 QVGA, each pixel doubled in both axes by libdvi
 *           DVI_VERTICAL_REPEAT  = 2 --> 240 scanlines x 2 = 480 active rows
 *           DVI_SYMBOLS_PER_WORD = 2 --> 320 pixels    x 2 = 640 active cols
 * Colour: RGB565, same format as the VGA build
 *
 * System clock requirement:
 *   TMDS bit rate for 640 x 480 @ 60 Hz = 252 Mbit/s per lane.
 *   dvi_out_init() raises sys_clock to 252 MHz before anything else starts.
 *   This must be the FIRST call in main().
 */

#pragma once
#include <stdint.h>
#include "color.h"    /* gfx_color_t (uint16_t RGB565), GFX_W, GFX_H */

/* Initialise libdvi, set sys_clock to 252 MHz, zero the framebuffers.
   Must be called from core 0 BEFORE launching core 1 or calling stdio_init. */
void dvi_out_init(void);

/* Entry point for core 1.  Call via multicore_launch_core1().
   Registers DMA IRQs, starts the TMDS serialiser, then runs
   dvi_scanbuf_main_16bpp() - never returns. */
void dvi_out_core1_main(void);

/* Return the back framebuffer (320 x 240 RGB565, row-major).
   Core 0 renders CHIP-8 pixels here while core 1 displays the front. */
gfx_color_t *dvi_out_back_buffer(void);

/* Atomically swap front and back framebuffer pointers.
   The new front is fed to the display starting with the next frame. */
void dvi_out_swap_buffers(void);

/* Feed one full frame (GFX_H = 240 scanlines) to libdvi from the front
   framebuffer and return.  queue_add_blocking_u32() stalls each iteration
   until libdvi has consumed the previous scanline, so this function takes
   exactly one frame period (~16.7 ms at 60 Hz) and serves as the vsync. */
void dvi_out_wait_vsync(void);

/* Push a single scanline (row y, 0-based) from the current front framebuffer
   into libdvi's colour queue.  Blocks if the queue is full.
   Call once per scanline in your interleaved CPU+DVI loop instead of
   dvi_out_wait_vsync() to avoid starving the TMDS queue. */
void dvi_out_push_row(int y);

/* Total frames fed to the display since init (incremented in wait_vsync). */
uint32_t dvi_out_frame_count(void);
