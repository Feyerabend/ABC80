#pragma once
#include <stdint.h>
#include <stdbool.h>

// ---- Timing constants -------------------------------------------------------
//
// NTSC 6560: crystal 14.318 MHz, CPU = /14 ≈ 1.0227 MHz
//   65 CPU clocks/line × 261 lines × 60.05 Hz
//
// PAL  6561: crystal 4.433619 MHz × 4 = 17.734 MHz, CPU ≈ 1.108 MHz
//   71 CPU clocks/line × 312 lines × 50.12 Hz

#ifdef VIC_PAL
#  define VIC_CYCLES_PER_LINE  71
#  define VIC_LINES_PER_FRAME  312
#else
#  define VIC_CYCLES_PER_LINE  65
#  define VIC_LINES_PER_FRAME  261
#endif

// First and last active scanline (PAL/NTSC differ slightly but this is a good
// default for the 22×23 default text screen with standard borders).
#define VIC_FIRST_ACTIVE_LINE   28
#define VIC_LAST_ACTIVE_LINE    207   // 28 + 23 rows × 8 px - 1 = 211; add a bottom border

// ---- VIC chip register offsets (base $9000) ---------------------------------
#define VIC_REG_INTERLACE  0x00   // [7]=interlace, [6:0]=raster MSB / origin-X
#define VIC_REG_ORIGIN_Y   0x01
#define VIC_REG_VIDEO_PTR  0x02   // [7]=8th col bit, [3:0]=video matrix high
#define VIC_REG_ROWS       0x03   // [0]=char height (0=8px,1=16px), [6:1]=row count, [7]=raster LSB
#define VIC_REG_RASTER_L   0x04   // read: current raster low byte
#define VIC_REG_RASTER_H   0x05   // read: current raster high byte (bit 0 only)
#define VIC_REG_LPEN_X     0x06
#define VIC_REG_LPEN_Y     0x07
#define VIC_REG_PADDLE_X   0x08
#define VIC_REG_PADDLE_Y   0x09
#define VIC_REG_VOICE1     0x0A   // [7]=enable, [6:0]=frequency
#define VIC_REG_VOICE2     0x0B
#define VIC_REG_VOICE3     0x0C
#define VIC_REG_NOISE      0x0D
#define VIC_REG_VOLUME     0x0E   // [3:0]=volume, [7:4]=aux/border colour
#define VIC_REG_COLOURS    0x0F   // [3:0]=background colour, [7:4]=border colour

// ---- Public API -------------------------------------------------------------

void    vic_reset(void);

// Called by the main emulation loop after each step6502() call.
// Advances internal raster state; returns true when a new frame has started.
bool    vic_tick(uint32_t cpu_cycles);

uint8_t vic_read(uint16_t addr);
void    vic_write(uint16_t addr, uint8_t val);

uint16_t vic_get_scanline(void);
uint32_t vic_get_frame(void);
bool     vic_in_vblank(void);

// Render one DVI buffer line (y = 0-239) into linebuf (320 RGB565 pixels).
void vic_render_dvi_line(int y, uint16_t *linebuf);
