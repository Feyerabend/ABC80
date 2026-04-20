/*
 * display.c - VGA output via pico_scanvideo_dpi
 *
 * Targets the Pimoroni Pico VGA Demo Base (same hardware as RPi Pico VGA Demo Board).
 * Core 1 calls display_vga_init() then display_vga_run(&active_fb_ptr).
 * Core 0 renders into a back buffer and swaps the pointer for double-buffering.
 *
 * GPIO assignments (fixed by the VGA demo board resistor DAC):
 *   GPIO  0- 4  Blue [4:0]
 *   GPIO  5-10  Green[5:0]
 *   GPIO 11-15  Red  [4:0]
 *   GPIO 16     H-Sync
 *   GPIO 17     V-Sync
 */

#include "display.h"
#include "abcfont.h"
#include "abc80.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "hardware/sync.h"
#include "hardware/gpio.h"
#include <string.h>

/*
 * VGA initialisation and scanline loop
 */

display_error_t display_vga_init(void) {
    scanvideo_setup(&vga_mode_320x240_60);
    scanvideo_timing_enable(true);
    return DISPLAY_OK;
}

/*
 * display_vga_run - Core 1 scanline output loop (never returns).
 *
 * Reads from *active_fb_ptr each scanline so that Core 0 can swap the
 * pointer between frames without a lock (32-bit aligned pointer write/read
 * is atomic on RP2350 shared SRAM).
 *
 * Scanline buffer format for COMPOSABLE_RAW_RUN with DISPLAY_WIDTH pixels:
 *   word[0]      : COMPOSABLE_RAW_RUN (lo16) | pixel[0] (hi16)
 *   word[1]      : (DISPLAY_WIDTH-3) (lo16)  | pixel[1] (hi16)
 *   word[2..159] : pixel[2*i] (lo16) | pixel[2*i+1] (hi16)   (i=1..159)
 *   word[161]    : COMPOSABLE_EOL_ALIGN
 *   data_used    : 162
 */
void display_vga_run(uint16_t * volatile *active_fb_ptr) {
    while (true) {
        struct scanvideo_scanline_buffer *buf =
            scanvideo_begin_scanline_generation(true);

        int line = scanvideo_scanline_number(buf->scanline_id);
        const uint16_t *src = *active_fb_ptr + line * DISPLAY_WIDTH;

        uint32_t *data = buf->data;
        data[0] = COMPOSABLE_RAW_RUN | ((uint32_t)src[0] << 16u);
        data[1] = (uint32_t)(DISPLAY_WIDTH - 3) | ((uint32_t)src[1] << 16u);

        for (int i = 1; i < DISPLAY_WIDTH / 2; i++) {
            data[1 + i] = (uint32_t)src[2 * i] | ((uint32_t)src[2 * i + 1] << 16u);
        }

        data[DISPLAY_WIDTH / 2 + 1] = COMPOSABLE_EOL_ALIGN;
        buf->data_used = DISPLAY_WIDTH / 2 + 2;

        scanvideo_end_scanline_generation(buf);
    }
}

/*
 * Button reading — the three button GPIOs share pins with VGA color LSBs.
 * The PIO releases them during vblank, so we wait for vblank, briefly
 * reconfigure as inputs, sample, then let scanvideo reclaim them.
 */
uint8_t display_buttons_read(void) {
    /* Wait until we are in vblank — PIO stops actively driving these pins */
    while (!scanvideo_in_vblank()) tight_loop_contents();

    /* Switch from PIO0 to SIO so we can drive direction ourselves */
    const uint pins[3] = { BUTTON_A_PIN, BUTTON_B_PIN, BUTTON_C_PIN };
    for (int i = 0; i < 3; i++) {
        gpio_set_function(pins[i], GPIO_FUNC_SIO);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
    }

    /* Let pull-ups settle (~1 μs at 150 MHz) */
    for (volatile int d = 0; d < 200; d++) {}

    uint8_t mask = 0;
    for (int i = 0; i < 3; i++) {
        if (!gpio_get(pins[i]))   /* active-low */
            mask |= (1u << i);
    }

    /* Restore PIO0 function — scanvideo reclaims direction on next active line */
    for (int i = 0; i < 3; i++) {
        gpio_set_function(pins[i], GPIO_FUNC_PIO0);
    }

    return mask;
}

/*
 * Framebuffer rendering helpers
 */

static inline int clamp_i(int v, int lo, int hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

static inline uint8_t cx_apply_channel(uint8_t in, int mul, int add) {
    int v = ((int)in * mul >> 8) + add;
    if (v < 0)   return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

static inline void unpack565(uint16_t c, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = ((c >> 11) & 0x1F) << 3;
    *g = ((c >>  5) & 0x3F) << 2;
    *b =  (c        & 0x1F) << 3;
}

static inline uint16_t pack565(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint16_t)(r & 0xF8) << 8) |
           ((uint16_t)(g & 0xFC) << 3) |
           (b >> 3);
}

static inline void wu_plot_fb(uint16_t *fb, int x, int y, uint8_t alpha, uint16_t color) {
    if ((unsigned)x >= DISPLAY_WIDTH || (unsigned)y >= DISPLAY_HEIGHT) return;
    uint16_t *dst = &fb[y * DISPLAY_WIDTH + x];

    uint8_t sr, sg, sb, dr, dg, db;
    unpack565(color, &sr, &sg, &sb);
    unpack565(*dst,  &dr, &dg, &db);

    uint8_t ia = 255 - alpha;
    uint8_t r = (uint8_t)(((uint16_t)sr * alpha + (uint16_t)dr * ia) >> 8);
    uint8_t g = (uint8_t)(((uint16_t)sg * alpha + (uint16_t)dg * ia) >> 8);
    uint8_t b = (uint8_t)(((uint16_t)sb * alpha + (uint16_t)db * ia) >> 8);
    *dst = pack565(r, g, b);
}

void fb_clear(uint16_t *fb, uint16_t color) {
    int total = DISPLAY_WIDTH * DISPLAY_HEIGHT;
    for (int i = 0; i < total; i++) fb[i] = color;
}

void fb_draw_pixel(uint16_t *fb, int x, int y, uint16_t color) {
    if ((unsigned)x < DISPLAY_WIDTH && (unsigned)y < DISPLAY_HEIGHT)
        fb[y * DISPLAY_WIDTH + x] = color;
}

void fb_fill_rect(uint16_t *fb, int x, int y, int w, int h, uint16_t color) {
    int x1 = clamp_i(x,     0, DISPLAY_WIDTH);
    int y1 = clamp_i(y,     0, DISPLAY_HEIGHT);
    int x2 = clamp_i(x + w, 0, DISPLAY_WIDTH);
    int y2 = clamp_i(y + h, 0, DISPLAY_HEIGHT);
    for (int row = y1; row < y2; row++)
        for (int col = x1; col < x2; col++)
            fb[row * DISPLAY_WIDTH + col] = color;
}

/*
 * fb_draw_char - ABC80 character ROM renderer.
 * Cell size: FONT_CELL_W x FONT_CELL_H (8 x 10 px).
 * Handles mosaic/semigraphic chars 0xA0-0xDF as 2x3 block patterns.
 */
void fb_draw_char(uint16_t *fb, int x, int y, char c, uint16_t fg, uint16_t bg) {
    if ((unsigned)x >= DISPLAY_WIDTH || (unsigned)y >= DISPLAY_HEIGHT) return;

    unsigned char code = (unsigned char)c;

    /* Mosaic / semigraphic chars 0xA0-0xDF: full-cell 3+3+4 zone rendering */
    if (code >= 0xA0 && code <= 0xDF) {
        static const int rh[3] = {3, 3, 4};
        uint8_t pat = code - 0xA0;
        int cy = y;
        for (int r = 0; r < 3; r++) {
            int cx = x;
            for (int col = 0; col < 2; col++) {
                uint16_t color = (pat >> (r * 2 + col)) & 1 ? fg : bg;
                fb_fill_rect(fb, cx, cy, 4, rh[r], color);
                cx += 4;
            }
            cy += rh[r];
        }
        return;
    }

    /* Text characters: charRom glyph + descRom descender */
    const uint8_t *glyph = charRom[code];
    const uint8_t *desc  = descRom[code];
    for (int row = 0; row < FONT_CELL_H && (y + row) < (int)DISPLAY_HEIGHT; row++) {
        uint8_t bits = (row < FONT_GLYPH_H)          ? glyph[row]
                     : (row < FONT_GLYPH_H + 2)      ? desc[row - FONT_GLYPH_H]
                     :                                  0x00;
        for (int col = 0; col < 8 && (x + col) < (int)DISPLAY_WIDTH; col++) {
            fb[(y + row) * DISPLAY_WIDTH + (x + col)] =
                (bits & (0x80 >> col)) ? fg : bg;
        }
    }
}

void fb_draw_string(uint16_t *fb, int x, int y, const char *str, uint16_t fg, uint16_t bg) {
    while (*str && x < (int)DISPLAY_WIDTH) {
        fb_draw_char(fb, x, y, *str++, fg, bg);
        x += FONT_CELL_W;
    }
}

void fb_draw_line_aa(uint16_t *fb, float x0, float y0, float x1, float y1, uint16_t color) {
    bool steep = (y1 - y0 < 0 ? y0 - y1 : y1 - y0) >
                 (x1 - x0 < 0 ? x0 - x1 : x1 - x0);
    if (steep)   { float t; t=x0;x0=y0;y0=t; t=x1;x1=y1;y1=t; }
    if (x0 > x1) { float t; t=x0;x0=x1;x1=t; t=y0;y0=y1;y1=t; }

    float dx = x1 - x0, dy = y1 - y0;
    float grad = (dx == 0.0f) ? 1.0f : dy / dx;

    int   xe = (int)(x0 + 0.5f);
    float ye = y0 + grad * (xe - x0);
    float xg = 1.0f - (x0 + 0.5f - xe);
    int xp1 = xe, yp1 = (int)ye;
    float frac = ye - yp1;
    if (steep) {
        wu_plot_fb(fb, yp1,   xp1, (uint8_t)(255 * (1-frac) * xg), color);
        wu_plot_fb(fb, yp1+1, xp1, (uint8_t)(255 * frac     * xg), color);
    } else {
        wu_plot_fb(fb, xp1, yp1,   (uint8_t)(255 * (1-frac) * xg), color);
        wu_plot_fb(fb, xp1, yp1+1, (uint8_t)(255 * frac     * xg), color);
    }
    float intery = ye + grad;

    xe = (int)(x1 + 0.5f);
    ye = y1 + grad * (xe - x1);
    xg = x1 + 0.5f - (int)(x1 + 0.5f);
    int xp2 = xe, yp2 = (int)ye;
    frac = ye - yp2;
    if (steep) {
        wu_plot_fb(fb, yp2,   xp2, (uint8_t)(255 * (1-frac) * xg), color);
        wu_plot_fb(fb, yp2+1, xp2, (uint8_t)(255 * frac     * xg), color);
    } else {
        wu_plot_fb(fb, xp2, yp2,   (uint8_t)(255 * (1-frac) * xg), color);
        wu_plot_fb(fb, xp2, yp2+1, (uint8_t)(255 * frac     * xg), color);
    }

    for (int xi = xp1+1; xi < xp2; xi++) {
        int yi = (int)intery;
        uint8_t f = (uint8_t)(255 * (intery - yi));
        if (steep) {
            wu_plot_fb(fb, yi,   xi, 255-f, color);
            wu_plot_fb(fb, yi+1, xi, f,     color);
        } else {
            wu_plot_fb(fb, xi, yi,   255-f, color);
            wu_plot_fb(fb, xi, yi+1, f,     color);
        }
        intery += grad;
    }
}

void fb_blit_scaled(uint16_t *fb,
                    const uint16_t *src, int sw, int sh,
                    int dx, int dy, int dw, int dh)
{
    if (!src || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;

    int step_x = (sw << 16) / dw;
    int step_y = (sh << 16) / dh;

    for (int py = 0; py < dh; py++) {
        int oy = dy + py;
        if (oy < 0 || oy >= (int)DISPLAY_HEIGHT) continue;

        int sy16 = (int)(((long long)py * step_y));
        int sy   = sy16 >> 16;
        int fy   = (sy16 >> 8) & 0xFF;

        int sy0 = clamp_i(sy,   0, sh-1);
        int sy1 = clamp_i(sy+1, 0, sh-1);

        int sx16 = 0;
        for (int px = 0; px < dw; px++, sx16 += step_x) {
            int ox = dx + px;
            if (ox < 0 || ox >= (int)DISPLAY_WIDTH) continue;

            int sx  = sx16 >> 16;
            int fx  = (sx16 >> 8) & 0xFF;

            int sx0 = clamp_i(sx,   0, sw-1);
            int sx1 = clamp_i(sx+1, 0, sw-1);

            uint16_t c00 = src[sy0 * sw + sx0];
            uint16_t c10 = src[sy0 * sw + sx1];
            uint16_t c01 = src[sy1 * sw + sx0];
            uint16_t c11 = src[sy1 * sw + sx1];

            uint8_t r00,g00,b00, r10,g10,b10, r01,g01,b01, r11,g11,b11;
            unpack565(c00,&r00,&g00,&b00);
            unpack565(c10,&r10,&g10,&b10);
            unpack565(c01,&r01,&g01,&b01);
            unpack565(c11,&r11,&g11,&b11);

            int ifx = 255 - fx, ify = 255 - fy;
            int w00 = ifx * ify, w10 = fx * ify;
            int w01 = ifx * fy,  w11 = fx * fy;
            int total = w00 + w10 + w01 + w11;
            if (total == 0) total = 1;

            uint8_t r = (uint8_t)((w00*r00 + w10*r10 + w01*r01 + w11*r11) / total);
            uint8_t g = (uint8_t)((w00*g00 + w10*g10 + w01*g01 + w11*g11) / total);
            uint8_t b = (uint8_t)((w00*b00 + w10*b10 + w01*b01 + w11*b11) / total);

            fb[oy * DISPLAY_WIDTH + ox] = pack565(r, g, b);
        }
    }
}

void fb_apply_color_transform(uint16_t *fb, const fb_color_transform_t *cx) {
    fb_apply_color_transform_rect(fb, cx, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

void fb_apply_color_transform_rect(uint16_t *fb, const fb_color_transform_t *cx,
                                   int x, int y, int w, int h)
{
    if (!cx) return;
    int x1 = clamp_i(x,     0, (int)DISPLAY_WIDTH);
    int y1 = clamp_i(y,     0, (int)DISPLAY_HEIGHT);
    int x2 = clamp_i(x + w, 0, (int)DISPLAY_WIDTH);
    int y2 = clamp_i(y + h, 0, (int)DISPLAY_HEIGHT);

    if (cx->r_mul == 256 && cx->r_add == 0 &&
        cx->g_mul == 256 && cx->g_add == 0 &&
        cx->b_mul == 256 && cx->b_add == 0) return;

    for (int row = y1; row < y2; row++) {
        uint16_t *line = &fb[row * DISPLAY_WIDTH + x1];
        for (int col = x1; col < x2; col++, line++) {
            uint8_t r, g, b;
            unpack565(*line, &r, &g, &b);
            r = cx_apply_channel(r, cx->r_mul, cx->r_add);
            g = cx_apply_channel(g, cx->g_mul, cx->g_add);
            b = cx_apply_channel(b, cx->b_mul, cx->b_add);
            *line = pack565(r, g, b);
        }
    }
}

/*
 * Mosaic dot API — operates on ABC80 screen RAM via abc80_screen_char/write.
 */

#define MOSAIC_CELL_COLS  (DISPLAY_WIDTH  / FONT_CELL_W)   /* 40 */
#define MOSAIC_CELL_ROWS  (DISPLAY_HEIGHT / FONT_CELL_H)   /* 24 */

static uint8_t mosaic_buf[MOSAIC_CELL_COLS * MOSAIC_CELL_ROWS];

void mosaic_clear(void) {
    memset(mosaic_buf, 0, sizeof(mosaic_buf));
}

/* ABC80 graphics encoding (MAME abc80_v.cpp):
 *   bit0=TL  bit1=TR  bit2=ML  bit3=MR  bit4=BL  bit6=BR  (bit5 unused for dots)
 * Font index:
 *   bit0=TL  bit1=TR  bit2=ML  bit3=MR  bit4=BL  bit5=BR
 * Index = (dot_y % 3) * 2 + (dot_x & 1) */
static const uint8_t dmask_tab[6] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x40 };

void setdot(int dot_x, int dot_y) {
    if ((unsigned)dot_x >= MOSAIC_DOT_COLS || (unsigned)dot_y >= MOSAIC_DOT_ROWS) return;

    int char_row = dot_y / 3;
    int char_col = dot_x / 2;
    uint8_t mask = dmask_tab[(dot_y % 3) * 2 + (dot_x & 1)];

    uint8_t cell = abc80_screen_char(char_row, char_col);
    cell |= mask | 0x20;
    abc80_screen_write(char_row, char_col, cell);

    if (char_col > 0 && (abc80_screen_char(char_row, 0) & 0x7F) != 0x17)
        abc80_screen_write(char_row, 0, 0x97);
}

void clrdot(int dot_x, int dot_y) {
    if ((unsigned)dot_x >= MOSAIC_DOT_COLS || (unsigned)dot_y >= MOSAIC_DOT_ROWS) return;

    int char_row = dot_y / 3;
    int char_col = dot_x / 2;
    uint8_t mask = dmask_tab[(dot_y % 3) * 2 + (dot_x & 1)];

    uint8_t cell = abc80_screen_char(char_row, char_col);
    cell = (cell & ~mask) | 0x20;
    abc80_screen_write(char_row, char_col, cell);
}

void mosaic_render(uint16_t *fb, uint16_t fg, uint16_t bg) {
    for (int row = 0; row < MOSAIC_CELL_ROWS; row++) {
        for (int col = 0; col < MOSAIC_CELL_COLS; col++) {
            uint8_t pat = mosaic_buf[row * MOSAIC_CELL_COLS + col];
            fb_draw_char(fb, col * FONT_CELL_W, row * FONT_CELL_H,
                         (char)(0xA0 + pat), fg, bg);
        }
    }
}
