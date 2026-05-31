/*
 * zx81_kbd.c  ZX81 keyboard via USB-CDC serial
 *
 * Each frame, zx81_kbd_scan() reads one character from USB CDC (non-blocking)
 * and maps it to the ZX81 8×5 key matrix.  The key is held for
 * KBD_HOLD_FRAMES frames, then cleared.  A KBD_GAP_FRAMES dead time follows
 * before the next character is accepted.
 *
 * ASCII → ZX81 matrix mapping mirrors the JS reference (zx81_emu.js).
 * Uppercase letters add SHIFT (row 0 col 0) to the corresponding letter key.
 * Symbols that require ZX81 SHIFT are mapped to SHIFT + the appropriate key.
 */

#include "zx81_kbd.h"
#include "zx81_load.h"
#include "zx81_screen.h"
#include "zx81.h"
#include "z80_api.h"
#include "airfight_bin.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* Frames to hold a key pressed, and gap before accepting the next one.
 * At ~50 Hz this is ~60 ms hold + ~40 ms gap per keystroke. */
#define KBD_HOLD_FRAMES  3
#define KBD_GAP_FRAMES   2

/* Key matrix: bit n of kbd_rows[row] = column n is pressed */
static uint8_t kbd_rows[8];

static enum { KBD_IDLE, KBD_ESC, KBD_CSI, KBD_HOLD, KBD_GAP } kbd_state;
static int kbd_timer;
static int esc_timer;

/* Frames to wait for the rest of an escape sequence before giving up. */
#define ESC_TIMEOUT_FRAMES  5

/* ---- ASCII → ZX81 matrix -------------------------------------------- */

typedef struct { uint8_t r1, c1, r2, c2; } km_t;

/* Row indices */
#define R_SH  0   /* SHIFT / Z / X / C / V */
#define R_A   1   /* A / S / D / F / G     */
#define R_Q   2   /* Q / W / E / R / T     */
#define R_1   3   /* 1 / 2 / 3 / 4 / 5    */
#define R_0   4   /* 0 / 9 / 8 / 7 / 6    */
#define R_P   5   /* P / O / I / U / Y     */
#define R_EN  6   /* ENTER / L / K / J / H */
#define R_SP  7   /* SPACE / . / M / N / B */

/* Column bit masks */
#define C0  (1u<<0)
#define C1  (1u<<1)
#define C2  (1u<<2)
#define C3  (1u<<3)
#define C4  (1u<<4)

/* Map helpers */
#define NK        { 0xFF, 0,  0xFF, 0  }   /* no mapping      */
#define K1(r,c)   { r,    c,  0xFF, 0  }   /* single key      */
#define KS(r,c)   { r,    c,  R_SH, C0 }   /* SHIFT + key     */

/* 128-entry table indexed by ASCII code.
 * Unspecified entries default to { 0, 0, 0, 0 } which is treated as NK
 * because r1==0 and c1==0 is a degenerate (SHIFT row, no columns pressed). */
static const km_t km[128] = {

    /* Control characters */
    ['\r'] = K1(R_EN, C0),           /* CR    → ENTER           */
    ['\n'] = K1(R_EN, C0),           /* LF    → ENTER           */
    ['\b'] = KS(R_0,  C0),           /* BS    → SHIFT+0 (Delete)*/
    [0x7F] = KS(R_0,  C0),           /* DEL   → SHIFT+0 (Delete)*/

    /* Space */
    [' ']  = K1(R_SP, C0),

    /* Digits */
    ['0']  = K1(R_0,  C0),
    ['1']  = K1(R_1,  C0),
    ['2']  = K1(R_1,  C1),
    ['3']  = K1(R_1,  C2),
    ['4']  = K1(R_1,  C3),
    ['5']  = K1(R_1,  C4),
    ['6']  = K1(R_0,  C4),
    ['7']  = K1(R_0,  C3),
    ['8']  = K1(R_0,  C2),
    ['9']  = K1(R_0,  C1),

    /* Lowercase letters */
    ['a']  = K1(R_A,  C0), ['b']  = K1(R_SP, C4), ['c']  = K1(R_SH, C3),
    ['d']  = K1(R_A,  C2), ['e']  = K1(R_Q,  C2), ['f']  = K1(R_A,  C3),
    ['g']  = K1(R_A,  C4), ['h']  = K1(R_EN, C4), ['i']  = K1(R_P,  C2),
    ['j']  = K1(R_EN, C3), ['k']  = K1(R_EN, C2), ['l']  = K1(R_EN, C1),
    ['m']  = K1(R_SP, C2), ['n']  = K1(R_SP, C3), ['o']  = K1(R_P,  C1),
    ['p']  = K1(R_P,  C0), ['q']  = K1(R_Q,  C0), ['r']  = K1(R_Q,  C3),
    ['s']  = K1(R_A,  C1), ['t']  = K1(R_Q,  C4), ['u']  = K1(R_P,  C3),
    ['v']  = K1(R_SH, C4), ['w']  = K1(R_Q,  C1), ['x']  = K1(R_SH, C2),
    ['y']  = K1(R_P,  C4), ['z']  = K1(R_SH, C1),

    /* Uppercase letters = SHIFT + letter */
    ['A']  = KS(R_A,  C0), ['B']  = KS(R_SP, C4), ['C']  = KS(R_SH, C3),
    ['D']  = KS(R_A,  C2), ['E']  = KS(R_Q,  C2), ['F']  = KS(R_A,  C3),
    ['G']  = KS(R_A,  C4), ['H']  = KS(R_EN, C4), ['I']  = KS(R_P,  C2),
    ['J']  = KS(R_EN, C3), ['K']  = KS(R_EN, C2), ['L']  = KS(R_EN, C1),
    ['M']  = KS(R_SP, C2), ['N']  = KS(R_SP, C3), ['O']  = KS(R_P,  C1),
    ['P']  = KS(R_P,  C0), ['Q']  = KS(R_Q,  C0), ['R']  = KS(R_Q,  C3),
    ['S']  = KS(R_A,  C1), ['T']  = KS(R_Q,  C4), ['U']  = KS(R_P,  C3),
    ['V']  = KS(R_SH, C4), ['W']  = KS(R_Q,  C1), ['X']  = KS(R_SH, C2),
    ['Y']  = KS(R_P,  C4), ['Z']  = KS(R_SH, C1),

    /* SHIFT+digit → ZX81 SHIFT+digit functions */
    ['!']  = KS(R_1,  C0),   /* SHIFT+1  = EDIT                  */
    ['@']  = KS(R_1,  C1),   /* SHIFT+2  = CAPS LOCK             */
    ['#']  = KS(R_1,  C2),   /* SHIFT+3  = TRUE VIDEO            */
    ['$']  = KS(R_1,  C3),   /* SHIFT+4  = INV VIDEO             */
    ['%']  = KS(R_1,  C4),   /* SHIFT+5  = ← (cursor left)      */
    ['^']  = KS(R_0,  C4),   /* SHIFT+6  = ↓ (cursor down)      */
    ['&']  = KS(R_0,  C3),   /* SHIFT+7  = ↑ (cursor up)        */

    /* Symbols — SHIFT combinations from ZX81 keyboard layout */
    ['"']  = KS(R_P,  C0),   /* SHIFT+P  = "  (double quote)    */
    ['.']  = K1(R_SP, C1),   /* .                                */
    [',']  = KS(R_SP, C1),   /* SHIFT+.  = ,                    */
    ['-']  = KS(R_EN, C3),   /* SHIFT+J  = -                    */
    ['+']  = KS(R_EN, C2),   /* SHIFT+K  = +                    */
    ['=']  = KS(R_EN, C1),   /* SHIFT+L  = =                    */
    [';']  = KS(R_SH, C2),   /* SHIFT+X  = ;                    */
    [':']  = KS(R_SH, C1),   /* SHIFT+Z  = :                    */
    ['/']  = KS(R_SH, C4),   /* SHIFT+V  = /                    */
    ['<']  = KS(R_Q,  C3),   /* SHIFT+R  = <                    */
    ['>']  = KS(R_Q,  C4),   /* SHIFT+T  = >                    */
    ['(']  = KS(R_0,  C1),   /* SHIFT+9  = (                    */
    [')']  = KS(R_0,  C0),   /* SHIFT+0  = ) in L-mode / DELETE in editor */
    ['*']  = KS(R_SP, C2),   /* SHIFT+M  = * (multiply)         */
};

/* ---- Display dump (Ctrl+D) ------------------------------------------ */

/* ZX81 character codes → printable ASCII.
 * 0x00 = space, 0x01-0x0A = block graphics (shown as .), 0x0B-0x1B = symbols,
 * 0x1C-0x25 = digits, 0x26-0x3F = A-Z. */
static const char zx81_chr[64] = {
    /* 00 */  ' ',
    /* 01-0A */ '.','.','.','.','.','.','.','.','.','.',
    /* 0B */ '"',
    /* 0C */ '#',   /* £ */
    /* 0D */ '$',
    /* 0E */ ':',
    /* 0F */ '?',
    /* 10 */ '(',
    /* 11 */ ')',
    /* 12 */ '>',
    /* 13 */ '<',
    /* 14 */ '=',
    /* 15 */ '+',
    /* 16 */ '-',
    /* 17 */ '*',
    /* 18 */ '/',
    /* 19 */ ';',
    /* 1A */ ',',
    /* 1B */ '.',
    /* 1C-25 */ '0','1','2','3','4','5','6','7','8','9',
    /* 26-3F */ 'A','B','C','D','E','F','G','H','I','J',
                'K','L','M','N','O','P','Q','R','S','T',
                'U','V','W','X','Y','Z'
};

static void dump_display(void)
{
    uint16_t d_file = (uint16_t)m[0x400C] | ((uint16_t)m[0x400D] << 8);
    if (d_file < 0x4000u || d_file > 0xBE00u) {
        printf("[D_FILE invalid: 0x%04X]\r\n", (unsigned)d_file);
        return;
    }

    printf("\r\n+--------------------------------+\r\n");

    uint16_t ptr = d_file + 1;   /* skip leading HALT preamble */
    for (int row = 0; row < 24; row++) {
        printf("|");
        int col;
        for (col = 0; col < 32; col++) {
            uint8_t b = m[ptr + (uint16_t)col];
            if (b == 0x76) break;                        /* row-end HALT */
            bool inv = (b & 0x80) != 0;
            char c = zx81_chr[b & 0x3F];
            /* Inverse chars shown in lowercase for visibility */
            putchar(inv && c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
        }
        /* Pad short rows with spaces */
        for (int i = col; i < 32; i++) putchar(' ');
        printf("|\r\n");

        /* Advance ptr past this row's HALT */
        if (col < 32) {
            ptr += (uint16_t)(col + 1);
        } else {
            ptr += 32;
            while (ptr < 0x7FFFu && m[ptr] != 0x76) ptr++;
            ptr++;
        }
    }

    printf("+--------------------------------+\r\n");
}

/* ---- Public API ----------------------------------------------------- */

void zx81_kbd_init(void)
{
    memset(kbd_rows, 0, sizeof(kbd_rows));
    kbd_state = KBD_IDLE;
    kbd_timer = 0;
    esc_timer = 0;
}

/* Press a key from a km_t entry and enter HOLD state. */
static void press_key(const km_t *k)
{
    memset(kbd_rows, 0, sizeof(kbd_rows));
    kbd_rows[k->r1] |= k->c1;
    if (k->r2 != 0xFF)
        kbd_rows[k->r2] |= k->c2;
    kbd_timer = KBD_HOLD_FRAMES;
    kbd_state = KBD_HOLD;
}

void zx81_kbd_scan(void)
{
    switch (kbd_state) {

    case KBD_IDLE: {
        int ch = getchar_timeout_us(0);
        if (ch < 0 || ch >= 128) break;

        if (ch == 0x04) { dump_display(); break; }       /* Ctrl+D: display dump  */
        if (ch == 0x12) {                               /* Ctrl+R: launch airfight*/
            memcpy(m + AIRFIGHT_ORG, airfight_bin, sizeof(airfight_bin));
            zx81_game_launch(AIRFIGHT_ORG);
            printf("GAME: airfight launched at 0x%04X (%u bytes)\r\n",
                   AIRFIGHT_ORG, (unsigned)sizeof(airfight_bin));
            break;
        }
        if (ch == 0x0C) { zx81_load_start(); break; }  /* Ctrl+L: tape load     */
        if (ch == 0x0F) { zx81_osd_cycle(); break; }   /* Ctrl+O: OSD overlay   */
        if (ch == 0x1B) {                               /* ESC: start of sequence */
            kbd_state = KBD_ESC;
            esc_timer = ESC_TIMEOUT_FRAMES;
            break;
        }

        const km_t *k = &km[(uint8_t)ch];
        if (k->r1 == 0xFF || k->c1 == 0) break;
        press_key(k);
        break;
    }

    case KBD_ESC: {
        /* Wait for '[' (CSI) or 'O' (SS3 – e.g. F1=ESC O P). */
        int ch = getchar_timeout_us(0);
        if (ch == '[') {
            kbd_state = KBD_CSI;
            esc_timer = ESC_TIMEOUT_FRAMES;
        } else if (ch == 'O') {
            /* SS3: next byte is the function key letter */
            int fk = getchar_timeout_us(0);
            if (fk == 'P') zx81_osd_cycle();   /* F1 = ESC O P */
            kbd_state = KBD_IDLE;
        } else if (ch >= 0 || --esc_timer <= 0) {
            kbd_state = KBD_IDLE;
        }
        break;
    }

    case KBD_CSI: {
        /* Map ANSI cursor-key final bytes → ZX81 SHIFT+digit cursor movement:
         *   A (up)    → SHIFT+7   B (down)  → SHIFT+6
         *   C (right) → SHIFT+8   D (left)  → SHIFT+5  */
        static const km_t csi_map[4] = {
            KS(R_0, C3),   /* A: up    → SHIFT+7 */
            KS(R_0, C4),   /* B: down  → SHIFT+6 */
            KS(R_0, C2),   /* C: right → SHIFT+8 */
            KS(R_1, C4),   /* D: left  → SHIFT+5 */
        };
        int ch = getchar_timeout_us(0);
        if (ch < 0) {
            if (--esc_timer <= 0) kbd_state = KBD_IDLE;
            break;
        }
        if (ch >= 'A' && ch <= 'D') {
            press_key(&csi_map[ch - 'A']);
        } else {
            kbd_state = KBD_IDLE;
        }
        break;
    }

    case KBD_HOLD:
        if (--kbd_timer <= 0) {
            memset(kbd_rows, 0, sizeof(kbd_rows));
            kbd_timer = KBD_GAP_FRAMES;
            kbd_state = KBD_GAP;
        }
        break;

    case KBD_GAP:
        if (--kbd_timer <= 0)
            kbd_state = KBD_IDLE;
        break;
    }
}

uint8_t zx81_kbd_read_port(uint8_t hi)
{
    uint8_t pressed = 0;
    for (int row = 0; row < 8; row++) {
        if (!((hi >> row) & 1))
            pressed |= kbd_rows[row];
    }
    /* Active-low: 0 = pressed.  Upper bits set to 1 (open bus / no tape). */
    return ~pressed & 0xFF;
}
