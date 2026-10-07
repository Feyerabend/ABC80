/*
 * z80asm.c -- assembler för Z80, till boken ABC80 inifrån
 *
 *     cc -O2 -o z80asm z80asm.c
 *     z80asm [-o fil.bin] [-x fil.hex] [-l fil.lst] källa.asm
 *
 * Det första passet räknar ut adresserna till alla etiketter (fler pass
 * om EQU eller DEFS beror på symboler längre ned), det sista skriver
 * koden. Utfilen (standard: källans namn med .bin) börjar
 * på den lägsta adress som har fått en byte och slutar på den högsta;
 * luckor mellan två ORG fylls med $FF, som en tom EPROM. -x skriver
 * samma bytes som Intel HEX, och -l en listning med adresser, bytes och
 * källraderna, och sist symbolerna.
 *
 * Källan:
 *     ETIKETT: INSTR operand,operand   ; kommentar
 * En etikett står först på raden (kolon behövs inte där) eller slutar
 * med kolon. Instruktionerna och registren skrivs som i Zilogs
 * handbok, med versaler eller gemener; etiketterna skiljer på dem.
 *
 * Direktiv:
 *     ORG  uttr             nästa byte hamnar på adressen
 *     namn EQU uttr         symbol (även  namn = uttr)
 *     DEFB uttr|"text",...  bytes (även DB, DEFM, DM)
 *     DEFW uttr,...         ord, låg byte först (även DW)
 *     DEFS antal[,byte]     antal bytes med värdet byte, standard 0 (DS)
 *     END                   resten av filen läses inte
 *
 * Tal: 255, $FF, 0FFH, 0xFF, %1010 och 'A'. $ ensamt är adressen där
 * raden börjar. Operatorer, svagast först: | ^ & << >> + - * / % och
 * framför ett tal - + ~. Parenteser grupperar, men en operand som helt
 * står inom parentes är en minnesadress: LD A,(BUF+1).
 *
 * Bara de dokumenterade instruktionerna finns; IXH, SLL och de andra
 * odokumenterade skrivs med DEFB.
 */

#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- */
/* Minnet och utdata                                                 */

static unsigned char bild[65536];   /* de assemblerade bytena */
static unsigned char satt[65536];   /* 1 där en byte har skrivits */
static int varv;                    /* passets nummer, 1, 2 ... */
static int sista;                   /* 1 i det sista passet, som skriver */
static int olosta;                  /* det som inte gick att räkna ut i passet */
static long pc;                     /* nästa adress */
static long radpc;                  /* adressen där raden började ($) */

static const char *filnamn;
static int radnr;
static int antalfel;
static jmp_buf radslut;             /* fel: hoppa till nästa rad */

/* Felet skrivs och räknas i det sista passet, så att varje fel kommer
 * en gång; i de andra passen hoppar raden bara över resten. */
static void fel(const char *fmt, ...)
{
    va_list ap;
    if (sista) {
        fprintf(stderr, "%s:%d: ", filnamn, radnr);
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        fputc('\n', stderr);
        antalfel++;
    }
    longjmp(radslut, 1);
}

/* Listningens bytes för den aktuella raden. */
static unsigned char radbytes[4096];
static int nradbytes;

static void ut(long v)
{
    if (pc > 0xFFFF)
        fel("koden går förbi $FFFF");
    if (sista) {
        if (satt[pc])
            fel("adressen $%04lX har redan fått en byte", pc);
        bild[pc] = (unsigned char)v;
        satt[pc] = 1;
        if (nradbytes < (int)sizeof radbytes)
            radbytes[nradbytes++] = (unsigned char)v;
    }
    pc++;
}

/* Värden kontrolleras först i det sista passet, när alla symboler är kända. */
static void ut8(long v)
{
    if (sista && (v < -128 || v > 255))
        fel("värdet %ld ryms inte i en byte", v);
    ut(v & 0xFF);
}

static void ut16(long v)
{
    if (sista && (v < -32768 || v > 65535))
        fel("värdet %ld ryms inte i ett ord", v);
    ut(v & 0xFF);
    ut((v >> 8) & 0xFF);
}

static void utd(long d)              /* förskjutningen i (IX+d) */
{
    if (sista && (d < -128 || d > 127))
        fel("förskjutningen %ld är utanför -128..127", d);
    ut(d & 0xFF);
}

/* ---------------------------------------------------------------- */
/* Symboler                                                          */

typedef struct Sym {
    struct Sym *nasta;
    long varde;
    int varv;                        /* passet då värdet sattes, 0 = inget */
    char namn[];
} Sym;

#define HASH 4096
static Sym *symtab[HASH];
static int antalsym;

static unsigned hash(const char *s)
{
    unsigned h = 5381;
    while (*s)
        h = h * 33 + (unsigned char)*s++;
    return h % HASH;
}

static Sym *sok(const char *namn, int skapa)
{
    unsigned h = hash(namn);
    for (Sym *s = symtab[h]; s; s = s->nasta)
        if (strcmp(s->namn, namn) == 0)
            return s;
    if (!skapa)
        return NULL;
    Sym *s = calloc(1, sizeof *s + strlen(namn) + 1);
    if (!s) {
        fprintf(stderr, "z80asm: minnet tog slut\n");
        exit(1);
    }
    strcpy(s->namn, namn);
    s->nasta = symtab[h];
    symtab[h] = s;
    antalsym++;
    return s;
}

/* En etikett eller EQU. Ett namn får bara definieras en gång per pass.
 * Ändras värdet från förra passet behövs ett pass till; i det sista
 * passet är det ett fel. */
static void definiera(const char *namn, long v)
{
    Sym *s = sok(namn, 1);
    if (s->varv == varv)
        fel("%s är redan definierad", namn);
    if (s->varv && s->varde != v) {
        if (sista && !antalfel)              /* annars följer det av ett fel */
            fel("%s har fått ett annat värde i sista passet ($%04lX, $%04lX)",
                namn, s->varde, v);
        olosta++;
    }
    s->varde = v;
    s->varv = varv;
}

/* ---------------------------------------------------------------- */
/* Uttryck                                                           */

static const char *up;               /* läspekaren i uttrycket */
static int okand;                    /* uttrycket använde en okänd symbol */

static void blank(void)
{
    while (*up == ' ' || *up == '\t')
        up++;
}

static int idtecken(int c)
{
    return isalnum(c) || c == '_' || c == '.';
}

static long uttryck(void);

static long tal(void)
{
    const char *b = up;
    while (isalnum((unsigned char)*up))
        up++;
    int n = (int)(up - b);
    char s[64];
    if (n >= (int)sizeof s)
        fel("för långt tal");
    memcpy(s, b, n);
    s[n] = 0;
    char *slut;
    long v;
    if (n > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        v = strtol(s + 2, &slut, 16);
    else if (s[n - 1] == 'H' || s[n - 1] == 'h') {
        s[n - 1] = 0;
        v = strtol(s, &slut, 16);
    } else
        v = strtol(s, &slut, 10);
    if (*slut)
        fel("felaktigt tal: %.*s", n, b);
    return v;
}

static long primar(void)
{
    blank();
    char c = *up;
    if (c == '(') {
        up++;
        long v = uttryck();
        blank();
        if (*up != ')')
            fel(") saknas");
        up++;
        return v;
    }
    if (c == '-') { up++; return -primar(); }
    if (c == '+') { up++; return primar(); }
    if (c == '~') { up++; return ~primar(); }
    if (c == '$') {
        up++;
        if (!isxdigit((unsigned char)*up))
            return radpc;
        const char *b = up;
        char *slut;
        long v = strtol(up, &slut, 16);
        up = slut;
        if (idtecken(*up))
            fel("felaktigt hextal: $%s", b);
        return v;
    }
    if (c == '%') {
        up++;
        if (*up != '0' && *up != '1')
            fel("felaktigt binärtal");
        long v = 0;
        while (*up == '0' || *up == '1')
            v = v * 2 + (*up++ - '0');
        return v;
    }
    if (c == '\'') {
        if (up[1] == 0 || up[2] != '\'')
            fel("felaktig teckenkonstant");
        long v = (unsigned char)up[1];
        up += 3;
        return v;
    }
    if (isdigit((unsigned char)c))
        return tal();
    if (idtecken(c)) {
        char namn[256];
        int n = 0;
        while (idtecken(*up) && n < (int)sizeof namn - 1)
            namn[n++] = *up++;
        namn[n] = 0;
        Sym *s = sok(namn, 0);
        if (s && s->varv)
            return s->varde;
        if (sista)
            fel("okänd symbol: %s", namn);
        okand = 1;
        return 0;
    }
    fel("uttryck väntades: %s", up);
    return 0;
}

static long muldiv(void)
{
    long v = primar();
    for (;;) {
        blank();
        char c = *up;
        if (c != '*' && c != '/' && c != '%')
            return v;
        up++;
        long w = primar();
        if (c == '*')
            v *= w;
        else if (w == 0) {
            if (sista)
                fel("division med noll");
            v = 0;
        } else
            v = c == '/' ? v / w : v % w;
    }
}

static long addsub(void)
{
    long v = muldiv();
    for (;;) {
        blank();
        if (*up == '+') { up++; v += muldiv(); }
        else if (*up == '-') { up++; v -= muldiv(); }
        else return v;
    }
}

static long skift(void)
{
    long v = addsub();
    for (;;) {
        blank();
        if (up[0] == '<' && up[1] == '<') { up += 2; v <<= addsub(); }
        else if (up[0] == '>' && up[1] == '>') { up += 2; v >>= addsub(); }
        else return v;
    }
}

static long och(void)
{
    long v = skift();
    for (;;) {
        blank();
        if (*up != '&') return v;
        up++;
        v &= skift();
    }
}

static long xeller(void)
{
    long v = och();
    for (;;) {
        blank();
        if (*up != '^') return v;
        up++;
        v ^= och();
    }
}

static long uttryck(void)
{
    long v = xeller();
    for (;;) {
        blank();
        if (*up != '|') return v;
        up++;
        v |= xeller();
    }
}

/* Hela texten s ska vara ett uttryck. */
static long varde(const char *s)
{
    up = s;
    long v = uttryck();
    blank();
    if (*up)
        fel("oväntat i uttrycket: %s", up);
    return v;
}

/* ---------------------------------------------------------------- */
/* Operander                                                         */

enum {
    O_R,        /* 8-bitsregister 0-7: B C D E H L (HL) A; 6 även (IX+d) */
    O_RP,       /* registerpar 0-3: BC DE HL SP; HL även IX, IY */
    O_AF, O_AFX,                     /* AF och AF' */
    O_I, O_RR,                       /* I och R */
    O_BCI, O_DEI, O_SPI, O_CI,       /* (BC) (DE) (SP) (C) */
    O_TAL,      /* ett värde */
    O_MEM,      /* (adress) */
};

typedef struct {
    int typ;
    int reg;
    int pfx;            /* $DD för IX, $FD för IY, annars 0 */
    long v;             /* värdet, adressen eller förskjutningen */
    char text[256];     /* operanden som den står, i versaler */
} Op;

static const char *R8[] = { "B", "C", "D", "E", "H", "L", "", "A" };

static void stora(char *d, const char *s, int max)
{
    int i;
    for (i = 0; s[i] && i < max - 1; i++)
        d[i] = (char)toupper((unsigned char)s[i]);
    d[i] = 0;
}

static int indexpfx(const char *u)       /* "IX" eller "IY" först */
{
    if (u[0] == 'I' && (u[1] == 'X' || u[1] == 'Y') && !idtecken(u[2]))
        return u[1] == 'X' ? 0xDD : 0xFD;
    return 0;
}

static void operand(const char *s, Op *o)
{
    char u[256];
    stora(u, s, sizeof u);
    strcpy(o->text, u);
    o->pfx = 0;
    o->v = 0;
    for (int r = 0; r < 8; r++)
        if (r != 6 && strcmp(u, R8[r]) == 0) {
            o->typ = O_R; o->reg = r;
            return;
        }
    static const char *RP[] = { "BC", "DE", "HL", "SP" };
    for (int r = 0; r < 4; r++)
        if (strcmp(u, RP[r]) == 0) {
            o->typ = O_RP; o->reg = r;
            return;
        }
    if ((o->pfx = indexpfx(u)) && u[2] == 0) {
        o->typ = O_RP; o->reg = 2;
        return;
    }
    if (strcmp(u, "AF") == 0) { o->typ = O_AF; return; }
    if (strcmp(u, "AF'") == 0) { o->typ = O_AFX; return; }
    if (strcmp(u, "I") == 0) { o->typ = O_I; return; }
    if (strcmp(u, "R") == 0) { o->typ = O_RR; return; }

    /* Inom parentes: (HL), (IX+d), (BC) ... eller en adress. */
    int n = (int)strlen(s);
    if (s[0] == '(' && s[n - 1] == ')') {
        int djup = 0, i;
        for (i = 0; i < n; i++) {
            if (s[i] == '(') djup++;
            else if (s[i] == ')' && --djup == 0) break;
        }
        if (i == n - 1) {                /* parentesen omsluter allt */
            char inre[256], ui[256];
            snprintf(inre, sizeof inre, "%.*s", n - 2, s + 1);
            char *b = inre;
            while (*b == ' ' || *b == '\t') b++;
            char *e = b + strlen(b);
            while (e > b && (e[-1] == ' ' || e[-1] == '\t')) *--e = 0;
            stora(ui, b, sizeof ui);
            if (strcmp(ui, "HL") == 0) { o->typ = O_R; o->reg = 6; return; }
            if (strcmp(ui, "BC") == 0) { o->typ = O_BCI; return; }
            if (strcmp(ui, "DE") == 0) { o->typ = O_DEI; return; }
            if (strcmp(ui, "SP") == 0) { o->typ = O_SPI; return; }
            if (strcmp(ui, "C") == 0) { o->typ = O_CI; return; }
            if ((o->pfx = indexpfx(ui))) {
                const char *r = b + 2;
                while (*r == ' ' || *r == '\t') r++;
                if (*r && *r != '+' && *r != '-')
                    fel("felaktig indexoperand: %s", s);
                o->typ = O_R; o->reg = 6;
                o->v = *r ? varde(r) : 0;
                return;
            }
            o->typ = O_MEM;
            o->v = varde(b);
            return;
        }
    }
    o->typ = O_TAL;
    o->v = varde(s);
}

/* ---------------------------------------------------------------- */
/* Instruktionerna                                                   */

/* Prefix, operationskod och (för (IX+d)) förskjutningen. */
static void rop(const Op *o, int kod)
{
    if (o->pfx) ut(o->pfx);
    ut(kod);
    if (o->pfx && o->typ == O_R && o->reg == 6) utd(o->v);
}

static int villkor(const char *u, int bara4)
{
    static const char *CC[] = { "NZ", "Z", "NC", "C", "PO", "PE", "P", "M" };
    for (int k = 0; k < (bara4 ? 4 : 8); k++)
        if (strcmp(u, CC[k]) == 0)
            return k;
    return -1;
}

static void relativ(long mal)
{
    long e = mal - (radpc + 2);
    if (sista && (e < -128 || e > 127))
        fel("hoppet når inte (%ld bytes)", e);
    ut(e & 0xFF);
}

typedef struct { const char *namn; int kod; } Enkel;

static const Enkel ENKLA[] = {
    { "NOP", 0x00 }, { "RLCA", 0x07 }, { "RRCA", 0x0F }, { "RLA", 0x17 },
    { "RRA", 0x1F }, { "DAA", 0x27 }, { "CPL", 0x2F }, { "SCF", 0x37 },
    { "CCF", 0x3F }, { "HALT", 0x76 }, { "EXX", 0xD9 }, { "DI", 0xF3 },
    { "EI", 0xFB },
    { "NEG", 0xED44 }, { "RETN", 0xED45 }, { "RETI", 0xED4D },
    { "RRD", 0xED67 }, { "RLD", 0xED6F },
    { "LDI", 0xEDA0 }, { "CPI", 0xEDA1 }, { "INI", 0xEDA2 }, { "OUTI", 0xEDA3 },
    { "LDD", 0xEDA8 }, { "CPD", 0xEDA9 }, { "IND", 0xEDAA }, { "OUTD", 0xEDAB },
    { "LDIR", 0xEDB0 }, { "CPIR", 0xEDB1 }, { "INIR", 0xEDB2 }, { "OTIR", 0xEDB3 },
    { "LDDR", 0xEDB8 }, { "CPDR", 0xEDB9 }, { "INDR", 0xEDBA }, { "OTDR", 0xEDBB },
    { NULL, 0 }
};

static const char *ALU[] = { "ADD", "ADC", "SUB", "SBC", "AND", "XOR", "OR", "CP" };
static const char *ROT[] = { "RLC", "RRC", "RL", "RR", "SLA", "SRA", "", "SRL" };

#define ANTAL(n) if (nop != (n)) fel("%s ska ha %d operander", mn, n)

static void instruktion(const char *mn, Op *op, int nop)
{
    Op *a = &op[0], *b = &op[1];

    for (const Enkel *e = ENKLA; e->namn; e++)
        if (strcmp(mn, e->namn) == 0) {
            ANTAL(0);
            if (e->kod > 0xFF) ut(e->kod >> 8);
            ut(e->kod & 0xFF);
            return;
        }

    for (int k = 0; k < 8; k++)
        if (strcmp(mn, ALU[k]) == 0) {
            /* 16 bitar: ADD HL,rr  ADD IX,rr  ADC HL,rr  SBC HL,rr */
            if (nop == 2 && a->typ == O_RP && a->reg == 2 && b->typ == O_RP
                && (k == 0 || k == 1 || k == 3)) {
                if (b->reg == 2 && b->pfx != a->pfx)
                    fel("%s %s,%s finns inte", mn, a->text, b->text);
                if (k == 0) {
                    if (a->pfx) ut(a->pfx);
                    ut(0x09 + 16 * b->reg);
                } else {
                    if (a->pfx) fel("%s %s finns inte", mn, a->text);
                    ut(0xED);
                    ut((k == 1 ? 0x4A : 0x42) + 16 * b->reg);
                }
                return;
            }
            /* 8 bitar: OP A,x eller OP x */
            Op *x = a;
            if (nop == 2) {
                if (a->typ != O_R || a->reg != 7)
                    fel("%s: första operanden ska vara A", mn);
                x = b;
            } else
                ANTAL(1);
            if (x->typ == O_R)
                rop(x, 0x80 + 8 * k + x->reg);
            else if (x->typ == O_TAL) {
                ut(0xC6 + 8 * k);
                ut8(x->v);
            } else
                fel("%s %s finns inte", mn, x->text);
            return;
        }

    if (strcmp(mn, "INC") == 0 || strcmp(mn, "DEC") == 0) {
        int dec = mn[0] == 'D';
        ANTAL(1);
        if (a->typ == O_R)
            rop(a, 0x04 + dec + 8 * a->reg);
        else if (a->typ == O_RP)
            rop(a, 0x03 + 8 * dec + 16 * a->reg);
        else
            fel("%s %s finns inte", mn, a->text);
        return;
    }

    if (strcmp(mn, "LD") == 0) {
        ANTAL(2);
        if (a->typ == O_R && b->typ == O_R) {
            if (a->reg == 6 && b->reg == 6)
                fel("LD %s,%s finns inte", a->text, b->text);
            const Op *x = a->reg == 6 ? a : b;
            if (x->pfx) ut(x->pfx);
            ut(0x40 + 8 * a->reg + b->reg);
            if (x->pfx) utd(x->v);
        } else if (a->typ == O_R && b->typ == O_TAL) {
            rop(a, 0x06 + 8 * a->reg);
            ut8(b->v);
        } else if (a->typ == O_R && a->reg == 7 && b->typ == O_BCI) {
            ut(0x0A);
        } else if (a->typ == O_R && a->reg == 7 && b->typ == O_DEI) {
            ut(0x1A);
        } else if (a->typ == O_R && a->reg == 7 && b->typ == O_MEM) {
            ut(0x3A); ut16(b->v);
        } else if (a->typ == O_R && a->reg == 7 && b->typ == O_I) {
            ut(0xED); ut(0x57);
        } else if (a->typ == O_R && a->reg == 7 && b->typ == O_RR) {
            ut(0xED); ut(0x5F);
        } else if (b->typ == O_R && b->reg == 7 && a->typ == O_BCI) {
            ut(0x02);
        } else if (b->typ == O_R && b->reg == 7 && a->typ == O_DEI) {
            ut(0x12);
        } else if (b->typ == O_R && b->reg == 7 && a->typ == O_MEM) {
            ut(0x32); ut16(a->v);
        } else if (b->typ == O_R && b->reg == 7 && a->typ == O_I) {
            ut(0xED); ut(0x47);
        } else if (b->typ == O_R && b->reg == 7 && a->typ == O_RR) {
            ut(0xED); ut(0x4F);
        } else if (a->typ == O_RP && b->typ == O_TAL) {
            rop(a, 0x01 + 16 * a->reg);
            ut16(b->v);
        } else if (a->typ == O_RP && b->typ == O_MEM) {
            if (a->reg == 2)
                rop(a, 0x2A);
            else {
                ut(0xED); ut(0x4B + 16 * a->reg);
            }
            ut16(b->v);
        } else if (a->typ == O_MEM && b->typ == O_RP) {
            if (b->reg == 2)
                rop(b, 0x22);
            else {
                ut(0xED); ut(0x43 + 16 * b->reg);
            }
            ut16(a->v);
        } else if (a->typ == O_RP && a->reg == 3 && b->typ == O_RP && b->reg == 2) {
            rop(b, 0xF9);
        } else
            fel("LD %s,%s finns inte", a->text, b->text);
        return;
    }

    if (strcmp(mn, "PUSH") == 0 || strcmp(mn, "POP") == 0) {
        int kod = mn[1] == 'U' ? 0xC5 : 0xC1;
        ANTAL(1);
        if (a->typ == O_RP && a->reg < 3)
            rop(a, kod + 16 * a->reg);
        else if (a->typ == O_AF)
            ut(kod + 0x30);
        else
            fel("%s %s finns inte", mn, a->text);
        return;
    }

    if (strcmp(mn, "EX") == 0) {
        ANTAL(2);
        if (a->typ == O_RP && a->reg == 1 && b->typ == O_RP && b->reg == 2 && !b->pfx)
            ut(0xEB);
        else if (a->typ == O_AF && b->typ == O_AFX)
            ut(0x08);
        else if (a->typ == O_SPI && b->typ == O_RP && b->reg == 2)
            rop(b, 0xE3);
        else
            fel("EX %s,%s finns inte", a->text, b->text);
        return;
    }

    /* Hoppen. Villkoren läses ur texten, så att C blir villkoret C och
     * inte registret, och så att symboler kan heta Z, P eller M. */
    if (strcmp(mn, "JP") == 0 || strcmp(mn, "CALL") == 0) {
        int jp = mn[0] == 'J';
        if (nop == 2) {
            int cc = villkor(a->text, 0);
            if (cc < 0) fel("okänt villkor: %s", a->text);
            if (b->typ != O_TAL) fel("%s %s,%s finns inte", mn, a->text, b->text);
            ut((jp ? 0xC2 : 0xC4) + 8 * cc);
            ut16(b->v);
            return;
        }
        ANTAL(1);
        if (jp && a->typ == O_R && a->reg == 6 && a->v == 0) {
            rop(&(Op){ O_RP, 2, a->pfx, 0, "" }, 0xE9);      /* JP (HL) */
            return;
        }
        if (a->typ != O_TAL) fel("%s %s finns inte", mn, a->text);
        ut(jp ? 0xC3 : 0xCD);
        ut16(a->v);
        return;
    }

    if (strcmp(mn, "JR") == 0 || strcmp(mn, "DJNZ") == 0) {
        if (nop == 2 && mn[0] == 'J') {
            int cc = villkor(a->text, 1);
            if (cc < 0) fel("JR har inte villkoret %s", a->text);
            if (b->typ != O_TAL) fel("JR %s,%s finns inte", a->text, b->text);
            ut(0x20 + 8 * cc);
            relativ(b->v);
            return;
        }
        ANTAL(1);
        if (a->typ != O_TAL) fel("%s %s finns inte", mn, a->text);
        ut(mn[0] == 'J' ? 0x18 : 0x10);
        relativ(a->v);
        return;
    }

    if (strcmp(mn, "RET") == 0) {
        if (nop == 0) { ut(0xC9); return; }
        ANTAL(1);
        int cc = villkor(a->text, 0);
        if (cc < 0) fel("okänt villkor: %s", a->text);
        ut(0xC0 + 8 * cc);
        return;
    }

    if (strcmp(mn, "RST") == 0) {
        ANTAL(1);
        if (a->typ != O_TAL || (a->v & ~0x38))
            fel("RST ska ha 0, 8, $10 ... $38");
        ut(0xC7 + a->v);
        return;
    }

    for (int k = 0; k < 8; k++)
        if (k != 6 && strcmp(mn, ROT[k]) == 0) {
            ANTAL(1);
            if (a->typ != O_R) fel("%s %s finns inte", mn, a->text);
            if (a->pfx) {
                ut(a->pfx); ut(0xCB); utd(a->v);
            } else
                ut(0xCB);
            ut(8 * k + a->reg);
            return;
        }

    if (strcmp(mn, "BIT") == 0 || strcmp(mn, "RES") == 0 || strcmp(mn, "SET") == 0) {
        int bas = mn[0] == 'B' ? 0x40 : mn[0] == 'R' ? 0x80 : 0xC0;
        ANTAL(2);
        if (a->typ != O_TAL || b->typ != O_R)
            fel("%s %s,%s finns inte", mn, a->text, b->text);
        if (sista && (a->v < 0 || a->v > 7))
            fel("bitnumret ska vara 0-7");
        if (b->pfx) {
            ut(b->pfx); ut(0xCB); utd(b->v);
        } else
            ut(0xCB);
        ut(bas + 8 * (a->v & 7) + b->reg);
        return;
    }

    if (strcmp(mn, "IN") == 0) {
        ANTAL(2);
        if (a->typ == O_R && a->reg == 7 && b->typ == O_MEM) {
            ut(0xDB); ut8(b->v);
        } else if (a->typ == O_R && a->reg != 6 && b->typ == O_CI) {
            ut(0xED); ut(0x40 + 8 * a->reg);
        } else
            fel("IN %s,%s finns inte", a->text, b->text);
        return;
    }

    if (strcmp(mn, "OUT") == 0) {
        ANTAL(2);
        if (a->typ == O_MEM && b->typ == O_R && b->reg == 7) {
            ut(0xD3); ut8(a->v);
        } else if (a->typ == O_CI && b->typ == O_R && b->reg != 6) {
            ut(0xED); ut(0x41 + 8 * b->reg);
        } else
            fel("OUT %s,%s finns inte", a->text, b->text);
        return;
    }

    if (strcmp(mn, "IM") == 0) {
        ANTAL(1);
        static const int IM[] = { 0x46, 0x56, 0x5E };
        if (a->typ != O_TAL || a->v < 0 || a->v > 2)
            fel("IM ska ha 0, 1 eller 2");
        ut(0xED); ut(IM[a->v]);
        return;
    }

    fel("okänd instruktion: %s", mn);
}

/* ---------------------------------------------------------------- */
/* Raderna                                                           */

/* Hoppa över en sträng "..." ("" inuti är ett citattecken) eller en
 * teckenkonstant 'x'; annars ett tecken. AF' är ingen konstant. */
static const char *forbi(const char *s)
{
    if (*s == '"') {
        for (s++; *s; s++)
            if (*s == '"') {
                if (s[1] != '"') return s + 1;
                s++;
            }
        return s;
    }
    if (*s == '\'' && s[1] && s[2] == '\'')
        return s + 3;
    return s + 1;
}

/* Dela operandtexten vid kommatecken utanför parenteser och strängar. */
static int dela(char *s, char **txt, int max)
{
    int n = 0, djup = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (!*s) return 0;
    char *b = s;
    for (;;) {
        if (*s == 0 || (*s == ',' && djup == 0)) {
            char *e = s;
            while (e > b && (e[-1] == ' ' || e[-1] == '\t')) e--;
            int slut = *s == 0;
            *e = 0;
            if (n == max) fel("för många operander");
            if (!*b) fel("tom operand");
            txt[n++] = b;
            if (slut) return n;
            for (s++; *s == ' ' || *s == '\t'; s++) ;
            b = s;
            continue;
        }
        if (*s == '(') djup++;
        if (*s == ')') djup--;
        s = (char *)forbi(s);
    }
}

static void bytes(char **txt, int n)
{
    for (int k = 0; k < n; k++) {
        const char *s = txt[k];
        if (s[0] == '"') {
            const char *e = forbi(s);
            if (e[-1] != '"' || e == s + 1 || *e)
                fel("felaktig sträng: %s", s);
            for (s++; s < e - 1; s++) {
                ut((unsigned char)*s);
                if (*s == '"') s++;
            }
        } else
            ut8(varde(s));
    }
}

static int slut;                     /* END har lästs */
static char *lstrad;                 /* källraden som den står, till listningen */

static void rad(char *s, FILE *lst)
{
    char etikett[256] = "", mn[16] = "";
    char *txt[256];
    Op op[2];
    int nop = 0;
    long eq = -1;                    /* EQU-värdet till listningen */

    radpc = pc;
    nradbytes = 0;

    if (setjmp(radslut))
        goto lista;

    /* kommentaren bort */
    for (char *p = s; *p; ) {
        if (*p == ';') { *p = 0; break; }
        p = (char *)forbi(p);
    }

    /* etiketten: först på raden, eller ett namn med kolon */
    char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    char *b = p;
    while (idtecken(*p)) p++;
    if (p > b && (*p == ':' || b == s)) {
        if (p - b >= (int)sizeof etikett) fel("för långt namn");
        if (isdigit((unsigned char)*b)) fel("ett namn får inte börja med en siffra");
        memcpy(etikett, b, p - b);
        etikett[p - b] = 0;
        if (*p == ':') p++;
    } else
        p = b;

    /* instruktionen */
    while (*p == ' ' || *p == '\t') p++;
    b = p;
    if (*p == '=') {
        strcpy(mn, "EQU");
        p++;
    } else {
        while (isalpha((unsigned char)*p)) p++;
        if (p - b >= (int)sizeof mn) fel("okänd instruktion: %.*s", (int)(p - b), b);
        stora(mn, b, (int)(p - b) + 1);
        if (*p && *p != ' ' && *p != '\t') fel("felaktig instruktion: %s", b);
    }
    nop = dela(p, txt, 256);

    if (strcmp(mn, "EQU") == 0) {
        if (!*etikett) fel("EQU saknar namn");
        if (nop != 1) fel("EQU ska ha ett värde");
        okand = 0;
        long v = varde(txt[0]);
        if (okand)
            olosta++;
        else {
            definiera(etikett, v);
            eq = v;
        }
        goto lista;
    }
    if (*etikett)
        definiera(etikett, pc);
    if (!*mn)
        goto lista;

    if (strcmp(mn, "ORG") == 0) {
        if (nop != 1) fel("ORG ska ha en adress");
        okand = 0;
        long v = varde(txt[0]);
        if (okand) olosta++;
        if (v < 0 || v > 0xFFFF) fel("ORG: adressen är utanför 0-$FFFF");
        pc = radpc = v;
    } else if (strcmp(mn, "DEFB") == 0 || strcmp(mn, "DB") == 0
               || strcmp(mn, "DEFM") == 0 || strcmp(mn, "DM") == 0) {
        if (nop == 0) fel("%s utan värden", mn);
        bytes(txt, nop);
    } else if (strcmp(mn, "DEFW") == 0 || strcmp(mn, "DW") == 0) {
        if (nop == 0) fel("%s utan värden", mn);
        for (int k = 0; k < nop; k++)
            ut16(varde(txt[k]));
    } else if (strcmp(mn, "DEFS") == 0 || strcmp(mn, "DS") == 0) {
        if (nop < 1 || nop > 2) fel("%s antal[,byte]", mn);
        okand = 0;
        long n = varde(txt[0]);
        if (okand) olosta++;
        if (n < 0 || pc + n > 0x10000) fel("%s: felaktigt antal %ld", mn, n);
        long fyll = nop == 2 ? varde(txt[1]) : 0;
        for (long k = 0; k < n; k++)
            ut8(fyll);
    } else if (strcmp(mn, "END") == 0) {
        slut = 1;
    } else {
        if (nop > 2) fel("%s ska ha högst två operander", mn);
        /* Villkoret i JP, JR, CALL och RET läses inte som ett uttryck. */
        int cc = nop == (strcmp(mn, "RET") == 0 ? 1 : 2)
                 && (strcmp(mn, "JP") == 0 || strcmp(mn, "JR") == 0
                     || strcmp(mn, "CALL") == 0 || strcmp(mn, "RET") == 0);
        for (int k = 0; k < nop; k++)
            if (k == 0 && cc)
                stora(op[0].text, txt[0], sizeof op[0].text);
            else
                operand(txt[k], &op[k]);
        instruktion(mn, op, nop);
    }

lista:
    if (!sista || !lst)
        return;
    int i = 0;
    do {
        if (eq >= 0)
            fprintf(lst, "     = %04lX     ", eq & 0xFFFF);
        else if (nradbytes || i == 0) {
            if (nradbytes || *etikett || *mn)
                fprintf(lst, "%04lX ", (radpc + i) & 0xFFFF);
            else
                fprintf(lst, "     ");
            for (int k = 0; k < 4; k++)
                if (i + k < nradbytes)
                    fprintf(lst, "%02X", radbytes[i + k]);
                else
                    fprintf(lst, "  ");
            fprintf(lst, "  ");
        }
        fprintf(lst, "%s\n", i == 0 ? lstrad : "");
        i += 4;
    } while (i < nradbytes);
}

/* ---------------------------------------------------------------- */

static char *last(const char *fn, long *n)
{
    FILE *f = fopen(fn, "rb");
    if (!f) {
        fprintf(stderr, "z80asm: kan inte öppna %s\n", fn);
        exit(1);
    }
    fseek(f, 0, SEEK_END);
    *n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *s = malloc(*n + 1);
    if (!s || fread(s, 1, *n, f) != (size_t)*n) {
        fprintf(stderr, "z80asm: kan inte läsa %s\n", fn);
        exit(1);
    }
    s[*n] = 0;
    fclose(f);
    return s;
}

static int jmfnamn(const void *a, const void *b)
{
    return strcmp((*(Sym *const *)a)->namn, (*(Sym *const *)b)->namn);
}

static void symboler(FILE *lst)
{
    Sym **v = malloc(sizeof *v * (antalsym + 1));
    int n = 0;
    for (int h = 0; h < HASH; h++)
        for (Sym *s = symtab[h]; s; s = s->nasta)
            v[n++] = s;
    qsort(v, n, sizeof *v, jmfnamn);
    fprintf(lst, "\nSymboler:\n");
    for (int k = 0; k < n; k++)
        fprintf(lst, "%-16s %04lX%s", v[k]->namn, v[k]->varde & 0xFFFF,
                k % 4 == 3 || k == n - 1 ? "\n" : "   ");
    free(v);
}

static void hexfil(FILE *f, long lag, long hog)
{
    for (long a = lag; a <= hog; ) {
        if (!satt[a]) { a++; continue; }
        int n = 0;
        while (n < 16 && a + n <= hog && satt[a + n]) n++;
        unsigned sum = n + (a >> 8) + (a & 0xFF);
        fprintf(f, ":%02X%04lX00", n, a);
        for (int k = 0; k < n; k++) {
            fprintf(f, "%02X", bild[a + k]);
            sum += bild[a + k];
        }
        fprintf(f, "%02X\n", (-sum) & 0xFF);
        a += n;
    }
    fprintf(f, ":00000001FF\n");
}

static void anvandning(void)
{
    fprintf(stderr, "användning: z80asm [-o fil.bin] [-x fil.hex] [-l fil.lst] källa.asm\n");
    exit(2);
}

int main(int argc, char **argv)
{
    const char *binfil = NULL, *hexnamn = NULL, *lstnamn = NULL, *kalla = NULL;
    for (int k = 1; k < argc; k++) {
        if (argv[k][0] == '-' && argv[k][1] && !argv[k][2] && k + 1 < argc) {
            switch (argv[k][1]) {
            case 'o': binfil = argv[++k]; continue;
            case 'x': hexnamn = argv[++k]; continue;
            case 'l': lstnamn = argv[++k]; continue;
            }
            anvandning();
        }
        if (kalla) anvandning();
        kalla = argv[k];
    }
    if (!kalla) anvandning();
    filnamn = kalla;

    char standard[1024];
    if (!binfil) {
        snprintf(standard, sizeof standard - 4, "%s", kalla);
        char *pkt = strrchr(standard, '.');
        if (pkt && !strchr(pkt, '/')) *pkt = 0;
        strcat(standard, ".bin");
        binfil = standard;
    }

    long n;
    char *text = last(kalla, &n);
    FILE *lst = NULL;
    if (lstnamn && !(lst = fopen(lstnamn, "w"))) {
        fprintf(stderr, "z80asm: kan inte skriva %s\n", lstnamn);
        return 1;
    }

    char *kopia = malloc(n + 1), *arbete = malloc(n + 1);
    /* Passen upprepas så länge något blir uträknat som inte var det
     * förut (EQU och DEFS med symboler som definieras längre ned). */
    int forra = -1;
    for (varv = 1; ; varv++) {
        sista = (olosta == 0 && varv > 1) || olosta == forra || varv == 20;
        forra = olosta;
        olosta = 0;
        memcpy(kopia, text, n + 1);
        pc = 0;
        slut = 0;
        radnr = 0;
        for (char *s = kopia; *s && !slut; ) {
            char *e = strchr(s, '\n');
            if (e) *e = 0;
            int len = (int)strlen(s);
            if (len && s[len - 1] == '\r') s[--len] = 0;
            radnr++;
            lstrad = s;
            strcpy(arbete, s);
            rad(arbete, lst);
            s = e ? e + 1 : s + len;
        }
        if (sista)
            break;
    }
    if (antalfel) {
        fprintf(stderr, "%s: %d fel\n", kalla, antalfel);
        if (lst) { fclose(lst); remove(lstnamn); }
        return 1;
    }

    long lag = 0, hog = -1;
    for (long a = 0; a < 65536; a++)
        if (satt[a]) { if (hog < 0) lag = a; hog = a; }
    FILE *f = fopen(binfil, "wb");
    if (!f) {
        fprintf(stderr, "z80asm: kan inte skriva %s\n", binfil);
        return 1;
    }
    for (long a = lag; a <= hog; a++)
        fputc(satt[a] ? bild[a] : 0xFF, f);
    fclose(f);
    if (hexnamn) {
        if (!(f = fopen(hexnamn, "w"))) {
            fprintf(stderr, "z80asm: kan inte skriva %s\n", hexnamn);
            return 1;
        }
        hexfil(f, lag, hog);
        fclose(f);
    }
    if (lst) {
        symboler(lst);
        fclose(lst);
    }
    return 0;
}
