#pragma once
#include <stdint.h>

#define DVI_BUF_LINES   240
#define DVI_BUF_PIXELS  320   // 640x480 mode: 640/2 = 320 (libdvi doubles each pixel)

// VIC-20 layout within the 320x240 framebuffer (in pixels)
// 1:1 pixel mapping: each VIC pixel → 1 fb pixel → 2 physical HDMI pixels (libdvi doubles).
// 22 columns × 8 px = 176 px wide, centred in 320: 72 px borders each side.
// 23 rows × 8 px = 184 px tall, centred in 240: 28 px top/bottom borders.
#define VIC_DVI_Y_START  28
#define VIC_DVI_Y_END   212   // 28 + 23 rows * 8 px
#define VIC_DVI_X_START  72   // left border (centres 176 px text in 320 px)
#define VIC_DVI_X_COLS   22   // text columns
#define VIC_DVI_X_PX_PER_COL 8   // framebuffer pixels per column (1:1 scale)
#define VIC_DVI_ROWS     23

// Raises DVDD to 1.2V, sets sys_clock to 252MHz, zeroes framebuffers.
// Must be the first call in main(), before stdio_init_all().
void      dvi_output_init(void);

// Core 1 entry point. Registers DMA IRQs, starts TMDS serialiser,
// then runs dvi_scanbuf_main_16bpp(). Never returns.
void      dvi_output_core1_main(void);

// Back framebuffer: 320x240 RGB565 pixels, row-major.
uint16_t *dvi_output_back_buf(void);

// Atomically swap front/back framebuffer pointers.
void      dvi_output_swap(void);

// Push one scanline (row y, 0-239) from the front buffer to the DVI encoder.
// Blocks briefly (~63 µs per call at 60 Hz) when the TMDS queue is full.
// Calling this for all 240 rows totals ~16.7ms = one frame = software vsync.
//
// RED-SCANLINE NOTE — root cause and cure
// ----------------------------------------
// The libdvi IRQ outputs a solid red scanline whenever q_tmds_valid is empty
// (see dvi.c line: "No valid scanline was ready (generates solid red scanline)").
// This happens if Core 0's main loop is slower than the DVI 60 Hz clock and
// the TMDS queue drains between vsync calls.
//
// WRONG approach (batch):
//   render all 240 lines → vsync (push all 240 at once) → run CPU
//   Total ≈ 15ms vsync + 4ms CPU/render = 19ms > 16.7ms → TMDS gap → RED.
//
// RIGHT approach (interleaved, used below):
//   for each DVI scanline y:
//     run CPU for (frame_cycles / 240) cycles
//     dvi_output_push_row(y)   ← blocks at the DVI rate (~63 µs)
//     render back_buf row y
//   The push_row block naturally paces the loop to exactly 60 Hz.
//   CPU + render per line ≈ 6 µs, well within the 63 µs DVI slot.
void      dvi_output_push_row(int y);
