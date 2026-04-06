#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"

// Display Pack 2.0 specifications: 320x240 pixels
#define DISPLAY_WIDTH 320
#define DISPLAY_HEIGHT 240

// Colors (RGB565 format)
#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_YELLOW    0xFFE0
#define COLOR_CYAN      0x07FF
#define COLOR_MAGENTA   0xF81F

// Button definitions
typedef enum {
    BUTTON_A = 0,
    BUTTON_B = 1,
    BUTTON_X = 2,
    BUTTON_Y = 3,
    BUTTON_COUNT = 4  /* sentinel - used for array sizing and bounds checks */
} button_t;

// Button callback function type
typedef void (*button_callback_t)(button_t button);

// Error codes
typedef enum {
    DISPLAY_OK = 0,
    DISPLAY_ERROR_INIT_FAILED,
    DISPLAY_ERROR_DMA_FAILED,
    DISPLAY_ERROR_INVALID_PARAM,
    DISPLAY_ERROR_NOT_INITIALIZED
} display_error_t;

// Display functions
display_error_t display_pack_init(void);
display_error_t display_clear(uint16_t color);
display_error_t display_fill_rect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
display_error_t display_draw_pixel(uint16_t x, uint16_t y, uint16_t color);
display_error_t display_blit_full(const uint16_t *pixels);
display_error_t display_draw_char(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg_color);
display_error_t display_draw_string(uint16_t x, uint16_t y, const char* str, uint16_t color, uint16_t bg_color);
display_error_t display_set_backlight(bool on);

// Button functions
display_error_t buttons_init(void);
void buttons_update(void);
bool button_pressed(button_t button);
bool button_just_pressed(button_t button);
bool button_just_released(button_t button);
display_error_t button_set_callback(button_t button, button_callback_t callback);

// Utility functions
bool display_is_initialized(void);
bool display_dma_busy(void);
void display_wait_for_dma(void);
void display_cleanup(void);
const char* display_error_string(display_error_t error);

// ---------------------------------------------------------------------------
// Framebuffer rendering API (draw into CPU-side buffer, blit with
// display_blit_full once per frame for flicker-free output)
// ---------------------------------------------------------------------------

/* Framebuffer pixel helpers: RGB565 pack / unpack */

/* Pack 8-bit RGB into a single RGB565 word (top bits of each channel used) */
static inline uint16_t fb_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint16_t)(r & 0xF8) << 8) |
           ((uint16_t)(g & 0xFC) << 3) |
           (b >> 3);
}

/*
 * Unpack RGB565 - raw channel fields (NOT full 8-bit values):
 *   r  0-31  (5 bits)
 *   g  0-63  (6 bits)
 *   b  0-31  (5 bits)
 *
 * For 8-bit (0-255) values, use the internal unpack565() in display.c
 * (which left-shifts each field to fill the byte).
 */
static inline void fb_unpack(uint16_t c, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = (c >> 11) & 0x1F;
    *g = (c >>  5) & 0x3F;
    *b =  c        & 0x1F;
}

// Framebuffer primitives - all operate on a caller-supplied pixel array
// of size DISPLAY_WIDTH * DISPLAY_HEIGHT.
void fb_clear(uint16_t *fb, uint16_t color);
void fb_draw_pixel(uint16_t *fb, int x, int y, uint16_t color);
void fb_fill_rect(uint16_t *fb, int x, int y, int w, int h, uint16_t color);
void fb_draw_char(uint16_t *fb, int x, int y, char c, uint16_t fg, uint16_t bg);
void fb_draw_string(uint16_t *fb, int x, int y, const char *str, uint16_t fg, uint16_t bg);

// Xiaolin Wu anti-aliased line (adapted from Flash renderer edge walker)
void fb_draw_line_aa(uint16_t *fb, float x0, float y0, float x1, float y1, uint16_t color);

// Bilinear-filtered blit - port of Flash Bitmap.GetSSRGBPixel logic.
// Scales src (sw x sh, RGB565) into dst region (dx,dy,dw,dh) using
// bilinear interpolation for smooth up/down scaling.
void fb_blit_scaled(uint16_t *fb,
                    const uint16_t *src, int sw, int sh,
                    int dx, int dy, int dw, int dh);

// ---------------------------------------------------------------------------
// Color transform - ported from Flash ColorTransform.
//   result_channel = clamp(src_channel * mul/256 + add)
// Use mul=256, add=0 for identity. mul=0..255 dims, add>0 tints.
// ---------------------------------------------------------------------------
typedef struct {
    int r_mul, r_add;   // red   channel:  out = clamp(in * r_mul/256 + r_add)
    int g_mul, g_add;   // green
    int b_mul, b_add;   // blue
} fb_color_transform_t;

// Apply colour transform to every pixel of the framebuffer
void fb_apply_color_transform(uint16_t *fb, const fb_color_transform_t *cx);

// Apply colour transform to a rectangular region only
void fb_apply_color_transform_rect(uint16_t *fb, const fb_color_transform_t *cx,
                                   int x, int y, int w, int h);

// ---------------------------------------------------------------------------
// Mosaic dot API
//
// Each 8x10 cell holds a 2x3 block of addressable "dots":
//
//   col->     0        1
//   row  +--------+--------+
//    0   | bit 0  | bit 1  |  (top zone,    glyph rows 0-1)
//        +--------+--------+
//    1   | bit 2  | bit 3  |  (middle zone, glyph rows 2-3)
//        +--------+--------+
//    2   | bit 4  | bit 5  |  (bottom zone, glyph rows 4-6)
//        +--------+--------+
//
// Addressable dot grid:
//   x : 0 .. MOSAIC_DOT_COLS-1  =  80  (40 cells x 2)
//   y : 0 .. MOSAIC_DOT_ROWS-1  =  72  (24 cells x 3)
//
// setdot / clrdot update an internal cell buffer.
// Call mosaic_render() to push the whole buffer into a framebuffer.
// ---------------------------------------------------------------------------
#define MOSAIC_DOT_COLS  80   /* (DISPLAY_WIDTH  / FONT_CELL_W) * 2 */
#define MOSAIC_DOT_ROWS  72   /* (DISPLAY_HEIGHT / FONT_CELL_H) * 3 */

void mosaic_clear(void);
// DEAD CODE: setdot/clrdot write to mosaic_buf[], not to Z80 screen RAM.
// screen_refresh() reads screen RAM directly, so these have no visible effect.
// void setdot(int x, int y);
// void clrdot(int x, int y);
void mosaic_render(uint16_t *fb, uint16_t fg, uint16_t bg);

// Convenient preset transforms
static inline fb_color_transform_t fb_cx_identity(void) {
    return (fb_color_transform_t){256,0, 256,0, 256,0};
}
static inline fb_color_transform_t fb_cx_dim(uint8_t level) {   // level 0=black 255=full
    return (fb_color_transform_t){level,0, level,0, level,0};
}
static inline fb_color_transform_t fb_cx_tint(uint8_t r, uint8_t g, uint8_t b) {
    return (fb_color_transform_t){256,r, 256,g, 256,b};
}

#endif // DISPLAY_H
