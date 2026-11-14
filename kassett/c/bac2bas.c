/*
 * bac2bas.c  BAC-fil (ett sparat, kompilerat BASIC-program) -> text,
 * som LIST skriver den.
 *
 *   bac2bas [--kod 7|dos|utf8] [--rad cr|crlf|lf] [-o UT] FIL.BAC ...
 *
 * Läser filerna (eller stdin, '-') och skriver listan till UT (eller
 * stdout).
 *   --kod  7     ABC:s 7-bitarskod som den är ([\]{|} och så vidare)
 *          dos   ÄÖÅäöå... i teckentabell 850 (MS-DOS)
 *          utf8  ÄÖÅäöå... i UTF-8 (standard)
 *   --rad  cr    radslut CR, som på ABC
 *          crlf  CR LF, som i MS-DOS och Windows
 *          lf    LF (standard)
 *
 * Formatet ses på första byten:
 *   $82  ABC80 (BASIC i ABC80, den gamla och den nya ROM:en)
 *   $8F  BASIC II (ABC 800 C och M, ABC 802, ABC 806)
 *
 * Programmet kör inte Z80-koden. Det gör LIST rutin för rutin som
 * ROM:en, med samma namn på rutinerna, och precis som bac2bas.py:
 *
 * ABC80: DETOK $1195 en rad, SPACE $11EB, DETEXP $1258 uttrycken
 * (omvänd polsk notation), DETOP $1398 en operator, OPTEXT $13D5,
 * FNTEXT $13EB, DETVAR $1425 ett namn, ROTTXT $1448 byter plats på två
 * textbitar, PRNUM $1872 ett heltal, FLTASC $1683 ett flyttal (BCD).
 *
 * BASIC II (ABC 802): DLINE $40D3 en rad, DSTMT $40EA satserna, DECOMP
 * $3D75 uttrycken, DOPR $3E3D en operator, DMERGE $3FE7 flyttar en
 * textbit, DFUNC $3DED och S3FBB funktionerna, DDIM $3F15, DNUM $3E7A
 * och FTODEC $5153 ett flyttal (binärt), NAMTX1 $3379 ett namn, S3032
 * $3032 ord i koden (länkade eller inte), L307D de lokala namnen i DEF.
 *
 * Tabellerna är kopierade ur ROM:erna, så sökningarna går som där.
 *
 * S. Lönnert 2026. Public domain.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abckas.h"

/* ------------------------------------------------------------------ */
/* fel: raden kan inte listas */

static jmp_buf hopp;
static char felet[120];

static void fel(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsprintf(felet, fmt, ap);
    va_end(ap);
    longjmp(hopp, 1);
}

static const char *namnet;          /* filen */
static int antal_fel;

static void meddela(int varning, const char *fmt, ...)
{
    va_list ap;
    fprintf(stderr, "bac2bas: %s: ", namnet);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    putc('\n', stderr);
    if (!varning)
        antal_fel++;
}

static void *minne(void *p, size_t n)
{
    p = realloc(p, n ? n : 1);
    if (!p) {
        fprintf(stderr, "bac2bas: minnet tog slut\n");
        exit(2);
    }
    return p;
}

/* listan för alla filerna */
static byte *lista;
static long llen, lcap;

static void till_lista(const byte *s, long n)
{
    if (llen + n > lcap) {
        lcap = 2 * (llen + n) + 4096;
        lista = (byte *) minne(lista, (size_t) lcap);
    }
    memcpy(lista + llen, s, (size_t) n);
    llen += n;
}

static void ej_listad(unsigned nr)
{
    char t[60];
    sprintf(t, "%u REM *** RADEN KUNDE INTE LISTAS ***\r", nr);
    till_lista((const byte *) t, (long) strlen(t));
}

/* ================================================================== */
/* ABC80 */

/* STKW1 ($1911) .. KWAS och STKW2 ($199C) direkt efter, som i ROM:en */
static const byte STKW80[] =
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

/* KWEQV ($23F5), 60 byte, och FNKW ($2431) direkt efter */
static const byte KWOPFN[] =
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

#define STKW1  0
#define KWDEFN 92       /* "DEFFN", efter $96 */
#define STKW2  139
#define KWEQV  0
#define KWEQVN 60       /* OPTEXT söker 60 byte (BC = $003C) */
#define FNKW   60
#define NSTKW  ((int) sizeof STKW80 - 1)
#define NKWOP  ((int) sizeof KWOPFN - 1)

static const byte *code;            /* raden */
static int hl, codelen;             /* HL och radens längd */
static byte out[4096];              /* texten */
static int de;                      /* DE */
static int stk[256], sp;            /* operandernas början */
static int jump;                    /* DETEXP hoppar in i texten */

static int rd(void)                 /* LD A,(HL) */
{
    if (hl < 0 || hl >= codelen)
        fel("koden tar slut");
    return code[hl];
}

static void wr(int c)               /* LD (DE),A : INC DE */
{
    if (de >= (int) sizeof out)
        fel("för lång");
    out[de++] = (byte) c;
}

static void push(int p)
{
    if (sp >= 256)
        fel("stacken");
    stk[sp++] = p;
}

static void pop(int n)
{
    if (sp < n)
        fel("stacken");
    sp -= n;
}

static int top(int k)               /* k = 0: den senaste */
{
    if (sp <= k)
        fel("stacken");
    return stk[sp - 1 - k];
}

/* CPIR från tab[start] (n byte, 0: till slutet): platsen efter koden */
static int cpir(const byte *tab, int tablen, int start, int n, int a)
{
    int i;
    for (i = start; i < tablen && (n == 0 || i < start + n); i++)
        if (tab[i] == a)
            return i + 1;
    return -1;
}

/* SPACE: ett blanksteg, om inte tecknet före är ett */
static void space(void)
{
    if (de > 0 && out[de - 1] == ' ')
        return;
    wr(' ');
}

/* PRNUM: ett heltal 0-65535 utan nollor före */
static void prnum(unsigned n)
{
    char t[8];
    int i;
    sprintf(t, "%u", n & 0xFFFF);
    for (i = 0; t[i]; i++)
        wr(t[i]);
}

/* ROTTXT: [p1 text 1][p2 text 2]DE -> [text 2][text 1] */
static void rottxt(int p1, int p2)
{
    byte tmp[sizeof out];
    int n2 = de - p2;
    if (p1 < 0 || p1 > p2 || p2 > de)
        fel("ROTTXT");
    memcpy(tmp, out + p2, (size_t) n2);
    memmove(out + p1 + n2, out + p1, (size_t) (p2 - p1));
    memcpy(out + p1, tmp, (size_t) n2);
}

/* DETVAR: namnet vid HL (namnbyte, bokstav, 2 byte adress). Svaret är
   antalet index * 4 (0: inget index). */
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

/* JOINA: tecknet a före den senaste operanden, som då blir en del av
   den före. JOINC: "," */
static void joina(int a)
{
    int p1 = top(0);
    int p2 = de;
    wr(a);
    rottxt(p1, p2);
    pop(1);
}

/* L13FA: texten efter en funnen kod, minst ett tecken */
static void copytext(const byte *tab, int tablen, int i)
{
    do
        wr(tab[i++]);
    while (i < tablen && !(tab[i] & 0x80));
}

/* OPTEXT: operatorn a. Bara den första av en operators koder (en per
   typ) står i KWEQV, så a, a-1, a-2 ... provas. $FF börjar med $FE. */
static void optext80(int a)
{
    int i, n;
    if (a != 0xFF)
        a++;
    for (n = 0; n < 256; n++) {
        a = (a - 1) & 0xFF;
        i = cpir(KWOPFN, KWEQVN, KWEQV, KWEQVN, a);
        if (i >= 0) {
            copytext(KWOPFN, NKWOP, i);
            return;
        }
    }
    fel("okänd operator");
}

/* FNKWTX: funktionen a */
static void fnkwtx(int a)
{
    int i = cpir(KWOPFN, NKWOP, FNKW, 0, a);
    if (i < 0)
        fel("okänd funktion");
    copytext(KWOPFN, NKWOP, i);
}

/* FNTEXT: byten i koden är 2 * funktionens nummer, koden $80 + numret.
   $80 är en egen funktion: "FN" och namnet. */
static void fntext(void)
{
    int a = 0x80 | (rd() >> 1);
    hl++;
    fnkwtx(a);
    if (a == 0x80) {
        detvar();
        hl++;
    }
}

/* L12A8: "(" före operanden under den senaste, ")" efter; den senastes
   början går bort */
static void paren80(void)
{
    wr('(');
    rottxt(top(1), top(0));
    wr(')');
    pop(1);
}

/* DETOP: operatorn skrivs sist och flyttas på plats.
   flags bit 0: blanksteg runt (EQV IMP OR XOR AND NOT)
         bit 1: en operand (NOT) */
static void detop(int flags)
{
    int p2 = top(0);
    int d0 = de;
    if ((flags & 1) && p2 > 0 && out[p2 - 1] != ' ')
        wr(' ');
    optext80(code[hl - 1]);
    if ((flags & 1) && p2 < de && out[p2] != ' ')
        wr(' ');
    rottxt(p2, d0);
    if (!(flags & 2))
        pop(1);
}

/* FLTASC: talet med exponenten i code[e], formatet fmt (listan har $2C:
   6 siffror, inget blanksteg före ett positivt tal). Mantissan är 6
   BCD-siffror i de 3 byten vid e-4, tecknet (bit 0) vid e-1. w[] är
   arbetsytan vid IX: w[0] formatet, w[1] tecknet, w[2] heltalssiffror
   eller nollor efter punkten, w[5] platsen för en minnessiffra,
   w[6..11] siffrorna, w[13] 1 om x < 1. */
static int kod_vid(int i)
{
    return i >= 0 && i < codelen ? code[i] : 0;
}

static void wrdig(const byte *w, int *p, int *c)    /* WRDIG */
{
    wr(w[*p] + '0');
    (*p)++;
    (*c)--;
}

static void fltasc(int e, int fmt)
{
    byte w[16];
    int i, a, n, c, p = 0, pr, E, B;

    memset(w, 0, sizeof w);
    w[0] = (byte) fmt;
    for (i = 0; i < 3; i++) {
        int k = kod_vid(e - 4 + i);
        w[6 + 2 * i] = (byte) ((k >> 4) & 0x0F);
        w[7 + 2 * i] = (byte) (k & 0x0F);
    }
    w[1] = (byte) kod_vid(e - 1);

    a = kod_vid(e);                     /* exponenten: w[2] och w[13] */
    if (a == 0)
        a = 1;                          /* x = 0 */
    else if (a >= 0x80) {
        a -= 0x80;
        if (a == 0)
            w[13] = 1;                  /* .1 <= x < 1 */
    } else {
        a = 0x80 - a;                   /* nollor efter punkten */
        w[13] = 1;
    }
    w[2] = (byte) a;
    E = a;
    if (a >= 6)
        w[0] |= 1;                      /* E-form */

    for (;;) {                          /* FAFMT: c = antal siffror */
        n = (w[0] >> 1) & 0x0F;
        if (w[0] & 1) {
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

    c &= 0x1F;                          /* FAROUN: avrunda */
    if (c > 7)
        c = 7;
    pr = p = 6 + c;
    if (w[p] >= 5) {
        for (;;) {                      /* FACARY */
            p--;
            w[p]++;
            if (w[p] != 10)
                break;
            w[p] = 0;
        }
        p = pr;
    }
    p--;                                /* FASTRP: nollorna sist bort */
    while (w[p] == 0) {
        p--;
        c--;
        if (c < 0)
            goto fazero;
    }
    p = 5;                              /* FAOVFL: minnessiffra i w[5]? */
    if (w[5]) {
        B = 1;
        if (w[13])
            B = -1;
        if (w[2] == 0) {
            w[13] = 0;
            B = 1;
        }
        w[2] = (byte) (w[2] + B);
        E++;                    /* den gamla ROM:ens fel: E+1 också när B = -1 */
        c++;
        p = 4;
    }
faout:
    p++;
    if (c == 7)
        c = 6;

    B = E;                              /* FASIGN */
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
        B--;
    } while (B != 0);
    if (c <= 0)
        return;
fadot:
    wr('.');
    for (; B > 0; B--)                  /* FALEAD */
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

/* DIM-post ($CC, $D2): "A(i,j)", "A$=n" eller "A$(i,j)=n", sedan ","
   eller $BF, som avslutar DIM-listan. Svaret är 1 vid $BF. */
static int dimelem(int string)
{
    int carry, a;

    if (string) {                       /* L135A */
        push(de);
        if ((rd() & 0x0C) == 0) {       /* L1382: inget index */
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
    if (rd() & 0x08)                    /* L133E */
        joina(',');
    push(de);
    a = '(';
l1347:
    detvar();
    wr(a);
    rottxt(top(1), top(0));
    if (!carry)
        wr(')');
    pop(2);                             /* L1370 */
    a = rd();
    hl++;
    if (a == 0xBF)
        return 1;
    hl--;
    wr(',');
    return 0;
}

/* DETEXP: uttryckets kod (operanderna först, sedan operatorn) -> text */
static void detexp(void)
{
    int a, q, n, c;

    for (;;) {
        a = rd();
        hl++;
        if (a >= 0xB6 && a <= 0xB8) {   /* tilldelning, typ 0-2 */
            joina('=');
            pop(1);
            return;
        }
        if (a < 0xC0) {                 /* uttrycket slut */
            pop(1);
            /* Fler operander kvar: i ROM:en tar RET en operands adress
               som återhopp och kör den listade texten som Z80-kod.
               Program använder det som LIST-skydd. Det går inte att
               följa utan en Z80. */
            if (sp > 0)
                jump = 1;
            return;
        }
        if (a == 0xE1) {                /* NOT */
            detop(3);
            continue;
        }
        if (a >= 0xE2) {                /* jämförelse, räknesätt */
            detop(0);
            continue;
        }
        if (a >= 0xDC) {                /* EQV IMP OR XOR AND */
            detop(1);
            continue;
        }
        if (a == 0xD7 || a == 0xD8) {   /* L141B: minus */
            n = de;
            wr('-');
            rottxt(top(0), n);
            continue;
        }
        if (a == 0xDB) {                /* argument till FN */
            hl += 4;
            continue;
        }
        if (a == 0xD3) {                /* parentes */
            push(de);
            paren80();
            continue;
        }
        if (a > 0xD3)                   /* $D4-$D6, $D9, $DA */
            continue;

        switch (a) {                    /* DETTAB */
        case 0xC8: case 0xC9: case 0xCA:        /* två index */
            joina(',');
            /* vidare */
        case 0xC0: case 0xC1: case 0xC2:        /* variabel */
        case 0xC4: case 0xC5: case 0xC6:        /* ett index */
            push(de);
            if (detvar())
                paren80();
            break;
        case 0xC3:                              /* flyttal */
            push(de);
            fltasc(hl + 4, 0x2C);
            hl += 5;
            break;
        case 0xC7:                              /* heltal */
            push(de);
            n = rd();
            hl++;
            n |= rd() << 8;
            hl++;
            prnum((unsigned) n);
            wr('%');
            break;
        case 0xCB:                              /* sträng */
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
        case 0xCC:                              /* DIM-post */
            hl++;
            if (dimelem(rd() & 0x02))
                return;
            break;
        case 0xD2:
            hl += 2;
            if (dimelem(0))
                return;
            break;
        case 0xCD:                              /* funktion utan argument */
            push(de);
            fntext();
            break;
        case 0xD1:                              /* 4 argument */
            joina(',');
            /* vidare */
        case 0xD0:                              /* 3 */
            joina(',');
            /* vidare */
        case 0xCF:                              /* 2 */
            joina(',');
            /* vidare */
        case 0xCE:                              /* 1 */
            push(de);
            fntext();
            paren80();
            break;
        }
    }
}

/* DETOK: raden [längd][nummer lo][hi][kod ...][$0D] -> text som slutar
   med CR i out[]. Svaret är längden. */
static int detok(const byte *line, int len)
{
    int a, i, n, b, t;

    code = line;
    codelen = len;
    de = 0;
    sp = 0;
    jump = 0;
    prnum((unsigned) (line[1] | (line[2] << 8)));
    hl = 3;
    space();
    for (;;) {
        a = rd();
        if (a == ':') {
            space();
            wr(a);
            hl++;
            space();
        } else if (a == 0x87) {                 /* IF/ELSE: hoppet */
            hl += 2;
        } else if (a == 0xB9) {                 /* FOR */
            hl += 3;
        } else if (a == 0xBF || a == 0xBE) {    /* radnummer */
            b = 1;
            if (a == 0xBE) {
                hl++;
                b = rd();
            }
            hl++;
            for (;;) {
                n = rd();
                hl++;
                n |= rd() << 8;
                hl += 3;
                prnum((unsigned) n);
                b = (b - 1) & 0xFF;
                if (b == 0)
                    break;
                wr(',');
            }
        } else if (a == 0x0D) {                 /* radslut */
            wr(a);
            return de;
        } else if (a >= 0xC0) {
            detexp();
            if (jump) {                         /* raden slutar här */
                wr(0x0D);
                return de;
            }
        } else if (a == 0x96) {                 /* DEF FN */
            hl++;
            copytext(STKW80, NSTKW, KWDEFN);
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
        } else if (a < 0x80) {                  /* ett tecken */
            wr(a);
            hl++;
        } else {                                /* ett nyckelord */
            t = STKW1;
            space();
            hl++;
            if (a == 0x86) {
                t = STKW2;
                a = rd();
                hl++;
            }
            i = cpir(STKW80, NSTKW, t, 0, a);
            if (i < 0)
                fel("okänt nyckelord $%02X", a);
            while (i < NSTKW && !(STKW80[i] & 0x80))
                wr(STKW80[i++]);
            space();
        }
    }
}

static int rad80(const byte *line, int len)
{
    if (setjmp(hopp))
        return -1;
    return detok(line, len);
}

/* ABC80: 256-bytes sektorer, den första börjar med $82. Raderna:
   [längd][nummer lo][hi][kod ...][$0D]. Längd $00: resten av sektorn
   är tom, $01: programmet slut. */
static void list80(const byte *data, long size)
{
    long s, nsec = (size + 255) / 256;
    int i, n, klar = 0;

    for (s = 0; s < nsec && !klar; s++) {
        byte sec[256];
        long off = s * 256;
        memset(sec, 0, sizeof sec);
        memcpy(sec, data + off, (size_t) (size - off < 256 ? size - off : 256));
        i = s == 0 ? 1 : 0;
        if (sec[i] == 0) {
            meddela(0, "sektor %ld är tom (ett block saknas?)", s);
            continue;
        }
        while (i < 256) {
            int L = sec[i];
            unsigned nr;
            if (L == 0)
                break;
            if (L == 1) {
                klar = 1;
                break;
            }
            if (L < 4 || i + L > 256) {
                meddela(0, "sektor %ld: fel radlängd %d", s, L);
                break;
            }
            nr = (unsigned) (sec[i + 1] | sec[i + 2] << 8);
            n = rad80(sec + i, L);
            if (n < 0) {
                meddela(0, "rad %u gick inte att lista (%s)", nr, felet);
                ej_listad(nr);
            } else {
                if (jump)
                    meddela(1, "rad %u: LIST kör maskinkod här (LIST-skydd); "
                            "raden står som den är sparad fram till dit", nr);
                till_lista(out, n);
            }
            i += L;
        }
    }
    if (!klar)
        meddela(0, "inget slutmärke ($01)");
}

/* ================================================================== */
/* BASIC II */

/* ROM:en i ABC 802, de delar som behövs: $0600-$0FFF (nyckelord,
   operatorer, funktioner; en okänd kod letas som i ROM:en vidare i
   koden efter tabellerna), $4BF7-$4C08 (satsernas kedja), $4DF2-$4F29
   (tiopotenserna), $6F87-$6F99 (DOS-satserna BYE KILL NAME). DOSINI
   lägger en nod för dem vid $FDD8 (DOSSTH) först i STKWP-kedjan; nästa
   nod är den gamla, $4BF7. */
struct romdel {
    long a, n;
    const byte *d;
};
#include "rom802.h"

/* byten vid a, eller -1 om den inte är med */
static int romp(long a)
{
    int i;
    for (i = 0; i < ANTAL_ROMDELAR; i++)
        if (a >= romdelar[i].a && a < romdelar[i].a + romdelar[i].n)
            return romdelar[i].d[a - romdelar[i].a];
    return -1;
}

static int rom(long a)
{
    int c = romp(a);
    if (c < 0)
        fel("ROM-adress $%04lX", a);
    return c;
}

#define STKW   0x089FL
#define OPKW   0x0625L
#define THENTX 0x0979L
#define FNKWP  0x0661L
#define STKWP  0xFDD8L      /* DOS-noden först, som efter DOSINI */
#define P10    0x4DF2L

/* S3FE4: texten vid p till en kod eller $00 */
static void kwfrom(long p, char *t)
{
    int n = 0, c;
    while ((c = rom(p++)) > 0 && c < 0x80)
        if (n < 255)
            t[n++] = (char) c;
    t[n] = 0;
}

/* DKWTXT: texten efter den första koden a från tab. Med ok != NULL:
   *ok = 0 om ROM:en tar slut, i stället för fel. */
static void kwtext(int a, long tab, char *t, int *ok)
{
    int c;
    for (;;) {
        c = ok ? romp(tab) : rom(tab);
        if (c < 0) {
            *ok = 0;
            return;
        }
        if (c == a)
            break;
        tab++;
    }
    tab++;
    if (ok) {
        int n = 0;
        while ((c = romp(tab++)) > 0 && c < 0x80)
            if (n < 255)
                t[n++] = (char) c;
        t[n] = 0;
        *ok = c >= 0;
        return;
    }
    kwfrom(tab, t);
}

/* DOPTXT: operatorn a. Bara den första av en operators koder står i
   OPKW, så a, a-1, a-2 ... provas. */
static void optext2(int a, char *t)
{
    int n;
    long p;
    for (n = 0; n < 256; n++) {
        for (p = OPKW; p < OPKW + 60; p++)
            if (rom(p) == a) {
                kwfrom(p + 1, t);
                return;
            }
        a = (a - 1) & 0xFF;
    }
    fel("okänd operator");
}

/* FNDTAB: noden i kedjan som har koden; svaret är 1 och texten i t,
   eller 0. Noden är [nästa][C][B][..][..][tabellen]: koderna C ..
   C+antal-1 med prefixet (bit 7 i B: antal = B & 15 + 1, prefix =
   B >> 4 & 7). En kod som inte står i tabellen letar ROM:en efter
   vidare; då hör satsen till en annan maskin. */
static int fndtab(long chain, unsigned kod, char *t)
{
    int d = (int) (kod >> 8), e = (int) (kod & 0xFF), ok;
    long node = chain;
    while (node) {
        long nxt = rom(node) | (long) rom(node + 1) << 8;
        int c = rom(node + 2), b = rom(node + 3), n, pre;
        if (b & 0x80) {
            n = (b & 15) + 1;
            pre = (b >> 4) & 7;
        } else {
            n = b;
            pre = 0;
        }
        if (pre == d && ((e - c) & 0xFF) < n) {
            long tab = rom(node + 6) | (long) rom(node + 7) << 8;
            ok = 1;
            kwtext(((e - c) + 0x80) & 0xFF, tab, t, &ok);
            return ok;
        }
        node = nxt;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* flyttal (DNUM, FTODEC): 8 byte, exponenten först (bias $80, mantissan
   0.1xxx binärt), tecknet i bit 7 av första mantissabyten. Mantissan är
   56 bitar och räkningen 64; tal här har 96 bitar i 6 ord om 16. */

typedef struct {
    unsigned long w[6];         /* w[0] lägst */
} tal;

static void t_noll(tal *x)
{
    memset(x, 0, sizeof *x);
}

static int t_ar_noll(const tal *x)
{
    int i;
    for (i = 0; i < 6; i++)
        if (x->w[i])
            return 0;
    return 1;
}

static int t_bit(const tal *x, int n)
{
    return (int) ((x->w[n >> 4] >> (n & 15)) & 1);
}

static void t_satt(tal *x, int n)
{
    x->w[n >> 4] |= 1UL << (n & 15);
}

static int t_byte(const tal *x, int k)  /* byte k, 0 lägst */
{
    return (int) ((x->w[k >> 1] >> (8 * (k & 1))) & 0xFF);
}

static void t_vanster(tal *x)           /* x <<= 1 */
{
    int i;
    for (i = 5; i > 0; i--)
        x->w[i] = ((x->w[i] << 1) | (x->w[i - 1] >> 15)) & 0xFFFF;
    x->w[0] = (x->w[0] << 1) & 0xFFFF;
}

static void t_hoger(tal *x)             /* x >>= 1 */
{
    int i;
    for (i = 0; i < 5; i++)
        x->w[i] = ((x->w[i] >> 1) | (x->w[i + 1] << 15)) & 0xFFFF;
    x->w[5] >>= 1;
}

static void t_skifta(tal *x, int n)     /* x <<= n, n < 0: x >>= -n */
{
    for (; n > 0; n--)
        t_vanster(x);
    for (; n < 0; n++)
        t_hoger(x);
}

static void t_add(tal *x, const tal *y)
{
    unsigned long c = 0;
    int i;
    for (i = 0; i < 6; i++) {
        c += x->w[i] + y->w[i];
        x->w[i] = c & 0xFFFF;
        c >>= 16;
    }
}

static void t_sub(tal *x, const tal *y)  /* modulo 2^96 */
{
    unsigned long b = 0;
    int i;
    for (i = 0; i < 6; i++) {
        unsigned long s = x->w[i] + 0x10000UL - y->w[i] - b;
        x->w[i] = s & 0xFFFF;
        b = s < 0x10000UL;
    }
}

static int t_jfr(const tal *x, const tal *y)
{
    int i;
    for (i = 5; i >= 0; i--)
        if (x->w[i] != y->w[i])
            return x->w[i] < y->w[i] ? -1 : 1;
    return 0;
}

static void t_maska(tal *x, int n)      /* bara de n lägsta bitarna */
{
    int i;
    for (i = 0; i < 6; i++) {
        if (16 * i >= n)
            x->w[i] = 0;
        else if (16 * i + 16 > n)
            x->w[i] &= (1UL << (n - 16 * i)) - 1;
    }
}

static int t_div10(tal *x)              /* x /= 10, svaret är resten */
{
    unsigned long r = 0;
    int i;
    for (i = 5; i >= 0; i--) {
        r = r << 16 | x->w[i];
        x->w[i] = r / 10;
        r %= 10;
    }
    return (int) r;
}

typedef struct {
    int e;                      /* exponenten */
    tal m;                      /* mantissan, 56 bitar */
} flyt;

/* mantissan ur 7 byte; bit 55 sätts */
static void mantissa(tal *m, const int *b)
{
    int i;
    t_noll(m);
    for (i = 0; i < 7; i++) {
        t_skifta(m, 8);
        m->w[0] |= (unsigned long) b[i];
    }
    t_satt(m, 55);
}

/* 10^n ur ROM:ens tabell vid $4DF2 */
static void p10(int n, flyt *x)
{
    long a = P10 + 8L * n;
    int b[7], i;
    for (i = 0; i < 7; i++)
        b[i] = rom(a + 1 + i);
    x->e = rom(a);
    mantissa(&x->m, b);
}

/* P10EXP: den första tiopotensen n >= 1 med exponent >= a + $81 */
static int p10exp(int a)
{
    int n = 1;
    a = (a + 0x81) & 0xFF;
    while (rom(P10 + 8L * n) < a) {
        n++;
        if (n > 38)
            fel("tiopotens");
    }
    return n;
}

/* antalet mantissabyte till den sista som inte är noll */
static int dlen(const tal *m)
{
    int n = 7;
    while (n > 1 && t_byte(m, 7 - n) == 0)
        n--;
    return n;
}

/* normalisera 64 bitar och avrunda till 56 */
static void rund(tal *acc, int e, flyt *x)
{
    if (t_ar_noll(acc)) {
        x->e = 0;
        t_noll(&x->m);
        return;
    }
    while (!t_bit(acc, 63)) {
        t_vanster(acc);
        e--;
    }
    x->m = *acc;
    t_skifta(&x->m, -8);
    if (t_bit(acc, 7)) {
        tal ett;
        t_noll(&ett);
        ett.w[0] = 1;
        t_add(&x->m, &ett);
        if (t_bit(&x->m, 56)) {
            t_noll(&x->m);
            t_satt(&x->m, 55);
            e++;
        }
    }
    x->e = e;
}

/* DMUL: multiplikatorn är den med kortast mantissa (vid lika:
   tiopotensen). Bit för bit, den minsta först, 64 bitar. */
static void dmul(flyt *x, const flyt *p)
{
    tal mc, mr, acc;
    int i, j, by;
    if (dlen(&x->m) < dlen(&p->m)) {
        mc = p->m;
        mr = x->m;
    } else {
        mc = x->m;
        mr = p->m;
    }
    t_skifta(&mc, 8);
    t_noll(&acc);
    for (i = dlen(&mr) - 1; i >= 0; i--) {
        by = t_byte(&mr, 6 - i);
        for (j = 0; j < 8; j++) {
            if (by & 1)
                t_add(&acc, &mc);
            by >>= 1;
            t_hoger(&acc);
        }
    }
    if (x->e + p->e - 1 < 0x80) {
        x->e = 0;
        t_noll(&x->m);
        return;
    }
    rund(&acc, x->e + p->e - 0x80, x);
}

/* DDIV: 64 kvotbitar, sedan som DMUL */
static void ddiv(flyt *x, const flyt *p)
{
    tal r = x->m, q;
    int i, cy = 0;
    t_noll(&q);
    for (i = 0; i < 64; i++) {
        t_vanster(&q);
        if (cy || t_jfr(&r, &p->m) >= 0) {
            t_sub(&r, &p->m);
            t_maska(&r, 56);
            q.w[0] |= 1;
        }
        t_vanster(&r);
        cy = t_bit(&r, 56);
        t_maska(&r, 56);
    }
    rund(&q, x->e - p->e + 0x81, x);
}

/* FTODEC: flyttalet (8 byte) -> text, som LIST skriver det */
static void ftodec(const byte *k, char *ut)
{
    flyt x, p;
    tal v, f;
    int b[7], i, ix8 = 0, C, D, keep;
    char s[24], t[24];
    char *u;
    int neg = k[1] & 0x80;

    strcpy(ut, "0");
    if (k[0] == 0)
        return;
    b[0] = k[1] | 0x80;
    for (i = 1; i < 7; i++)
        b[i] = k[1 + i];
    x.e = k[0];
    mantissa(&x.m, b);
    for (;;) {
        if (x.e == 0)
            return;
        if (x.e >= 0x81 && x.e < 0xB8) {
            f = x.m;
            t_maska(&f, 0xB8 - x.e);
            if (t_ar_noll(&f))
                break;
        }
        if (x.e <= 0xB8) {
            int n = p10exp(0xB8 - x.e);
            ix8 -= n;
            p10(n, &p);
            dmul(&x, &p);
            continue;
        }
        if (x.e - 0xC0 >= 0) {
            int n = p10exp(x.e - 0xC0);
            ix8 += n;
            p10(n, &p);
            ddiv(&x, &p);
        }
        break;
    }
    v = x.m;
    t_skifta(&v, x.e - 0x80 - 56);
    for (i = 21; i >= 0; i--)           /* 22 siffror */
        s[i] = (char) ('0' + t_div10(&v));
    s[22] = 0;
    C = 22 + ix8;
    for (u = s; *u == '0'; u++)
        C--;
    strcpy(t, u);
    D = (int) strlen(t);
    while (D > 0 && t[D - 1] == '0')
        t[--D] = 0;
    if (!D)
        return;
    keep = t[0] < '5' ? 17 : 16;        /* FMRND */
    if (D > keep) {
        int upp = t[keep] >= '5';
        t[D = keep] = 0;
        if (upp) {
            while (D > 0 && t[D - 1] == '9')
                t[--D] = 0;
            if (D)
                t[D - 1]++;
            else {
                strcpy(t, "1");
                D = 1;
                C++;
            }
        }
        while (D > 0 && t[D - 1] == '0')
            t[--D] = 0;
    }
    u = ut;
    if (neg)
        *u++ = '-';
    if ((C >= 0 && C <= 6) || (C < 0 && C + 6 >= 0 && C + 6 >= D)) {
        if (C <= 0) {
            *u++ = '.';
            for (i = 0; i < -C; i++)
                *u++ = '0';
            strcpy(u, t);
        } else if (C >= D) {
            strcpy(u, t);
            u += D;
            for (i = 0; i < C - D; i++)
                *u++ = '0';
            *u = 0;
        } else {
            memcpy(u, t, (size_t) C);
            u += C;
            *u++ = '.';
            strcpy(u, t + C);
        }
    } else {
        int e = C - 1;
        *u++ = t[0];
        *u++ = '.';
        strcpy(u, t + 1);
        u += D - 1;
        sprintf(u, "E%c%02d", e < 0 ? '-' : '+', e < 0 ? -e : e);
    }
}

/* ------------------------------------------------------------------ */
/* programmet som det står i minnet: sektorernas 253 första byte efter
   varandra. Huvudet: +0 $8F, +2 flaggor (bit 0: länkat), +4 VARBEG,
   +$0A antalet variabler. Raderna från $15: [längd][nr lo][hi] kod $87.
   Vid VARBEG står paren (referens, kod) för variablerna; före VARBEG
   ordet som pekar ut de långa namnen. */

static byte *m;
static long mlen;
static int linked;
static long varbeg;
static unsigned short parkod[65536L];   /* referens -> kod, den första */
static byte harpar[65536L];
static long fornext[65536L];            /* NEXT:s ord -> FOR:s referens */
static long lnkdef;
static int level;                       /* B: nivån för följande rader */
static long asgn;                       /* den senaste tilldelningen */
static long hl2;                        /* HL */
static byte buf[8192];                  /* texten */
static int blen;
static int fs[1024], nfs;               /* textbitarnas början */

/* sant om filen är som kas upp skriver den från bandet: sektorer om
   256 byte, där de 3 sista i varje är nollor (blocken har 253 byte).
   En fil från skivan är programmet i ett stycke. */
static int bandet(const byte *d, long n)
{
    long i;
    if (n % 256)
        return 0;
    for (i = 0; i < n; i += 256)
        if (d[i + 253] || d[i + 254] || d[i + 255])
            return 0;
    return 1;
}

static int b2(long a)
{
    if (a < 0 || a >= mlen)
        fel("utanför programmet");
    return m[a];
}

static unsigned w2(long a)
{
    return (unsigned) (b2(a) | b2(a + 1) << 8);
}

static int rd2(void)                    /* LD A,(HL) : INC HL */
{
    return b2(hl2++);
}

static unsigned rdw(void)               /* LDBCHL */
{
    unsigned v = w2(hl2);
    hl2 += 2;
    return v;
}

static void put(int c)
{
    if (blen >= 4096)
        fel("för lång");
    buf[blen++] = (byte) c;
}

static void puts2(const char *s)
{
    while (*s)
        put((byte) *s++);
}

static void putnum(unsigned long n, const char *fore, const char *efter)
{
    char t[32];
    sprintf(t, "%s%lu%s", fore, n, efter);
    puts2(t);
}

static void dblank(void)                /* DBLANK */
{
    if (!(blen && buf[blen - 1] == ' '))
        put(' ');
}

static void avsista(void)               /* den sista byten bort */
{
    if (!blen)
        fel("tom text");
    blen--;
}

static void lagg(int p)
{
    if (nfs >= 1024)
        fel("stacken");
    fs[nfs++] = p;
}

static void push2(void)                 /* PUSH DE */
{
    lagg(blen);
}

static int pop2(void)                   /* POP BC */
{
    if (!nfs)
        fel("stacken");
    return fs[--nfs];
}

/* DMERGE: den senaste textbiten flyttas före den under */
static void dmerge(void)
{
    static byte tmp[sizeof buf];
    int s = pop2(), f = pop2(), n;
    if (f < 0 || f > s || s > blen)
        fel("DMERGE");
    n = blen - s;
    memcpy(tmp, buf + s, (size_t) n);
    memmove(buf + f + n, buf + f, (size_t) (s - f));
    memcpy(buf + f, tmp, (size_t) n);
    lagg(f);
    lagg(s);
}

static void insert(int at, int c)       /* INSCHR */
{
    if (at < 0 || at > blen)
        fel("INSCHR");
    put(0);
    memmove(buf + at + 1, buf + at, (size_t) (blen - 1 - at));
    buf[at] = (byte) c;
}

static void dcomma(int n)               /* DCOMMA */
{
    for (; n > 1; n--)
        insert(pop2(), ',');
}

/* L307D: namnet på plats r i DEF FN:s parametrar och LOCAL efter dem.
   Strängarna i LOCAL har ett ord till (storleken). */
static unsigned local(int r)
{
    long p = lnkdef;
    int a, c, i, s;
    if (!p)
        fel("lokal variabel utanför DEF");
    a = (b2(p) + 1) & 0xFF;
    c = ((r >> 1) + 1) & 0xFF;
    for (i = 0; i < 1000; i++) {
        a = (a - 1) & 0xFF;
        if (a != 0) {
            p++;
            c = (c - 1) & 0xFF;
            if (c == 0)
                return w2(p);
            s = b2(p) & 2;
            p++;
            if (!s)
                continue;
            a = (a + 1) & 0xFF;
        }
        a = (a - 1) & 0xFF;             /* L3094 */
        if (a < 0x80)
            continue;
        p += 2;
    }
    fel("L307D");
    return 0;
}

/* S3032: ordet i koden. Länkat program: radnumret, FN-namnets kod
   eller variabelns kod ur adressen; annars ordet som det är. */
static unsigned ref(int kind)
{
    unsigned bc = rdw(), c;
    long after = hl2;
    int d;
    if (!linked)
        return bc;
    if (kind == 9)                              /* radnummer */
        return w2((after + bc + 1) & 0xFFFF);
    if (kind == 0x0B)                           /* FN-namn */
        return w2((after + bc - 5) & 0xFFFF);
    if (kind == 0x0A) {
        if (bc >> 8 == 0xFF)                    /* lokal */
            return local((int) (bc & 0xFF));
        if (harpar[bc])
            return parkod[bc];
        fel("variabeln $%04X saknas", bc);
    }
    if (kind == 7) {
        /* NEXT: ordet är FOR-blockets referens - 2, och blocket finns
           bara när programmet körs. FOR-satsens länk ($BC ord) pekar på
           det här ordet, så variabeln är den som FOR satte. */
        long r = after >= 2 && after - 2 < 65536L ? fornext[after - 2] : -1;
        if (r >= 0 && harpar[r])
            return parkod[r];
        for (d = 2; d <= 8; d *= 2) {
            unsigned k = (bc - d) & 0xFFFF;
            if (!harpar[k])
                continue;
            c = parkod[k];
            if ((d == 2) == ((c & 1) != 0) && !(c & 2))
                return c;
        }
        if (bc == 0)
            return 0;
        fel("variabeln i NEXT");
    }
    fel("S3032");
    return 0;
}

/* NAMTX1: variabelns namn ur koden. (kod >> 3) - $3FF >= 0: ett långt
   namn i listan före VARBEG; annars bokstav, siffra (15: ingen) och $
   (bit 1) eller % (bit 0). */
static void namtx1(unsigned k)
{
    long idx = (long) (k >> 3) - 0x3FF;
    if (idx >= 0) {
        long v = varbeg - 2, o = (long) w2(v), p;
        int n = 0, c;
        if (o & 0x8000)
            o -= 0x10000L;
        p = ((v + o) & 0xFFFF) + 4 + idx;
        for (;;) {
            c = b2(p++);
            if (c == 0 || c >= 0x80)
                break;
            put(c);
            n++;
        }
        if (!n)
            fel("långt namn");
    } else {
        int d = (int) ((k >> 3) & 15);
        put((int) ((k >> 8) | 0x40));
        if (d != 15)
            put(d | 0x30);
    }
    if (k & 2)
        put('$');
    else if (k & 1)
        put('%');
}

static void namtxt(void)                /* NAMTXT */
{
    namtx1(rdw());
}

static void s3de4(int kind)             /* S3DE4, DVNAME */
{
    unsigned c = ref(kind);
    if (c)
        namtx1(c);
}

/* S3FBB: funktionens eller satsens namn ur kedjan */
static void fname(long chain, const char *xname)
{
    char t[256];
    int a = rd2();
    unsigned k = (unsigned) a;
    if (a >= 0xF9)
        k = (unsigned) (a - 0xF8) << 8 | (unsigned) rd2();
    if (fndtab(chain, k, t))
        puts2(t);
    else
        putnum(k, xname, "");
}

static void paren2(void)                /* L3DB5 */
{
    put('(');
    dmerge();
    pop2();
    put(')');
}

/* DOPR: operatorn sist och flyttad före den högra operanden.
   flags bit 0: blanksteg runt, bit 1: en operand (NOT). */
static void dopr(int op, int flags)
{
    char t[256];
    int bc = nfs ? fs[nfs - 1] : 0;
    push2();
    if ((flags & 1) && bc > 0 && buf[bc - 1] != ' ')
        put(' ');
    optext2(op, t);
    puts2(t);
    if ((flags & 1) && bc < blen && buf[bc] != ' ')
        put(' ');
    dmerge();
    pop2();
    if (!(flags & 2))
        pop2();
}

/* DDIM, DCOMMN: en post i DIM eller COMMON. Svaret är sant när listan
   är slut. */
static int ddim(int common)
{
    int typ = rd2(), arr, ix, bc, n;
    if (!(typ & 2))
        lagg(0);                                /* ingen storlek */
    push2();
    put(')');
    arr = typ & 8;
    if (!arr) {
        avsista();
        s3de4(0x0A);
    }
    ix = pop2();
    bc = pop2();
    if (bc) {                                   /* =storleken */
        lagg(bc);
        lagg(ix);
        put('=');
        dmerge();
        pop2();
        pop2();
    }
    if (arr) {
        n = rd2();
        for (;;) {
            bc = pop2();
            if (bc)
                insert(bc, ':');                /* undre:övre */
            if (--n <= 0)
                break;
            insert(pop2(), ',');
        }
        push2();
        s3de4(0x0A);
        put('(');
        dmerge();
        pop2();
        pop2();
    }
    if (common)
        hl2 += 2;
    if (b2(hl2) < 0xC0)
        return 1;
    put(',');
    /* nästa post: DECOMP läser vidare och kommer tillbaka hit */
    return 0;
}

/* DECTAB, $A7-$CF. Svaret är sant när uttrycket är slut. */
static int dectab(int a)
{
    char t[256];
    int n, q, c;
    if (a == 0xA7) {                            /* DPAREN */
        push2();
        paren2();
    } else if (a == 0xA8 || a == 0xA9) {        /* DNEG */
        push2();
        put('-');
        dmerge();
        pop2();
    } else if (a >= 0xAA && a <= 0xAE) {        /* DSKIP */
        hl2++;
    } else if (a == 0xAF) {                     /* DNOT */
        dopr(a, 3);
    } else if (a == 0xB0 || a == 0xC3) {        /* DEMPTY */
        lagg(0);
    } else if (a >= 0xB1 && a <= 0xB3) {        /* DASGN1 */
        insert(pop2(), '=');
        pop2();
        return 1;
    } else if (a >= 0xB4 && a <= 0xB6) {        /* DASGN */
        asgn = (long) w2(hl2);
        push2();
        s3de4(0x0A);
        put('=');
        dmerge();
        pop2();
        pop2();
        return 1;
    } else if ((a >= 0xB7 && a <= 0xBC) || a == 0xBF) {  /* DDROP */
        pop2();
        return 1;
    } else if (a == 0xBD) {                     /* DTHEN */
        dblank();
        kwfrom(THENTX, t);
        puts2(t);
        hl2++;
        pop2();
        return 1;
    } else if (a == 0xBE) {
        hl2++;
        pop2();
        return 1;
    } else if (a == 0xC0 || a == 0xC1 || a == 0xC2 ||
               a == 0xC4 || a == 0xC5 || a == 0xC6) {   /* DVAR */
        push2();
        s3de4(0x0A);
    } else if (a == 0xC7) {                     /* DINT */
        push2();
        putnum(rdw(), "", "%");
    } else if (a >= 0xC8 && a <= 0xCA) {        /* DARR */
        dcomma(rd2());
        push2();
        s3de4(0x0A);
        paren2();
    } else if (a == 0xCB || a == 0xCC) {        /* DSTR, DSTR2 */
        q = a == 0xCB ? '"' : '\'';
        push2();
        n = rd2();
        put(q);
        while (n-- > 0) {
            c = rd2();
            put(c);
            if (c == q)
                put(q);
        }
        put(q);
    } else if (a == 0xCD || a == 0xCE) {        /* DDIM, DCOMMN */
        return ddim(a == 0xCE);
    } else if (a == 0xCF) {                     /* DFN */
        n = rd2();
        hl2++;
        if (n)
            dcomma(n);
        push2();
        puts2("FN");
        s3de4(0x0B);
        if (n)
            paren2();
    }
    return 0;
}

/* DECOMP: uttryckets kod (omvänd polsk notation) -> text. Varje
   operand är en textbit; fs har deras början. */
static void decomp(void)
{
    for (;;) {
        int a = rd2();
        if (a >= 0xDD) {                        /* DSMALL: 0-34 */
            push2();
            putnum((unsigned long) (a - 0xDD), "", "%");
        } else if (a >= 0xD5) {                 /* DNUM */
            byte k[8];
            char t[40];
            int n = a - 0xD4, i;
            push2();
            memset(k, 0, sizeof k);
            for (i = 0; i < n; i++)
                k[i] = (byte) b2(hl2 + i);
            hl2 += n;
            ftodec(k, t);
            puts2(t);
        } else if (a >= 0xD0) {                 /* DFUNC */
            int n = a - 0xD0;
            if (n >= 4)
                n = (rd2() - 0xD0) & 0xFF;
            if (n) {
                dcomma(n);
                push2();
                fname(FNKWP, "XFN");
                paren2();
            } else {
                push2();
                fname(FNKWP, "XFN");
            }
        } else if (a >= 0xA7) {
            if (dectab(a))
                return;
        } else if (a >= 0x89)                   /* DOP */
            dopr(a, 0);
        else if (a >= 0x84)                     /* DOPSP */
            dopr(a, 1);
    }
}

/* DNAMES: n namn med komma emellan; c bit 1: strängarna med
   =storleken (LOCAL) */
static void s422e(int n, int c)
{
    int i, a;
    for (i = 0; i < n; i++) {
        a = b2(hl2);
        namtxt();
        if ((a & 2) && (c & 2))
            putnum(rdw(), "=", "");
        put(',');
    }
    if (n)
        avsista();
}

static void dtext(void)                 /* DTEXT: längd och text, $87 sist */
{
    int n = rd2(), i;
    if (n)
        b2(hl2 + n - 1);
    for (i = 0; i < n - 1; i++)
        put(b2(hl2 + i));
    hl2 += n;
}

static void dedent(int *B, int *C)      /* DDEDNT */
{
    if (*B) {
        (*B)--;
        if (*C > *B)
            *C = *B;
    }
    dblank();
}

/* DLINE: raden vid p -> text som slutar med CR i buf[]. Svaret är
   längden. */
static int dline(long p)
{
    char t[256];
    int B, C, start, a, b, c, n, i;

    hl2 = p + 1;
    blen = 0;
    nfs = 0;
    B = C = level;
    putnum(rdw(), "", "");
    start = blen;
    dblank();
    for (;;) {                                  /* L40EE */
        a = b2(hl2);
        if (a == 0xBC) {                        /* FOR: länken till NEXT */
            unsigned ln = w2(hl2 + 1);
            if (ln && asgn >= 0)
                fornext[(hl2 + 3 + ln - 1) & 0xFFFF] = asgn;
            hl2 += 3;
            continue;
        }
        if (a == 0x87) {                        /* radslut */
            hl2++;
            break;
        }
        if (a >= 0xC0) {
            decomp();
            if (b2(hl2) >= 0xC0)
                dblank();
            continue;
        }
        if (a < 0x80) {
            put(a);
            hl2++;
            continue;
        }
        if (a == 0xB7) {                        /* DVARS */
            hl2++;
            n = 1;
            while (!(rd2() & 4))
                n++;
            for (;;) {
                decomp();
                n--;
                if (n || b2(hl2) == 0xB7)
                    put(',');
                if (!n)
                    break;
            }
            continue;
        }
        dblank();                               /* ett nyckelord */
        a = rd2();
        b = 1;
        if (a == 0xBF) {                        /* ON: antal och kod */
            b = rd2();
            a = rd2();
        }
        c = a;
        if (a >= 0x9D && a < 0xA0)
            a = (a + 0xE6) & 0xFF;
        if (a == 0x86)
            fname(STKWP, "XSTM");
        else {
            kwtext(a, STKW, t, NULL);
            puts2(t);
        }
        dblank();
        if (c < 0x86) {                         /* DLNUMS */
            for (i = 0; i < b; i++) {
                if (i)
                    put(',');
                putnum(ref(9), "", "");
            }
            continue;
        }
        if (c == 0x8C) {                        /* DEF FN */
            avsista();
            hl2 += 2;
            namtxt();
            hl2 += 2;
            lnkdef = hl2;
            n = rd2();
            if (n) {
                put('(');
                s422e(n, 0);
                put(')');
            }
            B++;
            if (b2(hl2) < 0xC0)
                continue;
            put('=');
            decomp();
            lnkdef = 0;
            dedent(&B, &C);
        } else if (c == 0x8A) {                 /* ELSE */
            hl2++;
        } else if (c == 0xB6) {                 /* LOCAL */
            s422e(rd2(), 2);
        } else if (c == 0x8F || c == 0x90) {    /* REM, ! */
            dtext();
            break;
        } else if (c == 0x8E) {                 /* DATA */
            hl2 += 2;
            dtext();
            break;
        } else if (c == 0x9A) {                 /* WHILE */
            hl2 += 2;
            B++;
            dblank();
        } else if (c == 0x9B) {                 /* WEND */
            hl2 += 2;
            dedent(&B, &C);
        } else if (c == 0x8D) {                 /* FNEND */
            lnkdef = 0;
            dedent(&B, &C);
        } else if (c == 0x98) {                 /* FOR */
            B++;
            dblank();
        } else if (c == 0x99) {                 /* NEXT */
            s3de4(7);
            dedent(&B, &C);
        } else if (c == 0xA2) {                 /* MID$ */
            avsista();
            decomp();
            if (b2(hl2) >= 0xC0)
                dblank();
        } else if (c == 0x8B) {                 /* ?: tecknet över blanket före */
            if (blen < 2)
                fel("tom text");
            buf[blen - 2] = buf[blen - 1];
            blen--;
            dtext();
            break;
        }
    }
    level = B;
    if (C) {                                    /* 2 blanka per nivå */
        if (start >= blen || blen + 2 * C + 1 > (int) sizeof buf)
            fel("för lång");
        memmove(buf + start + 2 * C, buf + start, (size_t) (blen - start));
        memset(buf + start, buf[start + 2 * C], (size_t) (2 * C));
        blen += 2 * C;
    }
    buf[blen++] = '\r';
    return blen;
}

static int rad2(long p)
{
    if (setjmp(hopp))
        return -1;
    return dline(p);
}

static int huvud2(void)
{
    long i, n, pp;
    if (setjmp(hopp))
        return -1;
    if (mlen < 3)
        fel("för kort");
    linked = m[2] & 1;
    varbeg = (long) w2(4);
    memset(harpar, 0, sizeof harpar);
    n = (long) w2(0x0A);
    for (i = 0; i < n; i++) {
        unsigned r;
        pp = varbeg + 4 * i;
        if (pp + 4 > mlen)
            break;
        r = w2(pp);
        if (!harpar[r]) {
            harpar[r] = 1;
            parkod[r] = (unsigned short) w2(pp + 2);
        }
    }
    for (i = 0; i < 65536L; i++)
        fornext[i] = -1;
    lnkdef = 0;
    level = 0;
    asgn = -1;
    return 0;
}

/* BASIC II: raderna från $15 till längden 1 */
static void list2(const byte *data, long size)
{
    long i;
    if (bandet(data, size)) {
        m = (byte *) minne(NULL, (size_t) (size / 256 * 253));
        for (mlen = 0, i = 0; i < size; i += 256, mlen += 253)
            memcpy(m + mlen, data + i, 253);
    } else {
        m = (byte *) minne(NULL, (size_t) size);
        memcpy(m, data, (size_t) size);
        mlen = size;
    }
    if (huvud2() < 0) {
        meddela(0, "för kort");
        free(m);
        return;
    }
    i = 0x15;
    for (;;) {
        int L, n;
        unsigned nr;
        if (i >= mlen) {
            meddela(0, "inget slutmärke (längden 1)");
            break;
        }
        L = m[i];
        if (L == 1)
            break;
        if (L < 4) {
            meddela(0, "fel radlängd %d vid $%04lX", L, i);
            break;
        }
        nr = i + 2 < mlen ? (unsigned) (m[i + 1] | m[i + 2] << 8) : 0;
        n = rad2(i);
        if (n < 0) {
            meddela(0, "rad %u gick inte att lista (%s)", nr, felet);
            ej_listad(nr);
        } else
            till_lista(buf, n);
        i += L;
    }
    free(m);
}

/* ================================================================== */

static void bac2bas(const byte *data, long size)
{
    if (!size) {
        meddela(0, "tom");
        return;
    }
    if (data[0] == 0x8F) {
        list2(data, size);
        return;
    }
    if (data[0] != 0x82)
        meddela(1, "börjar inte med $82 eller $8F, ingen BAC-fil?");
    list80(data, size);
}

static byte *las_allt(FILE *f, long *n)
{
    byte *d = NULL;
    long cap = 0;
    size_t k;
    *n = 0;
    for (;;) {
        if (*n == cap) {
            cap = cap ? 2 * cap : 65536L;
            d = (byte *) minne(d, (size_t) cap);
        }
        k = fread(d + *n, 1, (size_t) (cap - *n), f);
        if (k == 0)
            break;
        *n += (long) k;
    }
    return d;
}

static void anvandning(void)
{
    fprintf(stderr, "användning: bac2bas [--kod 7|dos|utf8] "
            "[--rad cr|crlf|lf] [-o UT] FIL.BAC ...\n");
    exit(2);
}

static void en_fil(const char *fil)
{
    byte *d;
    long n;
    FILE *f;
    if (!strcmp(fil, "-")) {
        namnet = "stdin";
        d = las_allt(stdin, &n);
    } else {
        namnet = fil;
        if (!(f = fopen(fil, "rb"))) {
            meddela(0, "går inte att öppna");
            return;
        }
        d = las_allt(f, &n);
        fclose(f);
    }
    bac2bas(d, n);
    free(d);
}

int main(int argc, char **argv)
{
    int i, kod = KOD_UTF8, filer = 0;
    const char *rad = rad_text("lf"), *utnamn = NULL;
    FILE *ut = stdout;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i], *v;
        if (!strncmp(a, "--kod", 5) || !strncmp(a, "--rad", 5)) {
            if (a[5] == '=')
                v = a + 6;
            else if (!a[5] && i + 1 < argc) {
                argv[i] = NULL;
                v = argv[++i];
            } else
                anvandning();
            if (a[2] == 'k') {
                if ((kod = kod_nr(v)) < 0)
                    anvandning();
            } else if (!(rad = rad_text(v)))
                anvandning();
            argv[i] = NULL;
        } else if (!strcmp(a, "-o")) {
            if (i + 1 >= argc)
                anvandning();
            argv[i] = NULL;
            utnamn = argv[++i];
            argv[i] = NULL;
        } else if (!strcmp(a, "-h") || !strcmp(a, "--help"))
            anvandning();
        else if (a[0] == '-' && a[1])
            anvandning();
    }
    for (i = 1; i < argc; i++)
        if (argv[i]) {
            en_fil(argv[i]);
            filer++;
        }
    if (!filer)
        en_fil("-");
    if (utnamn && !(ut = fopen(utnamn, "wb"))) {
        fprintf(stderr, "bac2bas: %s går inte att skriva\n", utnamn);
        return 2;
    }
    text_ut(ut, lista, llen, kod, rad);
    if (ut != stdout)
        fclose(ut);
    return antal_fel ? 1 : 0;
}
