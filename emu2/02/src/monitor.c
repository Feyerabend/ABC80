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
#include "disasm.h"
#include "monitor.h"
#include "z80asm.h"
#include "snake_asm.h"
#include "wifi_client.h"

#define MON_FG      0x67EC          // amber
#define MON_BG      COLOR_BLACK

#define MON_OUT_ROWS  20

// ---------------------------------------------------------------------------
// Assembly source — line-numbered editor, like ABC80 BASIC.
//
// "A 10 LD HL,0"  — set line 10 to "LD HL,0"
// "A 10"          — delete line 10
// "AL"            — list all lines
// "AL 10 30"      — list lines 10–30
// "AC"            — clear all lines
// "AS addr"       — assemble to Z80 address
//
// Lines are kept sorted by line number at all times.
#define ASM_MAX_LINES  512
#define ASM_LINE_LEN    72   // fits comfortably in 40-col display

typedef struct { uint16_t num; char text[ASM_LINE_LEN]; } AsmLine;
static AsmLine asm_lines[ASM_MAX_LINES];
static int     asm_nlines = 0;

static bool     mon_active      = false;
static char     mon_out[MON_OUT_ROWS][41];
static int      mon_nout        = 0;
static char     mon_line[80];
static int      mon_linelen     = 0;
static uint16_t mon_addr        = 0;

// ---------------------------------------------------------------------------
// Output and parsing helpers

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
// Command handlers — each receives the argument string (trimmed, after cmd char)

static void cmd_dump(const char *args) {
    if (*args) mon_addr = parse_hex(args);
    for (int i = 0; i < 8; i++, mon_addr += 8) {
        char line[41], ascii[9];
        int  pos = 0;
        pos += snprintf(line + pos, (int)sizeof(line) - pos, "%04X", mon_addr);
        for (int j = 0; j < 8; j++) {
            uint8_t b = abc80_read_mem((uint16_t)(mon_addr + j));
            pos += snprintf(line + pos, (int)sizeof(line) - pos, " %02X", b);
            ascii[j] = (b >= 0x20 && b < 0x7F) ? (char)b : '.';
        }
        ascii[8] = '\0';
        snprintf(line + pos, (int)sizeof(line) - pos, " %s", ascii);
        mon_print(line);
    }
}

static void cmd_unasm(const char *args) {
    if (*args) mon_addr = parse_hex(args);
    for (int i = 0; i < 16; i++) {
        char mnem[32], hex[13], line[41];
        int len = z80_disasm(mon_addr, mnem, (int)sizeof(mnem));
        int hp = 0;
        for (int j = 0; j < len && j < 4; j++)
            hp += snprintf(hex+hp, (int)sizeof(hex)-hp, "%02X ",
                           abc80_read_mem((uint16_t)(mon_addr+j)));
        while (hp < 12) hex[hp++] = ' ';
        hex[12] = '\0';
        snprintf(line, sizeof(line), "%04X %s%s", mon_addr, hex, mnem);
        mon_print(line);
        mon_addr = (uint16_t)(mon_addr + len);
    }
}

static void cmd_regs(const char *args) {
    (void)args;
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
}

static void cmd_status(const char *args) {
    (void)args;
    uint16_t bofa    = mon_read16(0xFE1C);
    uint16_t eofa    = mon_read16(0xFE1E);
    uint16_t heap    = mon_read16(0xFE20);
    uint16_t sp_ini  = mon_read16(0xFE27);
    uint16_t varroot = mon_read16(0xFE29);
    uint8_t  crow    = abc80_read_mem(0xFDF3);
    uint8_t  ccol    = abc80_read_mem(0xFDF4);
    char line[41];
    mon_print(". BASIC status .");
    snprintf(line, sizeof(line), "BOFA=%04X  EOFA=%04X", bofa, eofa);   mon_print(line);
    snprintf(line, sizeof(line), "HEAP=%04X  SPINI=%04X", heap, sp_ini); mon_print(line);
    snprintf(line, sizeof(line), "VARS=%04X", varroot);                  mon_print(line);
    snprintf(line, sizeof(line), "PRG=%d bytes  FREE=%d bytes",
             (int)(eofa - bofa), (int)(sp_ini - heap));                  mon_print(line);
    snprintf(line, sizeof(line), "CURSOR row=%d col=%d", crow, ccol);    mon_print(line);
}

static void cmd_vars(const char *args) {
    (void)args;
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

    if (root == 0) { mon_print("(no variables)"); return; }

    int count = 0;
    uint16_t addr = root;
    while (addr != 0 && count < 28) {
        uint8_t  typ  = abc80_read_mem(addr);
        uint8_t  naam = abc80_read_mem((uint16_t)(addr + 1));
        uint16_t next = mon_read16((uint16_t)(addr + 2));
        uint8_t  tc   = typ & 0x0F;

        if (naam < 0x41 || naam > 0x5D || tc == 3 || tc == 7 || tc > 10) {
            snprintf(line, sizeof(line), "%04X ?? typ=%02X nam=%02X", addr, typ, naam);
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
            case 6:  tlabel="ST()   "; bsuf="$()";  break;
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
                    for (int n = 0; ev[n] && rem-- > 0; n++) val[vp++] = ev[n];
                } else if (is_int) {
                    int16_t iv = (int16_t)mon_read16((uint16_t)(aa + (uint16_t)(i * 2)));
                    char ev[8];
                    int n = snprintf(ev, sizeof(ev), "%d", iv);
                    int rem = (int)sizeof(val) - vp - 1;
                    for (int k = 0; k < n && rem-- > 0; k++) val[vp++] = ev[k];
                } else {
                    break;  // string arrays: show pointer only
                }
            }
            if (mm > 4) { val[vp++] = '.'; val[vp++] = '.'; }
            val[vp] = '\0';
        }

        snprintf(line, sizeof(line), "%04X %-7s%-7s%s", addr, namn, tlabel, val);
        mon_print(line);
        addr = next;
        count++;
    }
    if (count == 0) mon_print("(no variables)");
}

static void cmd_devices(const char *args) {
    (void)args;
    // Walk enhetslistan (device list).
    // Head pointer at RAM 0xFE0A (16-bit LE).
    // Each entry (7 bytes): next(2) name[3] handler_table(2)
    uint16_t ptr = mon_read16(0xFE0A);
    char line[41];
    snprintf(line, sizeof(line), "ENHET head=FE0A -> %04X", ptr);
    mon_print(line);
    int count = 0;
    while (ptr != 0 && count < 16) {
        uint16_t next   = mon_read16(ptr);
        uint8_t  n0     = abc80_read_mem((uint16_t)(ptr + 2));
        uint8_t  n1     = abc80_read_mem((uint16_t)(ptr + 3));
        uint8_t  n2     = abc80_read_mem((uint16_t)(ptr + 4));
        uint16_t htable = mon_read16((uint16_t)(ptr + 5));
        char nm[4];
        nm[0] = (n0 >= 0x20 && n0 < 0x7F) ? (char)n0 : '?';
        nm[1] = (n1 >= 0x20 && n1 < 0x7F) ? (char)n1 : '?';
        nm[2] = (n2 >= 0x20 && n2 < 0x7F) ? (char)n2 : '?';
        nm[3] = '\0';
        snprintf(line, sizeof(line), "%04X  %s:  htbl=%04X next=%04X",
                 ptr, nm, htable, next);
        mon_print(line);
        ptr = next;
        count++;
    }
    if (count == 0) mon_print("(empty list)");
}

static void cmd_error(const char *args) {
    if (*args >= '0' && *args <= '9') {
        uint8_t n = 0;
        while (*args >= '0' && *args <= '9')
            n = (uint8_t)(n * 10 + (*args++ - '0'));
        const char *emsg = abc80_error_msg(n);
        char line[41];
        if (emsg) snprintf(line, sizeof(line), "%2d: %s", n, emsg);
        else      snprintf(line, sizeof(line), "%2d: (felkod ej funnen)", n);
        mon_print(line);
    } else {
        mon_print("usage: ? <error-number>");
    }
}

// ---------------------------------------------------------------------------
// Assembler commands — line-numbered editor (no modes)

// Callback: route assembler listing/error lines through mon_print.
static void asm_print(const char *s) { mon_print(s); }

// Parse an unsigned decimal integer from *p, advance *p past digits.
// Returns -1 if no digits found.
static int parse_dec(const char **p) {
    if (**p < '0' || **p > '9') return -1;
    int v = 0;
    while (**p >= '0' && **p <= '9') v = v * 10 + (*(*p)++ - '0');
    return v;
}

// Find index of line number num, or -1 if not present.
static int asm_find(uint16_t num) {
    for (int i = 0; i < asm_nlines; i++)
        if (asm_lines[i].num == num) return i;
    return -1;
}

// A [num [text]] — line editor entry point.
//   A 10 LD A,42   → set line 10
//   A 10           → delete line 10
//   A              → show line count
static void cmd_asm_line(const char *args) {
    while (*args == ' ') args++;
    if (*args < '0' || *args > '9') {
        char buf[41];
        snprintf(buf, sizeof(buf), "%d line(s) - AL (n)=list AC=clear AS=asm",
                 asm_nlines);
        mon_print(buf);
        return;
    }

    int num = parse_dec(&args);
    if (num < 0 || num > 9999) { mon_print("line number 0-9999"); return; }
    while (*args == ' ') args++;

    int idx = asm_find((uint16_t)num);

    if (*args == '\0') {
        // Delete line num (if it exists)
        if (idx < 0) { mon_print("(not found)"); return; }
        memmove(&asm_lines[idx], &asm_lines[idx + 1],
                (size_t)(asm_nlines - idx - 1) * sizeof(AsmLine));
        asm_nlines--;
        return;
    }

    // Set/replace line num
    if (idx < 0) {
        // Insert in sorted position
        if (asm_nlines >= ASM_MAX_LINES) { mon_print("source full"); return; }
        int ins = asm_nlines;
        for (int i = 0; i < asm_nlines; i++) {
            if (asm_lines[i].num > (uint16_t)num) { ins = i; break; }
        }
        memmove(&asm_lines[ins + 1], &asm_lines[ins],
                (size_t)(asm_nlines - ins) * sizeof(AsmLine));
        asm_nlines++;
        idx = ins;
    }
    asm_lines[idx].num = (uint16_t)num;
    snprintf(asm_lines[idx].text, ASM_LINE_LEN, "%s", args);
}

// AC — clear all lines.
static void cmd_asm_clear(const char *args) {
    (void)args;
    asm_nlines = 0;
    mon_print("source cleared");
}


// AL [from [to]] — list lines; optional decimal range.
static void cmd_asm_list(const char *args) {
    if (asm_nlines == 0) { mon_print("(empty)"); return; }
    while (*args == ' ') args++;
    int from = 0, to = 99999;
    if (*args >= '0' && *args <= '9') {
        from = parse_dec(&args);
        while (*args == ' ') args++;
        if (*args >= '0' && *args <= '9') to = parse_dec(&args);
        // single number: list from that line to the end
    }
    int shown = 0;
    for (int i = 0; i < asm_nlines && shown < 16; i++) {
        int n = asm_lines[i].num;
        if (n < from || n > to) continue;
        char buf[41];
        snprintf(buf, sizeof(buf), "%4d %s", n, asm_lines[i].text);
        mon_print(buf);
        shown++;
    }
    if (shown == 0) mon_print("(no lines in range)");
}

// AS [addr] — assemble all lines to Z80 address (hex, default 8000).
static void cmd_asm_assemble(const char *args) {
    if (asm_nlines == 0) { mon_print("no source (use A n text)"); return; }
    while (*args == ' ') args++;
    uint16_t origin = 0x8000;
    if (*args) origin = parse_hex(args);

    // Build flat source buffer from numbered lines.
    static char src_buf[ASM_MAX_LINES * (ASM_LINE_LEN + 1)];
    int len = 0;
    for (int i = 0; i < asm_nlines; i++) {
        int n = (int)strlen(asm_lines[i].text);
        if (len + n + 1 >= (int)sizeof(src_buf)) break;
        memcpy(src_buf + len, asm_lines[i].text, (size_t)n);
        len += n;
        src_buf[len++] = '\n';
    }

    char info[41];
    snprintf(info, sizeof(info), "ASM %d lines -> %04X", asm_nlines, origin);
    mon_print(info);

    extern uint8_t m[];
    int rc = z80asm_assemble(src_buf, len, m, origin, asm_print, true);
    if (rc < 0)
        snprintf(info, sizeof(info), "failed (%d error(s))", -rc);
    else
        snprintf(info, sizeof(info), "ok: %d bytes at %04X-%04X", rc, origin,
                 (uint16_t)(origin + (uint16_t)rc - 1u));
    mon_print(info);
}

// G [addr] — set PC (hex) and resume Z80 execution.
static void cmd_go(const char *args) {
    if (*args) {
        extern uint16_t pc;
        pc = parse_hex(args);
    }
    monitor_exit();
}

// P — load the built-in SNAKE source into the ASM line editor.
// Skips blank lines and pure-comment lines to stay within ASM_MAX_LINES.
// After loading: AL=list  AS 8000=assemble  G 8000=run
// Controls: W/A/S/D=direction  Ctrl-C=quit
static void cmd_snake(const char *args) {
    (void)args;
    cmd_asm_clear(NULL);

    const char *p = snake_src;
    uint16_t linenum = 10;
    char linebuf[80];
    char tmp[96];

    while (*p && linenum <= 9990) {
        // Extract one source line
        int len = 0;
        while (*p && *p != '\n') {
            if (len < 75) linebuf[len++] = *p;
            p++;
        }
        if (*p == '\n') p++;
        linebuf[len] = '\0';

        // Skip blank lines and pure-comment lines
        const char *s = linebuf;
        while (*s == ' ' || *s == '\t') s++;
        if (*s == '\0' || *s == ';') continue;

        // Load into editor as "linenum text"
        snprintf(tmp, sizeof(tmp), "%u %s", (unsigned)linenum, linebuf);
        cmd_asm_line(tmp);
        linenum += 10;
    }

    char buf[41];
    snprintf(buf, sizeof(buf), "SNAKE: %d lines  (AS 8000 / G 8000)",
             asm_nlines);
    mon_print(buf);
    mon_print("W/A/S/D=direction  Ctrl-C=quit");
}

// ---------------------------------------------------------------------------
// File commands — F-family (talk to PicoFS over WiFi)
//
// FL [path]      list directory (default: root /)
// FD path        delete file or directory (recursive)
// FK path        create directory (mkdir)
// FM src dst     move / rename a file or directory
// FI             disk info + WiFi status

// Shared response buffer for HTTP bodies.
#define FMON_BUF_SIZE 2048
static uint8_t fmon_buf[FMON_BUF_SIZE];

// Simple JSON array scanner for /ls response: [{"n":"name","t":"f","s":1234},...]
// Prints each entry through mon_print.  No malloc, no full JSON parser.
//
// Approach: for each object {…} find the boundaries, then search for each
// field independently within those bounds.  This is order-independent and
// immune to name-length or key-order issues.
static void print_ls_response(const uint8_t *buf, uint16_t len) {
    const char *p   = (const char *)buf;
    const char *end = p + len;
    int count = 0;

    while (p < end) {
        // Find start of next object
        while (p < end && *p != '{') p++;
        if (p >= end) break;
        const char *obj = p;         // points to '{'

        // Find matching '}'  (objects from /ls are flat, no nesting)
        while (p < end && *p != '}') p++;
        if (p >= end) break;
        const char *clos = p;        // points to '}'
        p++;                         // advance past '}'

        char name[17] = {0};
        char type = '\0';
        int  size = -1;

        // --- extract "n":"..." ---
        const char *f = obj;
        while (f + 4 <= clos) {
            if (f[0]=='"' && f[1]=='n' && f[2]=='"' && f[3]==':') {
                f += 4;
                while (f < clos && (*f == ' ' || *f == '\t')) f++;
                if (f < clos && *f == '"') {
                    f++;
                    int vi = 0;
                    while (f < clos && *f != '"' && vi < 16) name[vi++] = *f++;
                    name[vi] = '\0';
                }
                break;
            }
            f++;
        }

        // --- extract "t":"d" or "t":"f" ---
        f = obj;
        while (f + 4 <= clos) {
            if (f[0]=='"' && f[1]=='t' && f[2]=='"' && f[3]==':') {
                f += 4;
                while (f < clos && (*f == ' ' || *f == '\t')) f++;
                if (f < clos && *f == '"' && f + 1 < clos) type = f[1];
                break;
            }
            f++;
        }

        // --- extract "s":digits ---
        f = obj;
        while (f + 4 <= clos) {
            if (f[0]=='"' && f[1]=='s' && f[2]=='"' && f[3]==':') {
                f += 4;
                while (f < clos && (*f == ' ' || *f == '\t')) f++;
                if (f < clos && *f >= '0' && *f <= '9') {
                    size = 0;
                    while (f < clos && *f >= '0' && *f <= '9')
                        size = size * 10 + (*f++ - '0');
                }
                break;
            }
            f++;
        }

        if (!*name) continue;
        char line[41];
        if (type == 'd') {
            snprintf(line, sizeof(line), "  %-13s <DIR>", name);
        } else if (size >= 0) {
            if (size < 1024)
                snprintf(line, sizeof(line), "  %-13s %d B", name, size);
            else if (size < 1048576)
                snprintf(line, sizeof(line), "  %-13s %d K", name, size / 1024);
            else
                snprintf(line, sizeof(line), "  %-13s %d M", name, size / 1048576);
        } else {
            snprintf(line, sizeof(line), "  %s", name);
        }
        mon_print(line);
        count++;
    }
    if (count == 0) mon_print("  (empty)");
}

// Build a percent-encoded query string path parameter into out (max outlen).
// Only encodes space → %20; all other printable ASCII passed through.
static void url_encode_path(const char *path, char *out, int outlen) {
    int i = 0;
    for (; *path && i < outlen - 3; path++) {
        if (*path == ' ') { out[i++] = '%'; out[i++] = '2'; out[i++] = '0'; }
        else              out[i++] = *path;
    }
    out[i] = '\0';
}

// FL [path] — list a directory.
static void cmd_file_list(const char *args) {
    if (!wifi_ready()) { mon_print("no WiFi"); return; }

    char enc[80];
    const char *path = (*args) ? args : "/";
    url_encode_path(path, enc, sizeof(enc));

    char qry[100];
    snprintf(qry, sizeof(qry), "/ls?path=%s", enc);

    uint16_t rlen = 0;
    int rc = http_get(qry, fmon_buf, FMON_BUF_SIZE - 1, &rlen);
    if (rc == HTTP_OK) {
        fmon_buf[rlen] = '\0';
        char hdr[41];
        snprintf(hdr, sizeof(hdr), "DIR %s", path);
        mon_print(hdr);
        print_ls_response(fmon_buf, rlen);
    } else if (rc == HTTP_NOT_FOUND) {
        mon_print("not found");
    } else {
        char msg[41];
        snprintf(msg, sizeof(msg), "error %d", rc);
        mon_print(msg);
    }
}

// FD path — delete file or directory (server does recursive rmdir).
static void cmd_file_delete(const char *args) {
    if (!*args) { mon_print("usage: FD path"); return; }
    if (!wifi_ready()) { mon_print("no WiFi"); return; }

    char enc[80];
    url_encode_path(args, enc, sizeof(enc));
    char qry[100];
    snprintf(qry, sizeof(qry), "/rm?path=%s", enc);

    int rc = http_delete(qry);
    if (rc == HTTP_OK)        mon_print("deleted");
    else if (rc == HTTP_NOT_FOUND) mon_print("not found");
    else { char m[41]; snprintf(m, sizeof(m), "error %d", rc); mon_print(m); }
}

// FK path — create directory.
static void cmd_file_mkdir(const char *args) {
    if (!*args) { mon_print("usage: FK path"); return; }
    if (!wifi_ready()) { mon_print("no WiFi"); return; }

    char enc[80];
    url_encode_path(args, enc, sizeof(enc));
    char qry[100];
    snprintf(qry, sizeof(qry), "/mkdir?path=%s", enc);

    int rc = http_post(qry, NULL, 0, fmon_buf, FMON_BUF_SIZE - 1, NULL);
    if (rc == HTTP_OK)  mon_print("created");
    else { char m[41]; snprintf(m, sizeof(m), "error %d", rc); mon_print(m); }
}

// FM src dst — rename/move file or directory via server /rename endpoint.
static void cmd_file_move(const char *args) {
    // Split on first space
    const char *sp = args;
    while (*sp && *sp != ' ') sp++;
    if (!*args || !*sp) { mon_print("usage: FM src dst"); return; }
    if (!wifi_ready()) { mon_print("no WiFi"); return; }

    char src[64], dst[64];
    int slen = (int)(sp - args);
    if (slen >= (int)sizeof(src)) slen = (int)sizeof(src) - 1;
    memcpy(src, args, (size_t)slen); src[slen] = '\0';
    while (*sp == ' ') sp++;
    snprintf(dst, sizeof(dst), "%s", sp);
    if (!*src || !*dst) { mon_print("usage: FM src dst"); return; }

    char esrc[80], edst[80], qry[180];
    url_encode_path(src, esrc, sizeof(esrc));
    url_encode_path(dst, edst, sizeof(edst));
    snprintf(qry, sizeof(qry), "/rename?src=%s&dst=%s", esrc, edst);

    int rc = http_post(qry, NULL, 0, fmon_buf, FMON_BUF_SIZE - 1, NULL);
    if (rc == HTTP_OK)  mon_print("renamed");
    else { char m[41]; snprintf(m, sizeof(m), "error %d", rc); mon_print(m); }
}

// FC — connect (or reconnect) to PicoFS.
static void cmd_file_connect(const char *args) {
    (void)args;
    char line[41];
    snprintf(line, sizeof(line), "WiFi: %s -> connecting...", wifi_state_str());
    mon_print(line);
    if (wifi_connect()) {
        mon_print("WiFi: connected");
    } else {
        snprintf(line, sizeof(line), "WiFi: failed (%s)", wifi_state_str());
        mon_print(line);
    }
}

// FX — disconnect from PicoFS.
static void cmd_file_disconnect(const char *args) {
    (void)args;
    wifi_disconnect();
    char line[41];
    snprintf(line, sizeof(line), "WiFi: %s", wifi_state_str());
    mon_print(line);
}

// FI — WiFi + disk status.
static void cmd_file_info(const char *args) {
    (void)args;
    char line[41];

    snprintf(line, sizeof(line), "WiFi: %s", wifi_state_str());
    mon_print(line);

    if (!wifi_ready()) return;

    // Ping PicoFS
    uint16_t rlen = 0;
    int rc = http_get("/ping", fmon_buf, FMON_BUF_SIZE - 1, &rlen);
    if (rc == HTTP_OK) {
        fmon_buf[rlen < FMON_BUF_SIZE ? rlen : FMON_BUF_SIZE - 1] = '\0';
        // Trim to first newline for display
        for (int i = 0; i < (int)rlen; i++)
            if (fmon_buf[i] == '\n' || fmon_buf[i] == '\r') { fmon_buf[i] = '\0'; break; }
        snprintf(line, sizeof(line), "ping: %.34s", (char *)fmon_buf);
        mon_print(line);
    } else {
        snprintf(line, sizeof(line), "ping: error %d", rc);
        mon_print(line);
        return;
    }

    // Disk info
    rlen = 0;
    rc = http_get("/disk", fmon_buf, FMON_BUF_SIZE - 1, &rlen);
    if (rc != HTTP_OK) {
        snprintf(line, sizeof(line), "disk: error %d", rc);
        mon_print(line);
        return;
    }
    fmon_buf[rlen] = '\0';

    // Parse {"t":N,"f":N}
    long total = 0, free_b = 0;
    const char *p = (const char *)fmon_buf;
    const char *t = strstr(p, "\"t\":");
    const char *f = strstr(p, "\"f\":");
    if (t) { t += 4; while (*t >= '0' && *t <= '9') total  = total  * 10 + (*t++ - '0'); }
    if (f) { f += 4; while (*f >= '0' && *f <= '9') free_b = free_b * 10 + (*f++ - '0'); }

    if (total > 0)
        snprintf(line, sizeof(line), "SD: total=%ld K  free=%ld K",
                 total / 1024, free_b / 1024);
    else
        snprintf(line, sizeof(line), "SD: no info");
    mon_print(line);
}

// ---------------------------------------------------------------------------
// Command dispatch table

typedef struct {
    char        key;
    const char *synopsis;
    void      (*fn)(const char *args);
} mon_cmd_t;

static const mon_cmd_t commands[] = {
    { 'D', "D (addr)  hex dump 64 bytes",      cmd_dump        },
    { 'U', "U (addr)  unassemble 16 instrs",   cmd_unasm       },
    { 'R', "R         Z80 registers",          cmd_regs        },
    { 'S', "S         BASIC status/memory",    cmd_status      },
    { 'V', "V         variable list",          cmd_vars        },
    { 'E', "E         enhetslista (devices)",  cmd_devices     },
    { 'G', "G (addr)  go/run from addr",       cmd_go          },
    { 'P', "P         load SNAKE source->editor",cmd_snake       },
    { '?', "? N       error code N",           cmd_error       },
};
#define N_COMMANDS (int)(sizeof(commands) / sizeof(commands[0]))

static void cmd_help(void) {
    for (int i = 0; i < N_COMMANDS; i++)
        mon_print(commands[i].synopsis);
    mon_print("A n text  set asm line n / A n = delete");
    mon_print("AL (n)    list from line n  AC=clear");
    mon_print("AL n m    list lines n to m  AS addr=asm");
    mon_print("FC=connect FX=disconnect FI=info");
    mon_print("FL (path) list dir  FK path=mkdir");
    mon_print("FD path=delete  FM src dst=move");
    mon_print("H         this help");
    mon_print("Q         quit monitor");
}

static void mon_exec(const char *cmd) {
    while (*cmd == ' ') cmd++;
    char ch = *cmd++;
    if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);

    if (ch == '\0') return;   // empty line — do nothing
    if (ch == 'H') { cmd_help(); return; }

    /* F-family: FL, FD, FK, FM, FI */
    if (ch == 'F') {
        char sub = *cmd;
        if (sub >= 'a' && sub <= 'z') sub = (char)(sub - 32);
        cmd++;
        while (*cmd == ' ') cmd++;
        if      (sub == 'L') { cmd_file_list(cmd);       return; }
        else if (sub == 'D') { cmd_file_delete(cmd);     return; }
        else if (sub == 'K') { cmd_file_mkdir(cmd);      return; }
        else if (sub == 'M') { cmd_file_move(cmd);       return; }
        else if (sub == 'I') { cmd_file_info(cmd);       return; }
        else if (sub == 'C') { cmd_file_connect(cmd);    return; }
        else if (sub == 'X') { cmd_file_disconnect(cmd); return; }
        else { mon_print("F: L=list D=del K=mkdir M=move");
               mon_print("   I=info FC=connect FX=disconnect"); return; }
    }

    /* A-family: A n text, AS, AL, AC */
    if (ch == 'A') {
        char sub = *cmd;
        if (sub >= 'a' && sub <= 'z') sub = (char)(sub - 32);
        if (sub == 'S') { cmd++; while (*cmd == ' ') cmd++; cmd_asm_assemble(cmd); return; }
        if (sub == 'L') { cmd++; while (*cmd == ' ') cmd++; cmd_asm_list(cmd);     return; }
        if (sub == 'C') { cmd_asm_clear(cmd); return; }
        if (sub == 'I') { mon_print("AI command removed"); return; }
        /* A n [text] — line editor */
        while (*cmd == ' ') cmd++;
        cmd_asm_line(cmd);
        return;
    }

    while (*cmd == ' ') cmd++;
    for (int i = 0; i < N_COMMANDS; i++) {
        if (commands[i].key == ch) {
            commands[i].fn(cmd);
            return;
        }
    }
    mon_print("unknown command -- H for help");
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
    mon_print("ABC80 MONITOR  H=HELP  Q=EXIT");
    char info[41];
    snprintf(info, sizeof(info), "Stopped at PC=%04X SP=%04X", r.pc, r.sp);
    mon_print(info);
    snprintf(info, sizeof(info), "WiFi: %s", wifi_state_str());
    mon_print(info);
}

void monitor_exit(void) {
    mon_active = false;
}

void monitor_serial_poll(void) {
    int ch = getchar_timeout_us(0);
    if (ch == PICO_ERROR_TIMEOUT) return;

    if (ch == 0x0D || ch == 0x0A) {
        // Normal mode
        char echo[82];
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
    } else if (ch >= 0x20 && ch <= 0x7E && mon_linelen < 76) {
        mon_line[mon_linelen++] = (char)ch;
        mon_line[mon_linelen]   = '\0';
    }
}

void monitor_render(uint16_t *fb) {
    fb_clear(fb, MON_BG);

    // Title bar: amber background, black text
    fb_fill_rect(fb, 0, 1, DISPLAY_WIDTH, 9, MON_FG);
    fb_draw_string(fb, 0, 0, " ABC80 MONITOR", COLOR_BLACK, MON_FG);
    // WiFi state indicator in the middle of the title bar
    {
        const char *ws = wifi_state_str();
        char wlabel[8];
        snprintf(wlabel, sizeof(wlabel), "W:%-4s", ws);
        fb_draw_string(fb, 15 * 8, 0, wlabel, COLOR_BLACK, MON_FG);
    }
    fb_draw_string(fb, 24 * 8, 0, "X/Q=EXIT H=HELP", COLOR_BLACK, MON_FG);

    for (int i = 0; i < mon_nout; i++)
        fb_draw_string(fb, 0, (i + 2) * 10, mon_out[i], MON_FG, MON_BG);

    // Show last 38 chars of the input line so long asm lines stay visible
    char prompt[44];
    int plen = mon_linelen;
    const char *pstart = mon_line + (plen > 38 ? plen - 38 : 0);
    snprintf(prompt, sizeof(prompt), "> %.38s_", pstart);
    fb_draw_string(fb, 0, 23 * 10, prompt, MON_FG, MON_BG);
}
