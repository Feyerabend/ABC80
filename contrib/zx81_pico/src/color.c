/*
 * color.c  immediate-mode framebuffer drawing primitives
 *
 * These are the only drawing functions that touch individual pixels.  All
 * higher-level rendering (vector shapes in gfx.c, CHIP-8 display mapping in
 * screen.c) is built on top of these.
 *
 * Performance notes
 * -----------------
 * gfx_fb_clear and gfx_fb_hline write pixels in 32-bit pairs wherever
 * alignment allows.  On RP2350 this halves the number of bus transactions
 * to SRAM, which matters because these are called for every CHIP-8 pixel
 * on every frame that has draw_pending set.
 */

#include "color.h"
#include <stdint.h>
#include <stdlib.h>   /* abs() */

void gfx_fb_clear(gfx_color_t *fb, gfx_color_t c)
{
    uint32_t v32 = ((uint32_t)c << 16) | c;
    uint32_t *p = (uint32_t *)fb;
    for (int i = 0; i < GFX_W * GFX_H / 2; i++) p[i] = v32;
}

void gfx_fb_pixel(gfx_color_t *fb, int x, int y, gfx_color_t c)
{
    if ((unsigned)x < GFX_W && (unsigned)y < GFX_H)
        fb[y * GFX_W + x] = c;
}

void gfx_fb_hline(gfx_color_t *fb, int x, int y, int w, gfx_color_t c)
{
    if ((unsigned)y >= GFX_H) return;
    if (x < 0) { w += x; x = 0; }
    if (x + w > GFX_W) w = GFX_W - x;
    if (w <= 0) return;

    gfx_color_t *p = fb + y * GFX_W + x;
    /* step 1: align to 32-bit boundary */
    if (((uintptr_t)p & 2) && w > 0) { *p++ = c; w--; }
    /* step 2: bulk 32-bit writes (2 pixels per store) */
    uint32_t v32 = ((uint32_t)c << 16) | c;
    uint32_t *wp = (uint32_t *)p;
    for (; w >= 2; w -= 2) *wp++ = v32;
    /* step 3: trailing odd pixel */
    if (w) *(gfx_color_t *)wp = c;
}

void gfx_fb_vline(gfx_color_t *fb, int x, int y, int h, gfx_color_t c)
{
    if ((unsigned)x >= GFX_W) return;
    if (y < 0) { h += y; y = 0; }
    if (y + h > GFX_H) h = GFX_H - y;
    for (int i = 0; i < h; i++) fb[(y + i) * GFX_W + x] = c;
}

void gfx_fb_rect(gfx_color_t *fb, int x, int y, int w, int h, gfx_color_t c)
{
    for (int dy = 0; dy < h; dy++)
        gfx_fb_hline(fb, x, y + dy, w, c);
}

void gfx_fb_line(gfx_color_t *fb, int x0, int y0, int x1, int y1, gfx_color_t c)
{
    /* Bresenham integer line */
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    while (1) {
        gfx_fb_pixel(fb, x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}
