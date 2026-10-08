/*
 * bac2bas.c  list an ABC80 BAC file (a SAVEd, compiled BASIC program)
 * as BASIC text, the way LIST does it.
 *
 *   bac2bas [-a] [-c] [fil.BAC ...]  > fil.BAS
 *
 * Reads the files given (or stdin) and writes the listing to stdout.
 *   -a  keep the ABC80 7-bit characters ([\]{|}$ ...) instead of
 *       writing ÄÖÅäöå¤ ... in UTF-8
 *   -c  end the lines with CR (as on the ABC80) instead of LF
 *
 * The program does not emulate the Z80. It is a routine by routine
 * parallel of the listing code in the ABC80 BASIC ROM (the old ROM,
 * abc80old.asm), with the same names:
 *
 *   DETOK   $1195   a program line -> text
 *   SPACE   $11EB   a space, unless there already is one
 *   DETEXP  $1258   expression code (RPN) -> infix
 *   DETOP   $1398   an operator, moved into place with ROTTXT
 *   OPTEXT  $13D5   the text of an operator token (KWEQV)
 *   FNTEXT  $13EB   the name of a function (FNKW)
 *   JOINC/JOINA     "," or "=" in front of the last operand
 *   DETVAR  $1425   a variable name
 *   ROTTXT  $1448   swap the last two pieces of text
 *   PRNUM   $1872   an unsigned 16-bit number
 *   FLTASC  $1683   a floating point constant (BCD)
 *
 * The Z80 code keeps the start addresses of the operands on the stack.
 * Here they are in stk[], and DE (where the text is written) is de.
 * HL (where the code is read) is hl.
 *
 * The keyword tables are copied from the ROM. The ROM finds a token
 * with CPIR from the start of a table and the text ends at the next
 * byte with bit 7 set, so the tables below keep the ROM's order: a
 * search may run on into the following table, as it does in the ROM.
 *
 * File format. A BAC file is 256-byte sectors (on disk; from tape
 * wav2basic writes the 253-byte blocks padded to 256). The first
 * sector begins with $82. Then come the lines:
 *     [length][line number lo][hi][code ...][$0D]
 * where the length counts the whole line. Length $00 means that the
 * rest of the sector is unused, $01 that the program ends.
 *
 * S. Lonnert 2026. Public domain.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* tables */

/* STKW1 ($1911) .. KWAS, and STKW2 ($199C) right after, as in the ROM */
static const unsigned char STKW[] =
    "\x82" "LET"  "\x80" "GOTO"  "\x84" "PRINT"  "\x85" ";"  "\x88" "IF"
    "\x8A" "INPUTLINE"  "\x89" "INPUT"  "\x8B" "FOR"  "\x8C" "NEXT"
    "\x8E" "READ"  "\x8F" "RESTORE"  "\x90" "GOSUB"  "\x91" "RETURN"
    "\x94" "ONERRORGOTO"  "\x92" "ON"  "\x93" "DATA"
    "\x96" "DEFFN" "\xFF"                               /* KWDEFN */
    "\x81\x83\x95" "??? "                               /* KWLIST */
    "\xBD" "THEN" "\xFF"  "\xBC" "ELSE" "\xFF"  "\xBB" "STEP" "\xFF"
    "\xBA" "TO" "\xFF"  "\xB8" "ASFILE" "\xFF"  "\xB7" "AS" "\xFF"
    /* STKW2 */
    "\x80" "DIM"  "\x81" "POKE"  "\x82" "OUT"  "\x84" "REM"  "\x85" "OPEN"
    "\x86" "PREPARE"  "\x87" "CLOSE"  "\x88" "RANDOMIZE"  "\x89" "STOP"
    "\x8A" "END"  "\x8B" "CMD"  "\x8E" "SETDOT"  "\x8F" "CLRDOT"
    "\x90" "GET"  "\x91" "CHAIN"  "\x92" "KILL"  "\x93" "NAME"
    "\x94" "TRACE"  "\x95" "NOTRACE" "\xFF";

#define STKW1  0
#define KWDEFN 92       /* "DEFFN", after the $96 */
#define STKW2  139

/* KWEQV ($23F5), 60 bytes, and FNKW ($2431) right after */
static const unsigned char KWOPFN[] =
    "\xDC" "EQV" "\xFF"  "\xDD" "IMP" "\xFF"  "\xDE" "OR"  "\xDF" "XOR" "\xFF"
    "\xE0" "AND" "\xFF"  "\xE1" "NOT" "\xFF"
    "\xF1" "<="  "\xE5" "<>"  "\xE8" "<"  "\xEB" ">="  "\xEE" ">"  "\xE2" "="
    "\xFF"  "\xF4" "+"  "\xF7" "-" "\xFF"  "\xF9" "*"  "\xFB" "/" "\xFF"
    "\xFD" "^"  "\xFD" "**" "\xFF"
    /* FNKW */
    "\x80" "FN"  "\x81" "ABS"  "\x82" "ATN"  "\x83" "COS"  "\x84" "EXP"
    "\x85" "FIX"  "\x86" "INT"  "\x88" "LOG10"  "\x87" "LOG"  "\x89" "PI"
    "\x8A" "RND"  "\x8B" "SGN"  "\x8C" "SIN"  "\x8D" "SQR"  "\x8E" "TAN"
    "\x8F" "ASC"  "\x90" "CHR$"  "\x91" "LEFT$"  "\x92" "RIGHT$"
    "\x93" "MID$"  "\x94" "LEN"  "\x95" "INSTR"  "\x96" "SPACE$"
    "\x97" "STRING$"  "\x98" "NUM$"  "\x99" "VAL"  "\x9A" "SWAP%"
    "\x9B" "PEEK"  "\x9C" "INP"  "\x9D" "CALL"  "\x9E" "ERRCODE"
    "\x9F" "IEC$"  "\xA0" "TAB"  "\xA1" "CUR"  "\xA3" "DOT"  "\xB0" "ADD$"
    "\xB1" "SUB$"  "\xB2" "MUL$"  "\xB3" "DIV$"  "\xB4" "COMP%" "\xFF";

#define KWEQV  0
#define KWEQVN 60       /* OPTEXT searches 60 bytes (BC = $003C) */
#define FNKW   60

/*  state */

static const unsigned char *code;       /* the line */
static int hl, codelen;                 /* HL, and the length of the line */
static unsigned char out[4096];         /* the text */
static int de;                          /* DE */
static int stk[256], sp;                /* operand start addresses */
static int bad;                         /* the line could not be listed */
static int jump;                        /* DETEXP returns into the text */

static int rd(void)                     /* LD A,(HL) */
{
    if (hl < 0 || hl >= codelen) {
        bad = 1;
        return 0x0D;
    }
    return code[hl];
}

static void wr(int c)                   /* LD (DE),A : INC DE */
{
    if (de < (int)sizeof out)
        out[de++] = (unsigned char)c;
    else
        bad = 1;
}

static void push(int p)
{
    if (sp < 256)
        stk[sp++] = p;
    else
        bad = 1;
}

static void pop(int n)
{
    sp -= n;
    if (sp < 0) {
        sp = 0;
        bad = 1;
    }
}

static int top(int k)                   /* k = 0: the latest */
{
    if (sp - 1 - k < 0) {
        bad = 1;
        return de;
    }
    return stk[sp - 1 - k];
}

/* CPIR from tab[start]: the position after the token, or -1 */
static int cpir(const unsigned char *tab, int tablen, int start, int n, int a)
{
    int i;
    for (i = start; i < tablen && (n == 0 || i < start + n); i++)
        if (tab[i] == a)
            return i + 1;
    return -1;
}

/* routines */

/* SPACE: a space at DE, unless the character before is one */
static void space(void)
{
    if (de > 0 && out[de - 1] == ' ')
        return;
    wr(' ');
}

/* PRNUM: HL as an unsigned decimal number, without leading zeros */
static void prnum(unsigned n)
{
    static const unsigned div[] = { 10000, 1000, 100, 10 };
    int i, started = 0;
    n &= 0xFFFF;
    for (i = 0; i < 4; i++) {
        unsigned d = n / div[i];
        n %= div[i];
        if (d || started) {
            wr('0' + d);
            started = 1;
        }
    }
    wr('0' + n);
}

/* ROTTXT: [p1 text 1][p2 text 2]DE -> [text 2][text 1] */
static void rottxt(int p1, int p2)
{
    unsigned char tmp[sizeof out];
    int n2 = de - p2;
    if (p1 < 0 || p1 > p2 || p2 > de) {
        bad = 1;
        return;
    }
    memcpy(tmp, out + p2, n2);
    memmove(out + p1 + n2, out + p1, p2 - p1);
    memcpy(out + p1, tmp, n2);
}

/* DETVAR: the name at HL (name byte, letter, 2 bytes of address).
   Returns the number of indices * 4 (0: no index). */
static int detvar(void)
{
    int c = rd();
    hl++;
    wr(rd());
    hl += 3;
    if ((c & 0xF0) != 0xF0)
        wr(((c >> 4) & 0x0F) | 0x30);
    if (c & 3)
        wr((c & 3) == 1 ? '%' : '$');
    return c & 0x0C;
}

/* JOINA: the character a in front of the latest operand, which then
   becomes part of the one before it. JOINC: "," */
static void joina(int a)
{
    int p1 = top(0);
    int p2 = de;
    wr(a);
    rottxt(p1, p2);
    pop(1);
}

#define joinc() joina(',')

/* L13FA: copy the text after a found token, at least one character */
static void copytext(const unsigned char *tab, int tablen, int i)
{
    do
        wr(tab[i++]);
    while (i < tablen && !(tab[i] & 0x80));
}

/* OPTEXT: the text of operator token a. Only the first of the tokens of
   one operator (one per type) is in KWEQV, so a, a-1, a-2 ... are tried.
   $FF begins with $FE. */
static void optext(int a)
{
    int i, tries;
    if (a != 0xFF)
        a++;
    for (tries = 0; tries < 256; tries++) {
        a = (a - 1) & 0xFF;
        i = cpir(KWOPFN, KWEQVN, KWEQV, KWEQVN, a);
        if (i >= 0) {
            copytext(KWOPFN, sizeof KWOPFN - 1, i);
            return;
        }
    }
    bad = 1;
}

/* FNKWTX: the name of function token a */
static void fnkwtx(int a)
{
    int i = cpir(KWOPFN, sizeof KWOPFN - 1, FNKW, 0, a);
    if (i < 0) {
        wr('?');
        bad = 1;
        return;
    }
    copytext(KWOPFN, sizeof KWOPFN - 1, i);
}

/* FNTEXT: the byte in the code is 2 * function number; the token is
   $80 + number. $80 is a user function: "FN" + the name. */
static void fntext(void)
{
    int a = 0x80 | (rd() >> 1);
    hl++;
    if (a == 0x80) {
        fnkwtx(a);
        detvar();
        hl++;
    } else
        fnkwtx(a);
}

/* L12A8: "(" in front of the operand under the latest, ")" after;
   the latest start address goes */
static void paren(void)
{
    wr('(');
    rottxt(top(1), top(0));
    wr(')');
    pop(1);
}

/* DETOP: the operator text written last and rotated into place.
   flags bit 0: spaces around it (EQV IMP OR XOR AND NOT)
         bit 1: unary (NOT) */
static void detop(int flags)
{
    int p2 = top(0);
    int d0 = de;
    if ((flags & 1) && p2 > 0 && out[p2 - 1] != ' ')
        wr(' ');
    optext(code[hl - 1]);
    if ((flags & 1) && out[p2] != ' ')
        wr(' ');
    rottxt(p2, d0);
    if (!(flags & 2))
        pop(1);
}

/* FLTASC: the number with its exponent byte at code[e], format fmt
   (the listing uses $2C: 6 digits, no space before a positive number).
   The mantissa is 6 BCD digits in the 3 bytes at e-4, the sign (bit 0)
   at e-1. w[] is the work area at IX: w[0] format, w[1] sign, w[2]
   integer digits or zeros after the point, w[5] the place for a carry,
   w[6..11] the digits, w[13] 1 if x < 1. */
static void wrdig(const unsigned char *w, int *p, int *c)    /* WRDIG */
{
    wr(w[*p] + '0');
    (*p)++;
    (*c);
}

static void fltasc(int e, int fmt)
{
    unsigned char w[16];
    int i, a, n, c, p, pr, E, B;

    memset(w, 0, sizeof w);
    w[0] = (unsigned char)fmt;
    for (i = 0; i < 3; i++) {
        int m = (e - 4 + i < codelen && e - 4 + i >= 0) ? code[e - 4 + i] : 0;
        w[6 + 2 * i] = (m >> 4) & 0x0F;
        w[7 + 2 * i] = m & 0x0F;
    }
    w[1] = (unsigned char)(e - 1 < codelen ? code[e - 1] : 0);

    /* the exponent: w[2] and w[13] */
    a = e < codelen ? code[e] : 0;
    if (a == 0)
        a = 1;                          /* x = 0 */
    else if (a >= 0x80) {
        a -= 0x80;
        if (a == 0)
            w[13] = 1;                  /* .1 <= x < 1 */
    } else {
        a = 0x80 - a;                   /* zeros after the point */
        w[13] = 1;
    }
    w[2] = (unsigned char)a;
    E = a;
    if (a >= 6)
        w[0] |= 1;                      /* E format */

    /* FAFMT: c = the number of digits kept */
    for (;;) {
        n = (w[0] >> 1) & 0x0F;
        if (w[0] & 1) {                 /* E format */
            if (n >= 6)
                n = 5;
            c = n + 1;
            break;
        }
        if (n + E >= 7 && (w[0] & 0x40)) {
            w[0] |= 1;
            continue;
        }
        if (!w[13]) {
            c = n + E;
            break;
        }
        c = n - E;                      /* FASMAL */
        if (c <= 0) {                   /* FATINY */
            if ((w[0] & 0x80) && (w[0] & 0x0E)) {
                E = ((w[0] & 0x0E) >> 1) - 1;
                c = 1;
                p = 5;
                goto faout;
            }
            goto fazero;
        }
        break;
    }

    /* FAROUN: round at the digit after the c kept */
    c &= 0x1F;
    if (c > 7)
        c = 7;
    pr = p = 6 + c;
    if (w[p] >= 5) {
        for (;;) {                      /* FACARY */
            p;
            w[p]++;
            if (w[p] != 10)
                break;
            w[p] = 0;
        }
        p = pr;
    }
    /* FASTRP: drop the zeros at the end */
    p;
    while (w[p] == 0) {
        p;
        c;
        if (c < 0)
            goto fazero;
    }
    /* FAOVFL: did the rounding carry into w[5]? */
    p = 5;
    if (w[5]) {
        B = 1;
        if (w[13])
            B = -1;
        if (w[2] == 0) {
            w[13] = 0;
            B = 1;
        }
        w[2] = (unsigned char)(w[2] + B);
        E++;                            /* the old ROM's error: E+1 also when B = -1 */
        c++;
        p = 4;
    }
faout:
    p++;
    if (c == 7)
        c = 6;

    /* FASIGN */
    B = E;
    if (w[1] & 1)
        wr('-');
    else if (!(w[0] & 0x20))
        wr(' ');
    if (w[0] & 1) {                     /* FAEOUT */
        wrdig(w, &p, &c);
        if (c != 0) {
            wr('.');
            do
                wrdig(w, &p, &c);
            while (c != 0);
        }
        wr('E');
        if (w[13]) {
            wr('-');
            prnum((w[2] + 1) & 0xFF);
        } else {
            wr('+');
            prnum((w[2] - 1) & 0xFF);
        }
        return;
    }
    if (w[13]) {                        /* x < 1 */
        if (c == 0) {
            wr('0');
            return;
        }
        goto fadot;
    }
    do {                                /* FAINT */
        wrdig(w, &p, &c);
        B;
    } while (B != 0);
    if (c <= 0)
        return;
fadot:
    wr('.');
    for (; B > 0; B)                  /* FALEAD */
        wr('0');
    do                                  /* FAFRAC */
        wrdig(w, &p, &c);
    while (c != 0);
    return;

fazero:
    if (!(w[0] & 0x20))
        wr(' ');
    wr('0');
}

/* DIM element ($CC, $D2): "A(i,j)", "A$=n" or "A$(i,j)=n", then "," or
   $BF, which ends the DIM list. Returns 1 at $BF. */
static int dimelem(int string)
{
    int carry, a;

    if (string) {                       /* L135A */
        push(de);
        if ((rd() & 0x0C) == 0) {       /* L1382: no index */
            carry = 1;
            a = '=';
            goto l1347;
        }
        wr(')');
        wr('=');
        rottxt(top(1), top(0));
        pop(2);
        carry = 1;
    } else
        carry = 0;                      /* L133D */
    /* L133E */
    if (rd() & 0x08)
        joinc();
    push(de);
    a = '(';
l1347:
    detvar();
    wr(a);
    rottxt(top(1), top(0));
    if (!carry)
        wr(')');
    /* L1370 */
    pop(2);
    a = rd();
    hl++;
    if (a == 0xBF)
        return 1;
    hl;
    wr(',');
    return 0;
}

/* DETEXP: expression code (operands first, then the operator) -> infix */
static void detexp(void)
{
    int a, q, n, c;

    while (!bad) {
        a = rd();
        hl++;
        if (a >= 0xB6 && a <= 0xB8) {   /* assignment, type 0-2 */
            joina('=');
            pop(1);
            return;
        }
        if (a < 0xC0) {                 /* end of the expression */
            pop(1);
            /* More than one operand left: in the ROM the RET then takes
               an operand's start address as the return address and runs
               the listed text as Z80 code. Programs use this as a LIST
               protection (a string with machine code that rewrites the
               listing). It cannot be followed without a Z80. */
            if (sp > 0)
                jump = 1;
            return;
        }
        if (a == 0xE1) {                /* NOT */
            detop(3);
            continue;
        }
        if (a >= 0xE2) {                /* comparison, arithmetic */
            detop(0);
            continue;
        }
        if (a >= 0xDC) {                /* EQV IMP OR XOR AND */
            detop(1);
            continue;
        }
        if (a == 0xD7 || a == 0xD8) {   /* L141B: unary minus */
            n = de;
            wr('-');
            rottxt(top(0), n);
            continue;
        }
        if (a == 0xDB) {                /* FN argument entry */
            hl += 4;
            continue;
        }
        if (a == 0xD3) {                /* parenthesis */
            push(de);
            paren();
            continue;
        }
        if (a > 0xD3)                   /* $D4-$D6, $D9, $DA: not listed */
            continue;

        switch (a) {                    /* DETTAB */
        case 0xC8: case 0xC9: case 0xCA:        /* two indices */
            joinc();
            /* fall through */
        case 0xC0: case 0xC1: case 0xC2:        /* variable */
        case 0xC4: case 0xC5: case 0xC6:        /* one index */
            push(de);
            if (detvar())
                paren();
            break;
        case 0xC3:                              /* floating point constant */
            push(de);
            fltasc(hl + 4, 0x2C);
            hl += 5;
            break;
        case 0xC7:                              /* integer constant */
            push(de);
            n = rd();
            hl++;
            n |= rd() << 8;
            hl++;
            prnum(n);
            wr('%');
            break;
        case 0xCB:                              /* string constant */
            push(de);
            q = rd();
            hl++;
            n = rd();
            hl++;
            wr(q);
            for (;;) {
                n = (n - 1) & 0xFF;
                if (n & 0x80)
                    break;
                c = rd();
                hl++;
                wr(c);
                if (c == q)
                    wr(q);
            }
            wr(q);
            break;
        case 0xCC:                              /* DIM element */
            hl++;
            if (dimelem(rd() & 0x02))
                return;
            break;
        case 0xD2:
            hl += 2;
            if (dimelem(0))
                return;
            break;
        case 0xCD:                              /* function, no argument */
            push(de);
            fntext();
            break;
        case 0xD1:                              /* 4 arguments */
            joinc();
            /* fall through */
        case 0xD0:                              /* 3 */
            joinc();
            /* fall through */
        case 0xCF:                              /* 2 */
            joinc();
            /* fall through */
        case 0xCE:                              /* 1 */
            push(de);
            fntext();
            paren();
            break;
        }
    }
}

/* DETOK: a program line [length][number lo][hi][code ...][$0D] -> text
   ending with $0D. Returns the length of the text. */
static int detok(const unsigned char *line, int len)
{
    int a, i, n, b;

    code = line;
    codelen = len;
    de = 0;
    sp = 0;
    bad = 0;
    jump = 0;
    hl = 1;
    prnum(line[1] | (line[2] << 8));
    hl = 3;
    space();
    while (!bad) {
        a = rd();
        if (a == ':') {
            space();
            wr(a);
            hl++;
            space();
        } else if (a == 0x87) {                 /* IF/ELSE jump */
            hl += 2;
        } else if (a == 0xB9) {                 /* FOR entry */
            hl += 3;
        } else if (a == 0xBF || a == 0xBE) {    /* line references */
            b = 1;
            if (a == 0xBE) {
                hl++;
                b = rd();
            }
            hl++;
            for (;;) {
                n = rd();
                n |= code[hl + 1 < len ? hl + 1 : hl] << 8;
                hl += 4;
                prnum(n);
                b = (b - 1) & 0xFF;
                if (b == 0)
                    break;
                wr(',');
            }
        } else if (a == 0x0D) {                 /* end of the line */
            wr(a);
            return de;
        } else if (a >= 0xC0) {
            detexp();
            if (jump) {                         /* the line ends here */
                wr(0x0D);
                return de;
            }
        } else if (a == 0x96) {                 /* DEF FN */
            hl++;
            copytext(STKW, sizeof STKW - 1, KWDEFN);
            detvar();
            hl -= 2;
            n = rd();
            hl++;
            if (n) {
                for (i = 0; i < n; i++) {
                    wr(i ? ',' : '(');
                    detvar();
                    hl -= 2;
                }
                wr(')');
            }
        } else if (a < 0x80) {                  /* a character */
            wr(a);
            hl++;
        } else {                                /* a keyword */
            int t = STKW1;
            space();
            hl++;
            if (a == 0x86) {
                t = STKW2;
                a = rd();
                hl++;
            }
            i = cpir(STKW, sizeof STKW - 1, t, 0, a);
            if (i < 0) {
                bad = 1;
                break;
            }
            while (i < (int)sizeof STKW - 1 && !(STKW[i] & 0x80))
                wr(STKW[i++]);
            space();
        }
    }
    return -1;
}

/*  output */

static int ascii, crlf;

static void put_text(const unsigned char *s, int n)
{
    static const char *sw[128];
    int i;

    if (!sw['[']) {
        sw['@'] = "\xC3\x89"; sw['['] = "\xC3\x84"; sw['\\'] = "\xC3\x96";
        sw[']'] = "\xC3\x85"; sw['^'] = "\xC3\x9C"; sw['`'] = "\xC3\xA9";
        sw['{'] = "\xC3\xA4"; sw['|'] = "\xC3\xB6"; sw['}'] = "\xC3\xA5";
        sw['~'] = "\xC3\xBC"; sw['$'] = "\xC2\xA4";
    }
    for (i = 0; i < n; i++) {
        int c = s[i];
        if (c == 0x0D && i == n - 1)
            fputs(crlf ? "\r" : "\n", stdout);
        else if (!ascii && c < 128 && sw[c])
            fputs(sw[c], stdout);
        else if (!ascii && (c < 32 || c > 126))
            printf("\\x%02x", c);
        else
            putchar(c);
    }
}

static int list_file(const char *name, FILE *f)
{
    unsigned char *buf = NULL;
    size_t size = 0, cap = 0, k;
    long s, nsec;
    int i, len, errors = 0, done = 0;

    for (;;) {
        if (size == cap) {
            cap = cap ? 2 * cap : 65536;
            buf = realloc(buf, cap);
            if (!buf) {
                fprintf(stderr, "bac2bas: out of memory\n");
                exit(2);
            }
        }
        k = fread(buf + size, 1, cap - size, f);
        if (k == 0)
            break;
        size += k;
    }
    if (size == 0) {
        fprintf(stderr, "bac2bas: %s: empty\n", name);
        free(buf);
        return 1;
    }
    if (buf[0] != 0x82)
        fprintf(stderr, "bac2bas: %s: does not begin with $82, not a BAC file?\n",
                name);

    nsec = (long)((size + 255) / 256);
    for (s = 0; s < nsec && !done; s++) {
        unsigned char sec[256];
        size_t off = (size_t)s * 256;
        memset(sec, 0, sizeof sec);
        memcpy(sec, buf + off, size - off < 256 ? size - off : 256);
        i = s == 0 ? 1 : 0;
        if (sec[i] == 0) {
            fprintf(stderr, "bac2bas: %s: sector %ld empty (a missing block?)\n",
                    name, s);
            continue;
        }
        while (i < 256) {
            int L = sec[i];
            if (L == 0)
                break;
            if (L == 1) {
                done = 1;
                break;
            }
            if (L < 4 || i + L > 256) {
                fprintf(stderr, "bac2bas: %s: sector %ld: bad line length %d\n",
                        name, s, L);
                errors++;
                break;
            }
            len = detok(sec + i, L);
            if (jump)
                fprintf(stderr, "bac2bas: %s: line %u: LIST runs machine code"
                        " here (list protection), listed as stored up to"
                        " that point\n", name, sec[i + 1] | (sec[i + 2] << 8));
            if (len < 0) {
                char msg[80];
                unsigned num = sec[i + 1] | (sec[i + 2] << 8);
                fprintf(stderr, "bac2bas: %s: line %u could not be listed\n",
                        name, num);
                sprintf(msg, "%u REM *** RADEN KUNDE INTE LISTAS ***\r", num);
                put_text((const unsigned char *)msg, (int)strlen(msg));
                errors++;
            } else
                put_text(out, len);
            i += L;
        }
    }
    if (!done)
        fprintf(stderr, "bac2bas: %s: no end mark ($01)\n", name);
    free(buf);
    return errors != 0;
}

int main(int argc, char **argv)
{
    int i, files = 0, status = 0;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-a"))
            ascii = 1;
        else if (!strcmp(argv[i], "-c"))
            crlf = 1;
        else if (argv[i][0] == '-' && argv[i][1]) {
            fprintf(stderr, "usage: bac2bas [-a] [-c] [file.BAC ...]\n");
            return 2;
        }
    }
    for (i = 1; i < argc; i++) {
        FILE *f;
        if (argv[i][0] == '-' && argv[i][1])
            continue;
        files++;
        if (!strcmp(argv[i], "-")) {
            status |= list_file("stdin", stdin);
            continue;
        }
        f = fopen(argv[i], "rb");
        if (!f) {
            perror(argv[i]);
            status = 1;
            continue;
        }
        status |= list_file(argv[i], f);
        fclose(f);
    }
    if (!files)
        status = list_file("stdin", stdin);
    return status;
}
