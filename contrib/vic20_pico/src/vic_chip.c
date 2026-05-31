#include <string.h>
#include <stdint.h>
#include "vic_chip.h"
#include "dvi_output.h"
#include "memory.h"

// ---- Internal state ---------------------------------------------------------

static uint8_t  regs[16];
static uint16_t scanline;
static uint32_t frame;
static uint32_t cycle_remainder;

// ---- VIC-20 colour palette (RGB565) -----------------------------------------
//
// Based on VICE emulator palette, converted to RGB565.
// Colour index matches VIC-20 colour numbers 0-15.

static const uint16_t palette[16] = {
    0x0000, // 0  Black       #000000
    0xFFFF, // 1  White       #FFFFFF
    0x8980, // 2  Red         #883300
    0xAFFD, // 3  Cyan        #AAFFEE
    0xCA39, // 4  Purple      #CC44CC
    0x066A, // 5  Green       #00CC55
    0x0015, // 6  Blue        #0000AA
    0xEF6E, // 7  Yellow      #EEEE77
    0xDC4A, // 8  Orange      #DD8855
    0x6220, // 9  Brown       #664400
    0xFBAE, // 10 Light Red   #FF7777
    0x3186, // 11 Dark Grey   #333333
    0x73AE, // 12 Mid Grey    #777777
    0xAFEC, // 13 Light Green #AAFF66
    0x045F, // 14 Light Blue  #0088FF
    0xBDD7, // 15 Light Grey  #BBBBBB
};

// ---- Reset ------------------------------------------------------------------

void vic_reset(void) {
    memset(regs, 0, sizeof(regs));
    scanline        = 0;
    frame           = 0;
    cycle_remainder = 0;

    // Power-on defaults that match VIC-20 Kernal expectations.
    regs[VIC_REG_INTERLACE] = 0x0C;  // origin-X = 12 half-clocks
    regs[VIC_REG_ORIGIN_Y]  = 0x26;  // origin-Y = 38 lines
    regs[VIC_REG_VIDEO_PTR] = 0xF0;  // screen RAM → $1E00, char → $8000
    regs[VIC_REG_ROWS]      = 0x16;  // 23 rows, 8-pixel chars
    regs[VIC_REG_VOLUME]    = 0x00;
    regs[VIC_REG_COLOURS]   = 0x1B;  // bits[7:4]=1→white bg, ~1=14→light blue border
}

// ---- Timing -----------------------------------------------------------------

bool vic_tick(uint32_t cpu_cycles) {
    bool new_frame = false;

    cycle_remainder += cpu_cycles;

    while (cycle_remainder >= VIC_CYCLES_PER_LINE) {
        cycle_remainder -= VIC_CYCLES_PER_LINE;
        scanline++;
        if (scanline >= VIC_LINES_PER_FRAME) {
            scanline = 0;
            frame++;
            new_frame = true;
        }
    }

    // Keep raster counter registers readable by software.
    regs[VIC_REG_RASTER_L] = (uint8_t)(scanline & 0xFF);
    regs[VIC_REG_ROWS]     = (regs[VIC_REG_ROWS] & 0xFE) | ((scanline >> 8) & 0x01);

    return new_frame;
}

// ---- Register access --------------------------------------------------------

uint8_t vic_read(uint16_t addr) {
    return regs[addr & 0x0F];
}

void vic_write(uint16_t addr, uint8_t val) {
    uint8_t reg = addr & 0x0F;
    // Raster / light pen / paddle registers are read-only.
    // VIC_REG_RASTER_H (reg 5, $9005) is repurposed as a char-base selector
    // written by the running program, so it must be writable.
    if (reg == VIC_REG_RASTER_L ||
        reg == VIC_REG_LPEN_X   || reg == VIC_REG_LPEN_Y   ||
        reg == VIC_REG_PADDLE_X || reg == VIC_REG_PADDLE_Y)
        return;
    regs[reg] = val;
}

// ---- Status queries ---------------------------------------------------------

uint16_t vic_get_scanline(void) { return scanline; }
uint32_t vic_get_frame(void)    { return frame; }
bool     vic_in_vblank(void)    { return scanline >= VIC_LAST_ACTIVE_LINE; }

// ---- Rendering --------------------------------------------------------------
//
// vic_render_dvi_line(y, linebuf):
//   y       : buffer line 0-239 (each displayed twice by DVI_VERTICAL_REPEAT=2)
//   linebuf : 320 RGB565 pixels
//
// Horizontal layout (1:1 pixel mapping — each VIC pixel = 1 fb pixel = 2 HDMI pixels):
//   px   0-71   left border  (72 px)
//   px  72-247  VIC text     (176 px = 22 cols × 8 px)
//   px 248-319  right border (72 px)

void vic_render_dvi_line(int y, uint16_t *linebuf) {
    // R15: bits[7:4] = background colour (direct); border = bitwise complement of same nibble.
    const uint16_t bg_c     = palette[(regs[VIC_REG_COLOURS] >> 4) & 0x0F];
    const uint16_t border_c = palette[((uint8_t)(~regs[VIC_REG_COLOURS]) >> 4) & 0x0F];

    if (y < VIC_DVI_Y_START || y >= VIC_DVI_Y_END) {
        for (int i = 0; i < DVI_BUF_PIXELS; ++i)
            linebuf[i] = border_c;
        return;
    }

    const uint8_t *ram      = memory_raw();
    const uint8_t *char_rom = memory_char_rom();

    // R2 bit 7: screen at $1E00 if set, $1000 if clear (VICE formula).
    const uint16_t screen_base = (regs[VIC_REG_VIDEO_PTR] & 0x80) ? 0x1E00u : 0x1000u;

    int vic_row   = y - VIC_DVI_Y_START;
    int char_row  = vic_row / 8;
    int pixel_row = vic_row % 8;

    // Left border.
    for (int i = 0; i < VIC_DVI_X_START; ++i)
        linebuf[i] = border_c;

    // Char source: reg 5 ($9005) lower nibble selects the character base.
    // 0 = ROM charset (default); 1..15 = RAM at (nibble * $400).
    // This allows programs to switch to a custom charset in RAM by writing
    // the block number (e.g. 7 → $1C00) to $9005.
    const uint8_t char_sel = regs[VIC_REG_RASTER_H] & 0x0Fu;
    const uint8_t *char_src;
    uint32_t char_base;
    if (char_sel == 0u) {
        char_src  = char_rom;
        char_base = 0u;
    } else {
        char_src  = ram;
        char_base = (uint32_t)char_sel * 0x400u;
    }

    // Character cells — 1:1 pixel mapping: bit 7 → leftmost pixel.
    for (int col = 0; col < VIC_DVI_X_COLS; ++col) {
        uint16_t cell   = (uint16_t)(char_row * VIC_DVI_X_COLS + col);
        uint8_t  code   = ram[screen_base + cell];
        uint8_t  colour = ram[0x9400u + cell] & 0x0F;
        uint16_t fg_c   = palette[colour];

        uint8_t bits = char_src[char_base + (uint32_t)code * 8u + (uint32_t)pixel_row];

        int px = VIC_DVI_X_START + col * 8;
        for (int i = 0; i < 8; ++i)
            linebuf[px + i] = (bits >> (7 - i)) & 1u ? fg_c : bg_c;
    }

    // Right border.
    for (int i = VIC_DVI_X_START + VIC_DVI_X_COLS * 8; i < DVI_BUF_PIXELS; ++i)
        linebuf[i] = border_c;
}
