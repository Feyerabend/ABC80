// ABC80 built-in monitor — entered with Button X, exited with X/Q.
// Amber display distinguishes monitor mode from normal ABC80 operation.
// The Z80 is frozen while the monitor is active.

#include "pico/stdlib.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#include "display.h"
#include "abc80.h"
#include "abc80errors.h"
#include "monitor.h"

#define MON_FG      0x67EC          // amber
#define MON_BG      COLOR_BLACK

#define MON_OUT_ROWS  20

static bool     mon_active  = false;
static char     mon_out[MON_OUT_ROWS][41];
static int      mon_nout    = 0;
static char     mon_line[41];
static int      mon_linelen = 0;
static uint16_t mon_addr    = 0;

// ---------------------------------------------------------------------------
// Internal helpers

static void mon_print(const char *s) {
    if (mon_nout < MON_OUT_ROWS) {
        strncpy(mon_out[mon_nout++], s, 40);
        mon_out[mon_nout - 1][40] = '\0';
    } else {
        memmove(mon_out[0], mon_out[1], (MON_OUT_ROWS - 1) * 41);
        strncpy(mon_out[MON_OUT_ROWS - 1], s, 40);
        mon_out[MON_OUT_ROWS - 1][40] = '\0';
    }
}

static uint16_t parse_hex(const char *s) {
    uint16_t v = 0;
    while (*s) {
        char c = *s++;
        if      (c >= '0' && c <= '9') v = (v << 4) | (uint16_t)(c - '0');
        else if (c >= 'A' && c <= 'F') v = (v << 4) | (uint16_t)(c - 'A' + 10);
        else if (c >= 'a' && c <= 'f') v = (v << 4) | (uint16_t)(c - 'a' + 10);
        else break;
    }
    return v;
}

static uint16_t mon_read16(uint16_t addr) {
    return (uint16_t)abc80_read_mem(addr) |
           ((uint16_t)abc80_read_mem((uint16_t)(addr + 1)) << 8);
}

// Decode ABC80 5-byte BCD float: [S][S][S][I][E]
//   S = byte of 2 BCD digits (mantissa = 0.dddddd)
//   I = sign byte (0 = positive, non-zero = negative)
//   E = decimal exponent + 128
static void decode_float(const uint8_t *b, char *out, int outlen) {
    int d[6];
    for (int i = 0; i < 3; i++) {
        d[i*2]   = (b[i] >> 4) & 0x0F;
        d[i*2+1] =  b[i]       & 0x0F;
    }
    int neg = (b[3] != 0);
    int exp = (int)b[4] - 128;

    int nonzero = 0;
    for (int i = 0; i < 6; i++) nonzero |= d[i];
    if (!nonzero) { out[0] = '0'; out[1] = '\0'; return; }

    int pos = 0;
    if (neg) out[pos++] = '-';

    if (exp <= 0) {
        out[pos++] = '0'; out[pos++] = '.';
        for (int i = 0; i < -exp && pos < outlen-3; i++) out[pos++] = '0';
        for (int i = 0; i < 6   && pos < outlen-2; i++) out[pos++] = (char)('0' + d[i]);
    } else if (exp >= 6) {
        for (int i = 0; i < 6   && pos < outlen-3; i++) out[pos++] = (char)('0' + d[i]);
        for (int i = 6; i < exp && pos < outlen-2; i++) out[pos++] = '0';
    } else {
        for (int i = 0; i < exp && pos < outlen-3; i++) out[pos++] = (char)('0' + d[i]);
        out[pos++] = '.';
        for (int i = exp; i < 6 && pos < outlen-2; i++) out[pos++] = (char)('0' + d[i]);
    }
    out[pos] = '\0';

    char *dot = (char *)0;
    for (char *p = out; *p; p++) if (*p == '.') { dot = p; break; }
    if (dot) {
        char *end = out + pos - 1;
        while (end > dot && *end == '0') end--;
        if (*end == '.') end--;
        *(end + 1) = '\0';
    }
}

// ---------------------------------------------------------------------------
// Command execution

static void mon_exec(const char *cmd) {
    while (*cmd == ' ') cmd++;
    char ch = *cmd;
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);

    if (ch == 'D') {
        cmd++;
        while (*cmd == ' ') cmd++;
        if (*cmd) mon_addr = parse_hex(cmd);
        for (int i = 0; i < 8; i++, mon_addr += 8) {
            char line[41];
            char ascii[9];
            int  pos = 0;
            pos += snprintf(line + pos, (int)sizeof(line) - pos, "%04X", mon_addr);
            for (int j = 0; j < 8; j++) {
                uint8_t byt = abc80_read_mem((uint16_t)(mon_addr + j));
                pos += snprintf(line + pos, (int)sizeof(line) - pos, " %02X", byt);
                ascii[j] = (byt >= 0x20 && byt < 0x7F) ? (char)byt : '.';
            }
            ascii[8] = '\0';
            snprintf(line + pos, (int)sizeof(line) - pos, " %s", ascii);
            mon_print(line);
        }
    } else if (ch == 'R') {
        abc80_regs_t r;
        abc80_get_regs(&r);
        char line[41];
        snprintf(line, sizeof(line), "A=%02X F=%02X BC=%02X%02X DE=%02X%02X HL=%02X%02X",
                 r.a, r.f, r.b, r.c, r.d, r.e, r.h, r.l);
        mon_print(line);
        snprintf(line, sizeof(line), "PC=%04X SP=%04X IX=%04X IY=%04X IM=%d",
                 r.pc, r.sp, r.ix, r.iy, r.im);
        mon_print(line);
        snprintf(line, sizeof(line), "F: %c%c.%c.%c%c%c  IFF=%d%d",
                 (r.f & 0x80) ? 'S' : '-', (r.f & 0x40) ? 'Z' : '-',
                 (r.f & 0x10) ? 'H' : '-',
                 (r.f & 0x04) ? 'P' : '-', (r.f & 0x02) ? 'N' : '-',
                 (r.f & 0x01) ? 'C' : '-', (int)r.iff1, (int)r.iff2);
        mon_print(line);
    } else if (ch == 'S') {
        uint16_t bofa    = mon_read16(0xFE1C);
        uint16_t eofa    = mon_read16(0xFE1E);
        uint16_t heap    = mon_read16(0xFE20);
        uint16_t sp_ini  = mon_read16(0xFE27);
        uint16_t varroot = mon_read16(0xFE29);
        uint8_t  crow    = abc80_read_mem(0xFDF3);
        uint8_t  ccol    = abc80_read_mem(0xFDF4);
        char line[41];
        mon_print(". BASIC status .");
        snprintf(line, sizeof(line), "BOFA=%04X  EOFA=%04X", bofa, eofa);
        mon_print(line);
        snprintf(line, sizeof(line), "HEAP=%04X  SPINI=%04X", heap, sp_ini);
        mon_print(line);
        snprintf(line, sizeof(line), "VARS=%04X", varroot);
        mon_print(line);
        snprintf(line, sizeof(line), "PRG=%d bytes  FREE=%d bytes",
                 (int)(eofa - bofa), (int)(sp_ini - heap));
        mon_print(line);
        snprintf(line, sizeof(line), "CURSOR row=%d col=%d", crow, ccol);
        mon_print(line);

    } else if (ch == 'V') {
        // Walk the BASIC symbol table (linked list).
        // VARIABELROTEN at 0xFE29 = pointer to first record (0 = no variables).
        // Each record: [type_byte][name_byte][next_ptr(2 LE)][value...]
        //   type upper nibble: digit suffix 0-9, F=none
        //   type lower nibble: 0=float 1=int 2=string
        //                      4=float[] 5=int[] 6=str[]
        //                      8=float[,] 9=int[,] A=str[,]
        //   name: raw ABC80 char 0x41('A')..0x5A('Z'),0x5B(Ä),0x5C(Ö),0x5D(Å)
        //   float  (offset+4): 5 bytes SSSIE — BCD×3, sign, exp+128
        //   int    (offset+4): 2 bytes LE
        //   string (offset+4): DD LL RR (dim_len, cur_len, content_ptr, each 2B)
        uint16_t root = mon_read16(0xFE29);
        char line[41];
        snprintf(line, sizeof(line), "VARS root=%04X", root);
        mon_print(line);
        int count = 0;
        if (root == 0) {
            mon_print("(no variables)");
        } else {
            uint16_t addr = root;
            while (addr != 0 && count < 28) {
                uint8_t  typ  = abc80_read_mem(addr);
                uint8_t  naam = abc80_read_mem((uint16_t)(addr + 1));
                uint16_t next = mon_read16((uint16_t)(addr + 2));
                uint8_t  tc   = typ & 0x0F;

                if (naam < 0x41 || naam > 0x5D ||
                        tc == 3 || tc == 7 || tc > 10) {
                    snprintf(line, sizeof(line), "%04X ?? typ=%02X nam=%02X",
                             addr, typ, naam);
                    mon_print(line);
                    break;
                }

                char    nc    = (char)naam;
                uint8_t suf   = (typ >> 4) & 0x0F;
                char    suf_c = (suf <= 9) ? (char)('0' + suf) : '\0';

                const char *tlabel, *bsuf;
                switch (tc) {
                    case 0:  tlabel="REAL   "; bsuf="";     break;
                    case 1:  tlabel="INT    "; bsuf="%";    break;
                    case 2:  tlabel="STRING "; bsuf="$";    break;
                    case 4:  tlabel="REAL() "; bsuf="()";   break;
                    case 5:  tlabel="INT()  "; bsuf="%()";  break;
                    case 6:  tlabel="ST()  "; bsuf="$()";  break;
                    case 8:  tlabel="REAL(,)"; bsuf="(,)";  break;
                    case 9:  tlabel="INT(,) "; bsuf="%(,)"; break;
                    case 10: tlabel="STR(,) "; bsuf="$(,)"; break;
                    default: tlabel="?      "; bsuf="?";    break;
                }

                char namn[10]; int np = 0;
                namn[np++] = nc;
                if (suf_c) namn[np++] = suf_c;
                for (const char *p = bsuf; *p; ) namn[np++] = *p++;
                namn[np] = '\0';

                char val[22]; val[0] = '\0';
                if (tc == 1) {
                    int16_t iv = (int16_t)mon_read16((uint16_t)(addr + 4));
                    snprintf(val, sizeof(val), "%d", iv);
                } else if (tc == 2) {
                    // String descriptor at offset+4: DD(2) RR(2) LL(2)
                    //   DD = dimensioned (max) length
                    //   RR = pointer to string content in heap
                    //   LL = actual current length
                    uint16_t rr = mon_read16((uint16_t)(addr + 6));
                    uint16_t ll = mon_read16((uint16_t)(addr + 8));
                    int show = ((int)ll > 14) ? 14 : (int)ll;
                    int p = 0;
                    val[p++] = '"';
                    for (int i = 0; i < show; i++) {
                        uint8_t sc = abc80_read_mem((uint16_t)(rr + i));
                        val[p++] = (sc >= 0x20 && sc <= 0x7F) ? (char)sc : '.';
                    }
                    val[p++] = '"';
                    if ((int)ll > 14) { val[p++] = '.'; val[p++] = '.'; }
                    val[p] = '\0';
                } else if (tc == 0) {
                    uint8_t fb[5];
                    for (int i = 0; i < 5; i++)
                        fb[i] = abc80_read_mem((uint16_t)(addr + 4 + i));
                    decode_float(fb, val, (int)sizeof(val));
                } else {
                    // Array/matrix descriptor: AA(2) MM(2) [XX(2) YY(2)]
                    //   AA = pointer to element data
                    //   MM = total number of elements
                    uint16_t aa = mon_read16((uint16_t)(addr + 4));
                    uint16_t mm = mon_read16((uint16_t)(addr + 6));
                    int nelm = (mm > 4) ? 4 : (int)mm;
                    int vp = snprintf(val, sizeof(val), "(%d)", mm);
                    // tc 4,8 = float arrays; tc 5,9 = int arrays
                    int is_float = (tc == 4 || tc == 8);
                    int is_int   = (tc == 5 || tc == 9);
                    for (int i = 0; i < nelm && vp < 18; i++) {
                        val[vp++] = ' ';
                        if (is_float) {
                            uint8_t fb[5];
                            for (int j = 0; j < 5; j++)
                                fb[j] = abc80_read_mem((uint16_t)(aa + i*5 + j));
                            char ev[10];
                            decode_float(fb, ev, (int)sizeof(ev));
                            int rem = (int)sizeof(val) - vp - 1;
                            int n = 0;
                            while (ev[n] && rem-- > 0) val[vp++] = ev[n++];
                        } else if (is_int) {
                            int16_t iv = (int16_t)mon_read16(
                                             (uint16_t)(aa + (uint16_t)(i * 2)));
                            char ev[8];
                            int n = snprintf(ev, sizeof(ev), "%d", iv);
                            int rem = (int)sizeof(val) - vp - 1;
                            for (int k = 0; k < n && rem-- > 0; k++)
                                val[vp++] = ev[k];
                        } else {
                            // string array: just show pointer for now
                            break;
                        }
                    }
                    if (mm > 4) { val[vp++] = '.'; val[vp++] = '.'; }
                    val[vp] = '\0';
                }

                snprintf(line, sizeof(line), "%04X %-7s%-7s%s",
                         addr, namn, tlabel, val);
                mon_print(line);

                addr = next;
                count++;
            }
            if (count == 0) mon_print("(no variables)");
        }

    } else if (ch == '?' || ch == 'H') {
        cmd++;
        while (*cmd == ' ') cmd++;
        if (ch == '?' && *cmd >= '0' && *cmd <= '9') {
            uint8_t n = 0;
            while (*cmd >= '0' && *cmd <= '9')
                n = (uint8_t)(n * 10 + (*cmd++ - '0'));
            const char *emsg = abc80_error_msg(n);
            char line[41];
            if (emsg)
                snprintf(line, sizeof(line), "%2d: %s", n, emsg);
            else
                snprintf(line, sizeof(line), "%2d: (felkod ej funnen)", n);
            mon_print(line);
        } else {
            mon_print("D (addr)  hex dump 64 bytes");
            mon_print("R         Z80 register");
            mon_print("S         BASIC status/minne");
            mon_print("V         variable list");
            mon_print("? N       Swedish: finn felkod N");
            mon_print("Q         quit monitor");
        }
    } else if (ch != '\0') {
        mon_print("unknown command");
    }
}

// ---------------------------------------------------------------------------
// Public API

bool monitor_is_active(void) {
    return mon_active;
}

void monitor_enter(void) {
    mon_active  = true;
    mon_nout    = 0;
    mon_linelen = 0;
    mon_line[0] = '\0';
    memset(mon_out, 0, sizeof(mon_out));
    abc80_regs_t r;
    abc80_get_regs(&r);
    mon_addr = r.pc;
    mon_print("ABC80 MONITOR  ?=HELP  Q=EXIT");
    char info[41];
    snprintf(info, sizeof(info), "Stopped at PC=%04X SP=%04X", r.pc, r.sp);
    mon_print(info);
}

void monitor_exit(void) {
    mon_active = false;
}

void monitor_serial_poll(void) {
    int ch = getchar_timeout_us(0);
    if (ch == PICO_ERROR_TIMEOUT) return;

    if (ch == 0x0D || ch == 0x0A) {
        char echo[44];
        snprintf(echo, sizeof(echo), "> %s", mon_line);
        mon_print(echo);
        char c0 = mon_line[0];
        if (c0 >= 'a' && c0 <= 'z') c0 = (char)(c0 - 32);
        if (c0 == 'Q' || c0 == 'X') {
            monitor_exit();
        } else {
            mon_exec(mon_line);
        }
        mon_line[0] = '\0';
        mon_linelen = 0;
    } else if ((ch == 0x7F || ch == 0x08) && mon_linelen > 0) {
        mon_line[--mon_linelen] = '\0';
    } else if (ch >= 0x20 && ch <= 0x7E && mon_linelen < 38) {
        mon_line[mon_linelen++] = (char)ch;
        mon_line[mon_linelen]   = '\0';
    }
}

void monitor_render(uint16_t *fb) {
    fb_clear(fb, MON_BG);

    // Title bar: amber background, black text
    fb_fill_rect(fb, 0, 1, DISPLAY_WIDTH, 9, MON_FG);
    fb_draw_string(fb, 0, 0, " ABC80 MONITOR", COLOR_BLACK, MON_FG);
    fb_draw_string(fb, 24 * 8, 0, "X/Q=EXIT ?=HELP", COLOR_BLACK, MON_FG);

    for (int i = 0; i < mon_nout; i++)
        fb_draw_string(fb, 0, (i + 2) * 10, mon_out[i], MON_FG, MON_BG);

    char prompt[44];
    snprintf(prompt, sizeof(prompt), "> %s_", mon_line);
    fb_draw_string(fb, 0, 23 * 10, prompt, MON_FG, MON_BG);
}
