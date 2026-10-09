/*
 * zx81_screen.c  ZX81 display file → RGB565 framebuffer renderer
 *
 * Called once per frame during vblank (core 0, after DVI scanlines are fed).
 * Reads the display file from emulated RAM, maps each character through the
 * ROM bitmap table, and writes pixels into the back framebuffer.
 *
 * No cycle-accurate ULA emulation: we render the display file directly.
 * This is fast enough for 60 Hz and correct for all standard ZX81 software.
 */

#include "zx81_screen.h"
#include "z80_api.h"
#include <stddef.h>

/* ZX81 is black & white.  Paper and border are white; ink is black. */
#define COL_PAPER   GFX_WHITE
#define COL_INK     GFX_BLACK
#define COL_BORDER  GFX_BLACK

/* ---- OSD colours -------------------------------------------------------- */
#define OSD1_BG   GFX_RGB(  0,  0,128)   /* dark blue  – K-mode page  */
#define OSD2_BG   GFX_RGB(  0, 80,  0)   /* dark green – symbol page  */
#define OSD_TEXT  GFX_YELLOW
#define OSD_DIM   GFX_WHITE

/* ---- OSD ---------------------------------------------------------------- */

static int osd_state = 0;   /* 0=ZX81, 1=K-mode ref, 2=symbol map */

void zx81_osd_cycle(void)
{
    osd_state = (osd_state + 1) % 3;
}

/* ASCII → ZX81 character index (same encoding as the ROM bitmap table) */
static uint8_t ascii_to_zx(char c)
{
    if (c >= 'A' && c <= 'Z') return (uint8_t)(0x26 + (c - 'A'));
    if (c >= 'a' && c <= 'z') return (uint8_t)(0x26 + (c - 'a'));
    if (c >= '0' && c <= '9') return (uint8_t)(0x1C + (c - '0'));
    switch (c) {
        case '"': return 0x0B;
        case ':': return 0x0E;
        case '?': return 0x0F;
        case '(': return 0x10;
        case ')': return 0x11;
        case '>': return 0x12;
        case '<': return 0x13;
        case '=': return 0x14;
        case '+': return 0x15;
        case '-': return 0x16;
        case '*': return 0x17;
        case '/': return 0x18;
        case ';': return 0x19;
        case ',': return 0x1A;
        case '.': return 0x1B;
        default:  return 0x00;
    }
}

/* OSD page content – NULL-terminated, max 40 chars per string */

static const char * const osd_kmode[] = {
    " ZX81 K MODE [1/2]   Ctrl+O=NEXT",
    "",
    " KEY  UNSHIFTED   SHIFTED",
    "  E   NEXT",
    "  F   FOR         FAST",
    "  G   GOTO        GOSUB",
    "  I   IF",
    "  L   LET",
    "  N   NEW",
    "  P   PRINT",
    "  R   RUN         RETURN",
    "  S   SCROLL      SLOW",
    "",
    " SHIFT+DIGIT:",
    "  1=EDIT 2=CAPS 5=< 6=v 7=^ 8=>",
    "  9=(   0=)   BS=DELETE",
    "",
    " CURSOR: %=left ^=down &=up ->8",
    " K cursor: keyword  L cursor: letter",
    NULL
};

static const char * const osd_symbols[] = {
    " ZX81 SYMBOLS [2/2]  Ctrl+O=CLOSE",
    "",
    " TYPE -> COMBO     TYPE -> COMBO",
    "  \"  -> SHIFT+P    (  -> SHIFT+9",
    "  )  -> SHIFT+0    ,  -> SHIFT+.",
    "  -  -> SHIFT+J    +  -> SHIFT+K",
    "  =  -> SHIFT+L    ;  -> SHIFT+X",
    "  :  -> SHIFT+Z    /  -> SHIFT+V",
    "  <  -> SHIFT+R    >  -> SHIFT+T",
    "  *  -> SHIFT+M",
    "",
    " ARROW KEYS or use %^&",
    " BS/DEL -> DELETE (SHIFT+0)",
    "",
    " Ctrl+D -> DISPLAY DUMP",
    " Ctrl+R -> LAUNCH AIRFIGHT",
    " Ctrl+L -> LOAD .P FILE",
    " Ctrl+O -> CYCLE / CLOSE OSD",
    NULL
};

static void render_osd_line(gfx_color_t *fb, int y)
{
    gfx_color_t bg   = (osd_state == 1) ? OSD1_BG : OSD2_BG;
    const char * const *page = (osd_state == 1) ? osd_kmode : osd_symbols;

    int char_row = y / ZX81_CHAR_H;
    int pixel_y  = y % ZX81_CHAR_H;

    /* Find the content string for this character row */
    int r = 0;
    while (r < char_row && page[r] != NULL) r++;
    const char *line = (r == char_row && page[r] != NULL) ? page[r] : NULL;

    /* Background row */
    gfx_fb_hline(fb, 0, y, GFX_W, bg);

    if (!line) return;

    /* Render each character using the ZX81 ROM bitmap */
    for (int col = 0; col < GFX_W / ZX81_CHAR_W && line[col] != '\0'; col++) {
        uint8_t  ch     = ascii_to_zx(line[col]);
        uint16_t bmp    = ZX81_CHARSET_ADDR + (uint16_t)ch * ZX81_CHAR_H;
        uint8_t  pixels = m[bmp + (uint16_t)pixel_y];

        gfx_color_t *out = fb + (uint32_t)y * GFX_W + col * ZX81_CHAR_W;
        for (int cx = 0; cx < ZX81_CHAR_W; cx++) {
            bool ink = (pixels >> (7 - cx)) & 1u;
            out[cx] = ink ? OSD_TEXT : bg;
        }
    }
}

/* ---- Incremental render state ------------------------------------------- */

static bool     s_valid;
static uint16_t s_row_ptr[ZX81_ROWS];  /* D_FILE pointer for each char row */

/* Scan the D_FILE once and cache a pointer to the start of each character
 * row.  Call this in the inter-frame gap (before swap_buffers) so the scan
 * reflects the display file state at the end of the last CPU frame. */
void zx81_screen_prepare(void)
{
    if (osd_state != 0) {
        s_valid = false;
        return;
    }

    uint16_t d_file = (uint16_t)m[ZX81_D_FILE_ADDR] |
                      ((uint16_t)m[ZX81_D_FILE_ADDR + 1] << 8);

    if (d_file < 0x4000u || d_file > 0xBE00u) {
        s_valid = false;
        return;
    }
    s_valid = true;

    uint16_t ptr = d_file + 1;   /* skip the single leading HALT preamble */

    for (int row = 0; row < ZX81_ROWS; row++) {
        s_row_ptr[row] = ptr;

        /* Advance ptr past this row: scan for the row-terminating HALT. */
        int col;
        for (col = 0; col < ZX81_COLS; col++) {
            if (m[ptr + (uint16_t)col] == 0x76)
                break;
        }
        if (col < ZX81_COLS) {
            ptr += (uint16_t)(col + 1);   /* short row: skip chars + HALT */
        } else {
            ptr += ZX81_COLS;
            while (ptr < 0x7FFFu && m[ptr] != 0x76)
                ptr++;
            ptr++;                         /* full row: scan forward to HALT */
        }
    }
}

/* Render one DVI scanline y (0..GFX_H-1) into the back framebuffer.
 * Must be called after zx81_screen_prepare() has been called for this frame.
 * Back buffer and front buffer are independent; this may be called while
 * push_row() is feeding the front buffer to Core 1. */
void zx81_screen_render_line(gfx_color_t *fb, int y)
{
    if (osd_state != 0) {
        render_osd_line(fb, y);
        return;
    }

    /* Top or bottom border */
    if (y < ZX81_Y_OFFSET || y >= ZX81_Y_OFFSET + ZX81_DISP_H) {
        gfx_fb_hline(fb, 0, y, GFX_W, COL_BORDER);
        return;
    }

    /* Left and right borders */
    gfx_fb_hline(fb, 0,                            y, ZX81_X_OFFSET,                       COL_BORDER);
    gfx_fb_hline(fb, ZX81_X_OFFSET + ZX81_DISP_W, y, GFX_W - ZX81_X_OFFSET - ZX81_DISP_W, COL_BORDER);

    if (!s_valid) {
        gfx_fb_hline(fb, ZX81_X_OFFSET, y, ZX81_DISP_W, COL_PAPER);
        return;
    }

    int disp_y   = y - ZX81_Y_OFFSET;
    int char_row = disp_y / ZX81_CHAR_H;
    int pixel_y  = disp_y % ZX81_CHAR_H;

    uint16_t     row_base = s_row_ptr[char_row];
    gfx_color_t *out_row  = fb + (uint32_t)y * GFX_W + ZX81_X_OFFSET;

    for (int col = 0; col < ZX81_COLS; col++) {
        uint8_t ch_byte = m[row_base + (uint16_t)col];

        if (ch_byte == 0x76) {
            /* HALT: fill rest of row with PAPER */
            gfx_fb_hline(fb, ZX81_X_OFFSET + col * ZX81_CHAR_W, y,
                         (ZX81_COLS - col) * ZX81_CHAR_W, COL_PAPER);
            return;
        }

        bool    inv    = (ch_byte & 0x80u) != 0;
        uint8_t ch     = ch_byte & 0x3Fu;
        uint16_t bmp   = ZX81_CHARSET_ADDR + (uint16_t)ch * ZX81_CHAR_H;
        uint8_t  pixels = m[bmp + (uint16_t)pixel_y];

        gfx_color_t *out = out_row + col * ZX81_CHAR_W;
        for (int cx = 0; cx < ZX81_CHAR_W; cx++) {
            bool ink = (pixels >> (7 - cx)) & 1u;
            if (inv) ink = !ink;
            out[cx] = ink ? COL_INK : COL_PAPER;
        }
    }
}

/* ---- Full-frame render (prepare + all scanlines) ------------------------ */

void zx81_screen_render(gfx_color_t *fb)
{
    zx81_screen_prepare();
    for (int y = 0; y < GFX_H; y++)
        zx81_screen_render_line(fb, y);
}
