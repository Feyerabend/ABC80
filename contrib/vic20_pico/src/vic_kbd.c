#include "vic_kbd.h"
#include "memory.h"
#include "via.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <stdint.h>

// ---- PETSCII injection ------------------------------------------------------
//
// Regular keys are injected directly into the KERNAL keyboard buffer so that
// GETIN picks them up without needing a working VIA key matrix scan.
//
//   $C6       NDXA  — number of characters in queue (0-10)
//   $0277-$0280     — keyboard queue (10 bytes)

static void inject_petscii(uint8_t petscii) {
    uint8_t *ram = memory_raw();
    uint8_t n = ram[0x00C6];
    if (n < 10) {
        ram[0x0277 + n] = petscii;
        ram[0x00C6]     = n + 1;
    }
}

// ---- STOP key (RUN/STOP) via VIA2 key matrix --------------------------------
//
// The KERNAL STOP check ($FFE1 → $F770) reads zero-page $91 which is updated
// every IRQ by UDTIM ($FFEA).  UDTIM reads VIA2 Port A reg $0F ($912F) with
// Port B left at $F7 (column 3 selected) by SCNKEY.  STOP key is at col=3,
// row=0 in the matrix; Port A = $FE (bit 0 clear) signals STOP.
//
// Keyboard-buffer injection of $03 is NOT enough — we must use the matrix.

static int stop_held_frames;

static void press_stop(void) {
    via_key_event(0, 3, 1);   // col=3, row=0 pressed
    stop_held_frames = 2;     // hold for 2 frames so UDTIM sees it
}

// ---- ASCII → PETSCII --------------------------------------------------------
//
// VT100/ANSI terminals send standard ASCII.  In PETSCII (uppercase mode):
//   0x20-0x5F  identical to ASCII (space, punctuation, digits, A-Z, ^, _)
//   0x61-0x7A  lowercase a-z: map to 0x41-0x5A (uppercase A-Z in PETSCII)
//   0x0D/0x0A  Return
//   0x08/0x7F  Backspace/DEL → PETSCII DELETE (0x14)

static uint8_t to_petscii(uint8_t ch) {
    if (ch == '\r' || ch == '\n') return 0x0D;
    if (ch == 0x08 || ch == 0x7F) return 0x14;      // Backspace / DEL → DELETE
    if (ch >= 'a'  && ch <= 'z')  return ch - 0x20;  // lowercase → uppercase PETSCII
    if (ch >= 0x20 && ch <= 0x5F) return ch;          // printable ASCII subset = direct PETSCII
    return 0;  // no mapping — drop
}

// ---- ANSI / VT100 escape sequence state machine -----------------------------

#define ESC_TIMEOUT 5   // frames to wait for the rest of an escape sequence

static enum { KBD_IDLE, KBD_ESC, KBD_CSI, KBD_SS3 } kbd_state;
static int     esc_timer;
static uint8_t csi_param;

void vic_kbd_init(void) {
    kbd_state        = KBD_IDLE;
    esc_timer        = 0;
    csi_param        = 0;
    stop_held_frames = 0;
}

void vic_kbd_scan(void) {
    // Release STOP key after the hold period expires.
    if (stop_held_frames > 0 && --stop_held_frames == 0)
        via_key_event(0, 3, 0);

    switch (kbd_state) {

    case KBD_IDLE: {
        int ch = getchar_timeout_us(0);
        if (ch < 0) break;

        if (ch == 0x1B) {
            kbd_state = KBD_ESC;
            esc_timer = ESC_TIMEOUT;
            break;
        }

        // Ctrl-C → RUN/STOP via key matrix (not keyboard buffer)
        if (ch == 0x03) { press_stop(); break; }

        uint8_t p = to_petscii((uint8_t)ch);
        if (p) inject_petscii(p);
        break;
    }

    case KBD_ESC: {
        int ch = getchar_timeout_us(0);
        if (ch == '[') { kbd_state = KBD_CSI; esc_timer = ESC_TIMEOUT; csi_param = 0; break; }
        if (ch == 'O') { kbd_state = KBD_SS3; esc_timer = ESC_TIMEOUT; break; }
        if (ch < 0 && --esc_timer > 0) break;
        // Bare ESC or timeout → RUN/STOP
        press_stop();
        kbd_state = KBD_IDLE;
        break;
    }

    case KBD_CSI: {
        // CSI = ESC [ ...
        int ch = getchar_timeout_us(0);
        if (ch < 0) { if (--esc_timer <= 0) kbd_state = KBD_IDLE; break; }
        if (ch >= '0' && ch <= '9') { csi_param = (uint8_t)(csi_param * 10 + (ch - '0')); break; }

        switch (ch) {
        case 'A': inject_petscii(0x91); break; // cursor up
        case 'B': inject_petscii(0x11); break; // cursor down
        case 'C': inject_petscii(0x1D); break; // cursor right
        case 'D': inject_petscii(0x9D); break; // cursor left
        case 'H': inject_petscii(0x13); break; // Home
        case 'F': inject_petscii(0x13); break; // End → HOME (best available)
        case '~':
            switch (csi_param) {
            case 1: inject_petscii(0x13); break;  // Home
            case 3: inject_petscii(0x14); break;  // Delete (forward)
            case 4: inject_petscii(0x13); break;  // End → HOME
            // F1-F8 via xterm ESC[11~..ESC[18~
            case 11: inject_petscii(0x85); break;
            case 12: inject_petscii(0x89); break;
            case 13: inject_petscii(0x86); break;
            case 14: inject_petscii(0x8A); break;
            case 15: inject_petscii(0x87); break;
            case 17: inject_petscii(0x8B); break;
            case 18: inject_petscii(0x88); break;
            case 19: inject_petscii(0x8C); break;
            }
            break;
        }
        kbd_state = KBD_IDLE;
        break;
    }

    case KBD_SS3: {
        // SS3 = ESC O ... (function keys / arrows on some terminals)
        int ch = getchar_timeout_us(0);
        if (ch < 0) { if (--esc_timer <= 0) kbd_state = KBD_IDLE; break; }
        switch (ch) {
        case 'A': inject_petscii(0x91); break; // up
        case 'B': inject_petscii(0x11); break; // down
        case 'C': inject_petscii(0x1D); break; // right
        case 'D': inject_petscii(0x9D); break; // left
        case 'P': inject_petscii(0x85); break; // F1
        case 'Q': inject_petscii(0x89); break; // F2
        case 'R': inject_petscii(0x86); break; // F3
        case 'S': inject_petscii(0x8A); break; // F4
        }
        kbd_state = KBD_IDLE;
        break;
    }
    }
}
