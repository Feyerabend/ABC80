/*
 * color.h  pixel / colour layer: RGB565 type, canvas dimensions, framebuffer helpers
 *
 * This is the bottom layer of the graphics stack.  Everything above it
 * (gfx.h, screen.h, dvi_out.h) depends on this file; this file depends on
 * nothing inside the project.
 *
 * What lives here
 * ---------------
 *   - Canvas dimensions (GFX_W, GFX_H)
 *   - RGB565 colour type and encode/decode macros
 *   - Named colour constants
 *   - Immediate-mode framebuffer helpers (clear, pixel, h/vline, rect, line)
 *
 * What does NOT live here
 * -----------------------
 *   - Fixed-point arithmetic (gfx.h - needed by the vector renderer)
 *   - Affine matrices, colour transforms, gradient fills (gfx.h)
 *   - Shape / scene graph (gfx.h)
 *   - CHIP-8 display mapping, phosphor palette (screen.h)
 *   - Hardware output (dvi_out.h)
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

/* ---- canvas dimensions -------------------------------------------------- */
#define GFX_W  320
#define GFX_H  240

/* ---- RGB565 colour ------------------------------------------------------ */
typedef uint16_t gfx_color_t;

/* Pack 8-bit R/G/B into RGB565.  Inputs are truncated, not clamped. */
#define GFX_RGB(r,g,b) ((gfx_color_t)((((r)&0xF8u)<<8)|(((g)&0xFCu)<<3)|((b)>>3)))

/* Unpack RGB565 back to 8-bit per channel (lower bits are zero-filled). */
#define GFX_R(c) ((uint8_t)(((c)>>8)&0xF8u))
#define GFX_G(c) ((uint8_t)(((c)>>3)&0xFCu))
#define GFX_B(c) ((uint8_t)(((c)<<3)&0xF8u))

/* Named colours */
#define GFX_BLACK   GFX_RGB(  0,  0,  0)
#define GFX_WHITE   GFX_RGB(255,255,255)
#define GFX_RED     GFX_RGB(255,  0,  0)
#define GFX_GREEN   GFX_RGB(  0,255,  0)
#define GFX_BLUE    GFX_RGB(  0,  0,255)
#define GFX_YELLOW  GFX_RGB(255,255,  0)
#define GFX_CYAN    GFX_RGB(  0,255,255)
#define GFX_MAGENTA GFX_RGB(255,  0,255)

/* ---- immediate-mode framebuffer helpers ---------------------------------- */
/* All coordinates are in pixels.  Out-of-bounds draws are clipped silently.  */
/* The framebuffer is GFX_W × GFX_H, row-major, one gfx_color_t per pixel.    */

void gfx_fb_clear (gfx_color_t *fb, gfx_color_t c);
void gfx_fb_pixel (gfx_color_t *fb, int x, int y, gfx_color_t c);
void gfx_fb_hline (gfx_color_t *fb, int x, int y, int w, gfx_color_t c);
void gfx_fb_vline (gfx_color_t *fb, int x, int y, int h, gfx_color_t c);
void gfx_fb_rect  (gfx_color_t *fb, int x, int y, int w, int h, gfx_color_t c);
void gfx_fb_line  (gfx_color_t *fb, int x0, int y0, int x1, int y1, gfx_color_t c);
