/*
 * zx81_screen.h  ZX81 display file renderer
 *
 * The ZX81 display is 32 columns × 24 rows of 8×8 character cells.
 * Native resolution: 256×192 pixels, centred in the 320×240 canvas.
 *
 * Canvas mapping
 * --------------
 *   x offset = (320 - 256) / 2 = 32 px   (left/right border)
 *   y offset = (240 - 192) / 2 = 24 px   (top/bottom border)
 *
 * Character ROM
 * -------------
 *   64 ZX81 characters, 8 bytes each, stored in ROM at 0x1E00.
 *   Each byte = one pixel row, MSB = leftmost pixel.
 *   Bit 7 of a display-file byte = inverse video.
 *   Bits 5–0 = character index (0–63).
 *
 * Display file format
 * -------------------
 *   D_FILE system variable (at 0x400C, 2 bytes, little-endian) points to
 *   the start of the display file in RAM.  Layout:
 *     [0x76]              ← 1-byte preamble (HALT/newline marker)
 *     [row 0: 0–32 chars + 0x76]
 *     [row 1: 0–32 chars + 0x76]
 *     ...
 *     [row 23: 0–32 chars + 0x76]
 *   Rows are variable-length: a short row is right-padded with PAPER by
 *   the renderer.
 */

#pragma once
#include "color.h"

#define ZX81_COLS        32
#define ZX81_ROWS        24
#define ZX81_CHAR_W       8
#define ZX81_CHAR_H       8

#define ZX81_DISP_W      (ZX81_COLS * ZX81_CHAR_W)          /* 256 */
#define ZX81_DISP_H      (ZX81_ROWS * ZX81_CHAR_H)          /* 192 */
#define ZX81_X_OFFSET    ((GFX_W - ZX81_DISP_W) / 2)        /*  32 */
#define ZX81_Y_OFFSET    ((GFX_H - ZX81_DISP_H) / 2)        /*  24 */

/* ROM address of the 64-character bitmap table (8 bytes per character).
 * The ZX81 I register is initialised to 0x1E by the ROM, placing the
 * character set at 0x1E00 in the address space. */
#define ZX81_CHARSET_ADDR  0x1E00u

/* RAM address of the D_FILE system variable (2-byte pointer, little-endian).
 * ZX81 system variable table (IY = 0x4000): D_FILE at 0x400C (confirmed from
 * ROM listing: "LD HL,($400C) ; fetch start of Display File from D_FILE").
 * Rendering starts at D_FILE + 1 to skip the single leading HALT byte. */
#define ZX81_D_FILE_ADDR   0x400Cu

/* Full-frame render (calls prepare + all render_line). */
void zx81_screen_render(gfx_color_t *fb);

/* Split API for incremental rendering inside the DVI feed loop.
 *
 * Call zx81_screen_prepare() once per frame in the inter-frame gap
 * (before swap_buffers).  It scans D_FILE and caches row pointers.
 *
 * Call zx81_screen_render_line(fb, y) for every DVI scanline y (0..GFX_H-1)
 * inside the push_row loop.  It renders one pixel row into the back buffer.
 * Since render_line writes to the BACK buffer and push_row reads from the
 * FRONT buffer they are independent — no conflict. */
void zx81_screen_prepare(void);
void zx81_screen_render_line(gfx_color_t *fb, int y);

/* OSD (on-screen display) keyboard help overlay.
 * Cycles through: ZX81 display → K-mode reference → Symbol map → ZX81. */
void zx81_osd_cycle(void);
