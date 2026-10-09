
// ABC80 machine core for Raspberry Pi Pico 2W
//
// Derived from emu/abc.c.  All ncurses and host-filesystem code has been
// removed.  Keyboard input comes from USB CDC serial (getchar_timeout_us).
// Display output is done by the caller via abc80_screen_char().

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "pico/stdlib.h"

#include "abcprom.h"
#include "z80.h"   // exposes m[], pc, iff1, im, etc.
#include "abc80.h"

// ---------------------------------------------------------------------------
// Screen RAM layout
//
// ABC80 uses a 24 row × 40 col display.  The rows are stored non-linearly
// in Z80 address space: they are interleaved in banks of 8, each row
// 0x28 bytes apart from the next within a bank.
static const uint16_t rowstart[24] = {
    0x7C00, 0x7C80, 0x7D00, 0x7D80, 0x7E00, 0x7E80, 0x7F00, 0x7F80,
    0x7C28, 0x7CA8, 0x7D28, 0x7DA8, 0x7E28, 0x7EA8, 0x7F28, 0x7FA8,
    0x7C50, 0x7CD0, 0x7D50, 0x7DD0, 0x7E50, 0x7ED0, 0x7F50, 0x7FD0
};

// ---------------------------------------------------------------------------
// Port addresses
#define KB_PORT     0x38
#define KB_INT_VEC  0x34
#define B_PORT      0x3A

// ---------------------------------------------------------------------------
// Keyboard state
static uint8_t pending_key = 0;
static int     key_reads   = 0;

// ESC sequence state for arrow-key decoding over serial.
static int esc_state = 0;   // 0 = idle, 1 = saw ESC, 2 = saw ESC [

// ---------------------------------------------------------------------------
// Character set conversion (tested under "screen" on macOS)
//
// ABC80 uses SIS 662241, a Swedish variant of 7-bit ASCII.  When the user
// sends Swedish characters over the USB serial link they arrive as UTF-8
// 2-byte sequences.  We translate the common Nordic letters and ¤.
static uint8_t utf8_to_abc80(unsigned char lead, unsigned char cont) {
    if (lead == 0xC2) {
        return (cont == 0xA4) ? 0x24 : 0;   // ¤  U+00A4
    }
    if (lead == 0xC3) {
        switch (cont) {
            case 0xA4: return 0x7B;  // ä  U+00E4
            case 0xB6: return 0x7C;  // ö  U+00F6
            case 0xA5: return 0x7D;  // å  U+00E5
            case 0x84: return 0x5B;  // Ä  U+00C4
            case 0x96: return 0x5C;  // Ö  U+00D6
            case 0x85: return 0x5D;  // Å  U+00C5
            case 0xA9: return 0x60;  // é  U+00E9
            case 0x9C: return 0x5E;  // Ü  U+00DC
            case 0xBC: return 0x7E;  // ü  U+00FC
            default:   return 0;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Port I/O callbacks (called by the Z80 emulator on IN/OUT instructions)

u8 port_in(u8 port) {
    if (port == KB_PORT) {
        if (key_reads <= 0)
            return 0;
        uint8_t val = pending_key;
        if (--key_reads == 0) {
            pending_key = 0;
            m[0xFDF5]   = 0;
        }
        return val;
    }
    if (port == B_PORT)
        return 0xFF;   // B bus idle: all lines high
    return 0;
}

void port_out(u8 port, u8 val) {
    (void)port;
    (void)val;
    // Port 6 = buzzer.  Not wired on the Display Pack; ignore for now. You do!
}

// ---------------------------------------------------------------------------
// Public API

void abc80_init(void) {
    init();                              // reset Z80 registers
    memcpy(m, abcprom, sizeof(abcprom)); // load ROM image
    pc = 0x0000;                         // start at reset vector
}

void abc80_step(void) {
    step();
}

void abc80_strobe(void) {
    // Mimic the ROM's NMI handler (ABC80: VSYNC -> NMI @ 50 Hz?).
    // The handler uses DJNZ on m[0xFDF0]: decrement, fall through to outer
    // 16-bit counter at m[0xFDF1-2] only when m[0xFDF0] transitions 1 -> 0.
    if (m[0xFDF0] == 1) {
        uint16_t outer = (uint16_t)m[0xFDF1] | ((uint16_t)m[0xFDF2] << 8);
        if (outer > 0) outer--;
        m[0xFDF1] = outer & 0xFF;
        m[0xFDF2] = outer >> 8;
    }
    m[0xFDF0]--;   // wraps 0 -> 255 naturally, matching DJNZ wrap behaviour

    gen_int(KB_INT_VEC);
}

// abc80_keyboard_poll — call on every main-loop iteration (non-blocking).
//
// Mirrors the emu's approach: keyboard is polled every Z80 step so that
// BASIC's tight IN A,(0x38) poll loop at 0x02F7 always sees a fresh key.
// getchar_timeout_us(0) is non-blocking — it just checks the USB FIFO.
void abc80_keyboard_poll(void) {
    if (pending_key != 0)
        return;

    int ch = getchar_timeout_us(0);   // non-blocking: check USB FIFO only
    if (ch == PICO_ERROR_TIMEOUT)
        return;

    uint8_t abc = 0;

    // Handle ESC sequences in progress from the previous call.
    if (esc_state == 1) {
        esc_state = 0;
        if (ch == '[') { esc_state = 2; return; }
        // Option/Alt-key combos (minicom Meta mode): map to ABC80 codes.
        switch (ch) {
            case '4':           abc = 0x24; break;  // Option+4  -> ¤
            case 'e': case 'E': abc = 0x60; break;  // Option+e  -> é
            case 'u': case 'U': abc = 0x7E; break;  // Option+u  -> ü
            default: return;                        // unknown, discard
        }
    } else if (esc_state == 2) {
        esc_state = 0;
        switch (ch) {
            case 'C': abc = 0x09; break;   // right arrow
            case 'D': abc = 0x08; break;   // left  arrow
            default:  return;
        }
    } else {
        switch (ch) {
            case 0x1B: esc_state = 1; return;
            case 0x03: abc = 0x03; break;   // Ctrl-C -> ABC80 break (ISR sees 0x83)
            case 0x7F:
            case 0x08: abc = 0x08; break;   // DEL/BS -> backspace
            case 0x0A:
            case 0x0D: abc = 0x0D; break;   // LF/CR  -> CR
            case 0xC2:
            case 0xC3: {
                int b2 = getchar_timeout_us(5000);
                if (b2 == PICO_ERROR_TIMEOUT) return;
                abc = utf8_to_abc80((unsigned char)ch, (unsigned char)b2);
                break;
            }
            default:
                if (ch >= 0x20 && ch <= 0x7E) abc = (uint8_t)ch;
                break;
        }
    }

    if (abc == 0)
        return;

    pending_key = abc | 0x80;
    key_reads   = 2;
    // Pre-write the ISR latch so BASIC's fast-path check (BIT 7,(ix+2))
    // fires immediately after the next strobe interrupt.
    m[0xFDF5] = pending_key;
}

uint8_t abc80_screen_char(int row, int col) {
    return m[rowstart[row] + col];
}

void abc80_get_regs(abc80_regs_t *r) {
    r->a  = a;  r->b = b;  r->c = c;  r->d = d;
    r->e  = e;  r->h = h;  r->l = l;
    r->pc = pc; r->sp = sp; r->ix = ix; r->iy = iy;
    r->im = im; r->iff1 = iff1; r->iff2 = iff2;
    r->f  = (uint8_t)(((uint8_t)sf << 7) | ((uint8_t)zf << 6) |
                      ((uint8_t)yf << 5) | ((uint8_t)hf << 4) |
                      ((uint8_t)xf << 3) | ((uint8_t)pf << 2) |
                      ((uint8_t)nf << 1) |  (uint8_t)cf);
}

uint8_t abc80_read_mem(uint16_t addr) {
    return m[addr];
}
