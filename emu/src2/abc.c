#define _DEFAULT_SOURCE
// ABC80 emulator

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdint.h>
#include <stdbool.h>
#include <locale.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <ncurses.h>

#include "abcprom.h"
#include "z80.h"

//   ABC80 uses a Swedish variant of 7-bit ASCII (SIS 662241) (SIS 63 61 27)
//   Some positions that differ from standard ASCII are:
//
//   0x24 = ¤    "sol-symbol" ($ in ASCII)
//   0x5B = Ä    0x5C = Ö    0x5D = Å
//   0x7B = ä    0x7C = ö    0x7D = å
//   0x5E = Ü    0x7E = ü
//   0x60 = é
//
//   Input arrives in two flavours depending on how the terminal is configured:
//
//   Direct UTF-8 (Option-as-character mode, default on macOS Terminal)
//   Sorry, you might have to change this is not a macOS is used:
//     The special character is sent as a raw multi-byte UTF-8 sequence.
//     We catch the lead byte (0xC2 or 0xC3) and immediately read the
//     continuation byte from the same getch() call.
//
//   Meta/ESC prefix (Option-as-Meta mode, or pressing Option then key):
//     The terminal sends 0x1B followed by a second byte.  We must NOT
//     treat every 0x1B as "quit" -- instead we cache the ESC in
//     esc_pending and resolve it on the very next read_key() call:
//       ESC with no follow-up  --> bare ESC key --> quit
//       ESC + '4'              --> ¤  (0x24)
//       ESC + 'e' / 'E'        --> é  (0x60)
//       ESC + 'u' / 'U'        --> ü  (0x7E)
//       ESC + anything else    --> silently ignored

static const uint16_t rowstart[24] = {
    0x7C00, 0x7C80, 0x7D00, 0x7D80, 0x7E00, 0x7E80, 0x7F00, 0x7F80,
    0x7C28, 0x7CA8, 0x7D28, 0x7DA8, 0x7E28, 0x7EA8, 0x7F28, 0x7FA8,
    0x7C50, 0x7CD0, 0x7D50, 0x7DD0, 0x7E50, 0x7ED0, 0x7F50, 0x7FD0
};

#define KB_PORT     0x38
#define KB_INT_VEC  0x34
#define IEC_PORT    0x3A   // PIO B
#define STROBE_HZ          50
#define STEPS_PER_STROBE   (3000000 / STROBE_HZ)
#define STEPS_PER_REFRESH  5000

static uint8_t pending_key = 0;
static int     key_reads   = 0;
static bool    done        = false;

static long long now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long) ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

// Translate ncurses input to an ABC80 key code.
// Returns 0 if no key or unrecognised.
static bool esc_pending = false;  // true when 0x1B was the last byte seen

static uint8_t read_key(void) {
    int ch = getch();

    // Resolve a cached ESC
    // The previous call saw 0x1B and returned without acting.
    // Now we know what (if anything) followed it.
    if (esc_pending) {
        esc_pending = false;
        if (ch == ERR) {
            // Nothing followed the ESC -> it was a real bare ESC key -> quit.
            done = true;
            return 0;
        }
        // ESC + char = Option/Alt modifier combo.  Map to ABC80 codes.
        switch (ch) {
            case '4':           return 0x24;  // Option+4  -> ¤  (sun/currency)
            case 'e': case 'E': return 0x60;  // Option+e  -> é
            case 'u': case 'U': return 0x7E;  // Option+u  -> ü
            default:            return 0;     // Unknown combo, swallow silently
        }
    }

    if (ch == ERR)
        return 0;

    switch (ch) {
        // Cache ESC; do NOT exit yet -- it might be an Option-key prefix.
        case 0x1B:
            esc_pending = true;
            return 0;

        case 0x7F:                           // DEL
        case 0x08:  return 0x08;             // backspace
        case 0x0A:                           // LF
        case 0x0D:  return 0x0D;             // CR -> BASIC expects CR

        // UTF-8 lead byte 0xC2 (U+0080..U+00BF): ¤ and friends.
        case 0xC2: {
            int b2 = getch();
            switch (b2) {
                case 0xA4: return 0x24;  // ¤  (U+00A4)
                default:   return 0;
            }
        }

        // UTF-8 lead byte 0xC3 (U+00C0..U+00FF): Nordic letters + é/ü.
        case 0xC3: {
            int b2 = getch();
            switch (b2) {
                case 0xA4: return 0x7B;  // ä  (U+00E4)
                case 0xB6: return 0x7C;  // ö  (U+00F6)
                case 0xA5: return 0x7D;  // å  (U+00E5)
                case 0x84: return 0x5B;  // Ä  (U+00C4)
                case 0x96: return 0x5C;  // Ö  (U+00D6)
                case 0x85: return 0x5D;  // Å  (U+00C5)
                case 0xA9: return 0x60;  // é  (U+00E9)
                case 0x9C: return 0x5E;  // Ü  (U+00DC)
                case 0xBC: return 0x7E;  // ü  (U+00FC)
                default:   return 0;
            }
        }

        // Silently consume any other multi-byte UTF-8 lead bytes (e.g. 0xE2
        // for €) so stray continuation bytes don't pollute later reads.
        case 0xE2: case 0xE3: case 0xE4: case 0xE5: case 0xE6: case 0xE7:
        case 0xE8: case 0xE9: case 0xEA: case 0xEB: case 0xEC: case 0xED:
        case 0xEE: case 0xEF:
            getch(); getch();   // discard the two continuation bytes
            return 0;
        case 0xF0: case 0xF1: case 0xF2: case 0xF3:
            getch(); getch(); getch();  // discard three continuation bytes
            return 0;

        // as far as I remeber the editing ..
        case KEY_LEFT:  return 0x08;  // cursor left (erasing)
        case KEY_RIGHT: return 0x09;  // cursor right (brings characters "out")

        default:
            if (ch >= 0x20 && ch <= 0x7E)
                return (uint8_t) ch;
            return 0;
    }
}


// ELOAD / ESAVE / ELIB -- host file I/O for BASIC programs
// -----------------------------------------------------------
// The ABC80 line input buffer in screen RAM. The current input row
// is m[0xFDF3] (index into rowstart[]). On CR we read that row,
// check for our commands and handle them in C before BASIC
// sees the line.
//
// BASIC program memory (from ROM analysis):
//   m[0xFE1C..1D] = program start address (set by RAM test at boot)
//   m[0xFE1E..1F] = end of program / start of variable area
//   m[0xFE20..21] = end of variable area / start of free space
//
// We save/load the raw tokenised bytes [start, end) as a flat .BAC file.
// (Not sure how close it is to the real situation.)

// Read the current input row from screen RAM into buf.
// Strips bit 7 (ABC80 attribute), removes trailing spaces,
// null-terminates.
static void read_screen_line(char *buf, int buflen) {
    uint8_t row = m[0xFDF3];
    if (row >= 24) { buf[0] = 0; return; }

    uint16_t base = rowstart[row];
    int len = 0;
    for (int col = 0; col < 40 && len < buflen - 1; col++) {
        uint8_t c = m[base + col] & 0x7F;
        buf[len++] = (c < 0x20) ? ' ' : (char) c;
    }
    buf[len] = 0;
    while (len > 0 && buf[len - 1] == ' ')
        buf[--len] = 0;
}

// Display a status message in response to one of our intercepted commands.
//
// The ABC80 line input routine maintains 2 parallel buffers:
//  - Screen RAM  (non-linear, via rowstart[]) - what is visible on screen (BUF1?)
//  - $FE40-$FEB7 (flat, 120-byte)             - what BASIC tokenises on CR (BUF2)
//
//   1. Write the message into screen RAM at the CURRENT cursor row,
//      overwriting the command the user typed. Immediately visible.
//   2. Put $0D into $FE40[0] so BASIC sees an empty (CR-only) line.
//   3. Reset $FDF4 (column counter, IX+1) to 0.
//   4. Advance $FDF3 (row counter, IX+0) by 1. BASIC's prompt routine
//      outputs CR (resets col) then LF (advances row) before printing
//      prompt, so starting from row+1 puts the prompt on row+2, leaving
//      our message on row undisturbed.
//   5. The caller delivers the CR keystroke as normal. BASIC reads $FE40,
//      sees $0D = empty line, and loops back to the prompt cleanly.
static void screen_msg(const char *msg) {
    uint8_t row = m[0xFDF3];
    if (row >= 24) row = 23;

    uint16_t base = rowstart[row];
    int col;
    for (col = 0; col < 40 && msg[col]; col++)
        m[base + col] = (uint8_t) msg[col];
    for (; col < 40; col++)
        m[base + col] = 0x20;

    m[0xFE40] = 0x0D;
    m[0xFDF4] = 0;
    if (row < 23)
        m[0xFDF3] = row + 1;
}

// Extract the filename argument from a command line like "ESAVE foo" or
// 'ELOAD "myfile"'. Writes result into fname[flen], adds .BAC if no ext.
// Returns false if no filename found.
static bool extract_filename(const char *line, int skip, char *fname, int flen) {
    const char *p = line + skip;
    while (*p == ' ') p++;
    if (*p == '"' || *p == 39) p++;   // strip leading quote (39 = ')
    char *out = fname;
    char *end_out = fname + flen - 1;
    while (*p && *p != '"' && *p != 39 && *p != ' ' && out < end_out)
        *out++ = *p++;
    *out = 0;
    if (fname[0] == 0) return false;
    if (!strchr(fname, '.'))
        strncat(fname, ".BAC", (size_t)(flen - (int)strlen(fname) - 1));
    return true;
}

static bool try_file_command(void) {
    char line[48];
    read_screen_line(line, sizeof(line));

    bool is_save  = (strncasecmp(line, "ESAVE", 5) == 0);
    bool is_load  = (strncasecmp(line, "ELOAD", 5) == 0);
    bool is_list  = (strncasecmp(line, "ELIB", 5) == 0);

    if (!is_save && !is_load && !is_list)
        return false;

    uint16_t prog_start = (uint16_t)(m[0xFE1C] | (m[0xFE1D] << 8));
    uint16_t prog_end   = (uint16_t)(m[0xFE1E] | (m[0xFE1F] << 8));

    if (is_list) {
        // List .BAC files in the current directory on the next screen row.
        // We write filenames separated by spaces across one row (40 chars).
        char listbuf[41] = {0};
        int pos = 0;
        DIR *dir = opendir(".");
        if (dir) {
            struct dirent *ent;
            while ((ent = readdir(dir)) != NULL && pos < 38) {
                char *dot = strrchr(ent->d_name, '.');
                if (dot && strcasecmp(dot, ".BAC") == 0) {
                    int nlen = (int)(dot - ent->d_name);
                    if (pos + nlen + 1 < 40) {
                        if (pos > 0) listbuf[pos++] = ' ';
                        memcpy(listbuf + pos, ent->d_name, (size_t) nlen);
                        pos += nlen;
                    }
                }
            }
            closedir(dir);
        }
        screen_msg(pos > 0 ? listbuf : "NO FILES");
        return true;
    }

    char fname[40];
    if (!extract_filename(line, 5, fname, sizeof(fname))) {
        screen_msg("FILENAME MISSING");
        return true;
    }

    if (is_save) {
        if (prog_end <= prog_start) {
            screen_msg("NOTHING TO SAVE");
            return true;
        }
        FILE *f = fopen(fname, "wb");
        if (!f) { screen_msg("CANNOT CREATE FILE"); return true; }
        fwrite(m + prog_start, 1, prog_end - prog_start, f);
        fclose(f);
        char msg[48];
        snprintf(msg, sizeof(msg), "SAVED %u BYTES", (unsigned)(prog_end - prog_start));
        screen_msg(msg);

    } else {
        FILE *f = fopen(fname, "rb");
        if (!f) { screen_msg("FILE NOT FOUND"); return true; }
        size_t n = fread(m + prog_start, 1, 0x7BFF - prog_start, f);
        fclose(f);

        uint16_t new_end = prog_start + (uint16_t) n;

        // Write the end-of-program marker. LIST scans for $01 at the start
        // of each "line" entry to know when the program ends. Without this
        // byte LIST walks off into garbage RAM and hangs.
        m[new_end] = 0x01;

        // Update the program-end pointer.
        m[0xFE1E] = new_end & 0xFF;
        m[0xFE1F] = new_end >> 8;

        // Reset the variable area to empty, starting one past prog_end
        // (matching what BASIC's own NEW (or clear) command does).
        uint16_t var_start = new_end + 1;
        m[0xFE20] = var_start & 0xFF;
        m[0xFE21] = var_start >> 8;

        // Clear the remaining interpreter state that NEW resets, so that
        // RUN works cleanly after ELOAD without stale FOR/GOSUB context.
        m[0xFE0C] = 0x82;
        m[0xFE25] = 0x00;
        m[0xFE29] = 0x00;
        m[0xFE2A] = 0x00;
        m[0xFE33] = 0x00;
        m[0xFE35] = 0x00;
        m[0xFE38] = 0x00;

        char msg[48];
        snprintf(msg, sizeof(msg), "LOADED %u BYTES", (unsigned) n);
        screen_msg(msg);
    }

    return true;
}

static void keyboard_poll(void) {
    if (pending_key != 0)
        return;

    uint8_t ch = read_key();
    if (ch == 0)
        return;

    // On CR, check for one of our file commands. screen_msg() will have
    // already rewritten $FE40 to a bare $0D (empty line) and advanced
    // $FDF3 by one row, so BASIC will see a blank CR and loop back to
    // its prompt cleanly -- without executing the original command text.
    if (ch == 0x0D)
        try_file_command();

    pending_key = ch | 0x80;
    key_reads   = 2;

    // Pre-write the ISR latch so the fast path (BIT 7,(ix+2)) in the
    // BASIC input loop fires immediately after the next interrupt.
    m[0xFDF5] = pending_key;
}

u8 port_in(u8 port) {
    // INP(56)
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

    // PIO B: all lines idle-high = no device connected.
    // Returning 0 would hold every line low, locking up the bus handshake.
    // INP(58)
    if (port == IEC_PORT)
        return 0xFF;

    return 0;
}

void port_out(u8 port, u8 val) {
    if (port != 6) return;
    // ROM writes to port 6 for sound:
    //   $00 = silence/reset  (first half of BEL sequence, ignore)
    //   $83 = BEL tone       (second half of BEL: OUT 6,$00 then OUT 6,$83)
    //   $80 = break/STOP sound (keyboard ISR when Ctrl-C/break is detected)
    if (val == 0x83 || val == 0x80)
        write(STDOUT_FILENO, "\a", 1);
}

// Map ABC80 character codes to printable UTF-8 for screen display.
// The screen stores raw ABC80 codes; we translate on the way out.
static const char * abc80_to_utf8(uint8_t c) {
    switch (c & 0x7F) {
        case 0x24: return "\xC2\xA4";  // ¤  (currency/"sun" sign)
        case 0x5B: return "\xC3\x84";  // Ä
        case 0x5C: return "\xC3\x96";  // Ö
        case 0x5D: return "\xC3\x85";  // Å
        case 0x5E: return "\xC3\x9C";  // Ü
        case 0x7B: return "\xC3\xA4";  // ä
        case 0x7C: return "\xC3\xB6";  // ö
        case 0x7D: return "\xC3\xA5";  // å
        case 0x7E: return "\xC3\xBC";  // ü
        case 0x60: return "\xC3\xA9";  // é
        default:   return NULL;        // plain ASCII, use mvaddch
    }
}

static void screen_refresh(void) {
    for (int row = 0; row < 24; row++) {
        for (int col = 0; col < 40; col++) {
            uint8_t cell = m[rowstart[row] + col];

            if (cell & 0x80) {
                attron(A_BLINK);
                mvaddch(row, col, '_');
                attroff(A_BLINK);
                continue;
            }

            const char *utf8 = abc80_to_utf8(cell);
            if (utf8) {
                mvprintw(row, col, "%s", utf8);
            } else {
                mvaddch(row, col, cell & 0x7F);
            }
        }
    }
    refresh();
}

// Signal handler for Ctrl-C: sets the ABC80 break flag.
// The main BASIC interpreter loop checks m[0xFE07] between every
// statement and jumps to the break handler when it is non-zero.
static void ctrlc_handler(int sig) {
    (void) sig;
    m[0xFE07] = 0x83;
}

static void run(void) {
    setlocale(LC_ALL, "");

    // Catch SIGINT (Ctrl-C) and convert it to an ABC80 break.
    // We use a signal handler rather than raw bytes because cbreak()
    // leaves terminal ISIG enabled (needed so Ctrl-Z / SIGTSTP works).
    signal(SIGINT, ctrlc_handler);

    init();
    memcpy(m, abcprom, sizeof(abcprom));

    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, true);
    nonl();
    keypad(stdscr, true);   // let ncurses decode arrow keys into KEY_LEFT/KEY_RIGHT

    pc = 0x0000;

    long long    strobe_ns   = 1000000000LL / STROBE_HZ;
    long long    next_strobe = now_ns() + strobe_ns;
    unsigned int steps       = 0;

    while (!done) {
        step();
        steps++;

        keyboard_poll();

        if (steps >= STEPS_PER_STROBE) {
            steps = 0;

            long long now = now_ns();
            if (now >= next_strobe) {
                next_strobe = now + strobe_ns;
                gen_int(KB_INT_VEC);
            }
        }

        if (steps % STEPS_PER_REFRESH == 0)
            screen_refresh();
    }

    screen_refresh();
    endwin();
    putchar('\n');
}

int main(void) {
    run();
}

