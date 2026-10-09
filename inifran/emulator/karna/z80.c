/*
 * z80.c -- Z80-processorn, till boken ABC80 inifrån
 *
 * Operationskoden delas upp i fälten
 *     x = bit 7-6, y = bit 5-3, z = bit 2-0, p = bit 5-4, q = bit 3
 * och nästan varje grupp av instruktioner är en rad i en switch på dem.
 * Registren numreras som i operationskoderna:
 *     r:   0 B  1 C  2 D  3 E  4 H  5 L  6 (HL)  7 A
 *     rp:  0 BC 1 DE 2 HL 3 SP     rp2: samma, men 3 AF
 *     cc:  0 NZ 1 Z  2 NC 3 C  4 PO 5 PE 6 P  7 M
 * Prefixen $DD och $FD gör HL till IX eller IY och (HL) till (IX+d) eller
 * (IY+d); H och L blir då IXH och IXL, utom i instruktioner som också
 * använder (IX+d). $CB är bitoperationerna, $ED resten.
 *
 * T-cyklerna är Zilogs. Varje instruktion räknas för sig: processorn
 * vet inte när minnet eller en port är långsam (ABC80 har inga
 * väntetillstånd).
 */

#include "z80.h"

/* Flaggorna S, Z, Y och X för ett värde, och med P (jämn paritet). */
static uint8_t sz53[256], sz53p[256];

static void tabeller(void)
{
    static int klara;
    if (klara)
        return;
    for (int v = 0; v < 256; v++) {
        uint8_t f = v & (Z80_S | Z80_Y | Z80_X);
        if (v == 0)
            f |= Z80_Z;
        int p = v ^ v >> 4;
        p ^= p >> 2;
        p ^= p >> 1;
        sz53[v] = f;
        sz53p[v] = f | (p & 1 ? 0 : Z80_P);
    }
    klara = 1;
}

/* ---------------------------------------------------------------- */
/* Minne, stack och register                                         */

static uint8_t las(Z80 *z, uint16_t a) { return z->las(z->dator, a); }
static void skriv(Z80 *z, uint16_t a, uint8_t v) { z->skriv(z->dator, a, v); }

static uint16_t las16(Z80 *z, uint16_t a)
{
    return las(z, a) | las(z, (uint16_t)(a + 1)) << 8;
}

static void skriv16(Z80 *z, uint16_t a, uint16_t v)
{
    skriv(z, a, v & 0xFF);
    skriv(z, (uint16_t)(a + 1), v >> 8);
}

/* En byte ur programmet; M1 är när en operationskod hämtas, då R ökar. */
static uint8_t hamta(Z80 *z) { return las(z, z->pc++); }

static uint8_t hamtaM1(Z80 *z)
{
    z->r = (z->r & 0x80) | ((z->r + 1) & 0x7F);
    return las(z, z->pc++);
}

static uint16_t hamta16(Z80 *z)
{
    uint16_t v = las16(z, z->pc);
    z->pc += 2;
    return v;
}

static void push(Z80 *z, uint16_t v)
{
    z->sp -= 2;
    skriv16(z, z->sp, v);
}

static uint16_t pop(Z80 *z)
{
    uint16_t v = las16(z, z->sp);
    z->sp += 2;
    return v;
}

static uint16_t BC(Z80 *z) { return z->b << 8 | z->c; }
static uint16_t DE(Z80 *z) { return z->d << 8 | z->e; }
static uint16_t HL(Z80 *z) { return z->h << 8 | z->l; }
static void setBC(Z80 *z, uint16_t v) { z->b = v >> 8; z->c = v & 0xFF; }
static void setDE(Z80 *z, uint16_t v) { z->d = v >> 8; z->e = v & 0xFF; }
static void setHL(Z80 *z, uint16_t v) { z->h = v >> 8; z->l = v & 0xFF; }

/* HL, eller IX eller IY efter ett prefix. */
static uint16_t hl(Z80 *z)
{
    return z->pfx == 0xDD ? z->ix : z->pfx == 0xFD ? z->iy : HL(z);
}

static void sethl(Z80 *z, uint16_t v)
{
    if (z->pfx == 0xDD) z->ix = v;
    else if (z->pfx == 0xFD) z->iy = v;
    else setHL(z, v);
}

static uint16_t rp(Z80 *z, int p)
{
    switch (p) {
    case 0: return BC(z);
    case 1: return DE(z);
    case 2: return hl(z);
    default: return z->sp;
    }
}

static void setrp(Z80 *z, int p, uint16_t v)
{
    switch (p) {
    case 0: setBC(z, v); break;
    case 1: setDE(z, v); break;
    case 2: sethl(z, v); break;
    default: z->sp = v; break;
    }
}

static uint16_t rp2(Z80 *z, int p)
{
    return p == 3 ? z->a << 8 | z->f : rp(z, p);
}

static void setrp2(Z80 *z, int p, uint16_t v)
{
    if (p == 3) { z->a = v >> 8; z->f = v & 0xFF; }
    else setrp(z, p, v);
}

/* 8-bitsregistret r (inte 6). Efter ett prefix är 4 och 5 IXH och IXL. */
static uint8_t reg(Z80 *z, int r)
{
    switch (r) {
    case 0: return z->b;
    case 1: return z->c;
    case 2: return z->d;
    case 3: return z->e;
    case 4: return hl(z) >> 8;
    case 5: return hl(z) & 0xFF;
    default: return z->a;
    }
}

static void setreg(Z80 *z, int r, uint8_t v)
{
    switch (r) {
    case 0: z->b = v; break;
    case 1: z->c = v; break;
    case 2: z->d = v; break;
    case 3: z->e = v; break;
    case 4: sethl(z, (uint16_t)(v << 8 | (hl(z) & 0xFF))); break;
    case 5: sethl(z, (uint16_t)((hl(z) & 0xFF00) | v)); break;
    default: z->a = v; break;
    }
}

/* Adressen för (HL), eller (IX+d) med förskjutningen ur programmet,
 * som kostar 8 T till. */
static uint16_t madr(Z80 *z)
{
    if (!z->pfx)
        return HL(z);
    int8_t d = (int8_t)hamta(z);
    z->extra += 8;
    return (uint16_t)(hl(z) + d);
}

/* Registret r i en instruktion som också har (IX+d): H och L är H och L. */
static uint8_t regu(Z80 *z, int r)
{
    uint8_t p = z->pfx, v;
    z->pfx = 0;
    v = reg(z, r);
    z->pfx = p;
    return v;
}

static void setregu(Z80 *z, int r, uint8_t v)
{
    uint8_t p = z->pfx;
    z->pfx = 0;
    setreg(z, r, v);
    z->pfx = p;
}

static int villkor(Z80 *z, int cc)
{
    static const uint8_t flagga[4] = { Z80_Z, Z80_C, Z80_P, Z80_S };
    int satt = (z->f & flagga[cc >> 1]) != 0;
    return cc & 1 ? satt : !satt;
}

/* ---------------------------------------------------------------- */
/* Aritmetiken                                                       */

/* ADD ADC SUB SBC AND XOR OR CP med A och v. */
static void alu(Z80 *z, int op, uint8_t v)
{
    unsigned a = z->a, c = z->f & Z80_C, res;
    switch (op) {
    case 0:                                  /* ADD */
        c = 0;
        /* fall igenom */
    case 1:                                  /* ADC */
        res = a + v + c;
        z->f = sz53[res & 0xFF] | ((a ^ v ^ res) & Z80_H)
             | (~(a ^ v) & (a ^ res) & 0x80 ? Z80_P : 0) | (res >> 8 & Z80_C);
        z->a = res & 0xFF;
        break;
    case 2:                                  /* SUB */
        c = 0;
        /* fall igenom */
    case 3:                                  /* SBC */
        res = a - v - c;
        z->f = sz53[res & 0xFF] | ((a ^ v ^ res) & Z80_H)
             | ((a ^ v) & (a ^ res) & 0x80 ? Z80_P : 0) | Z80_N | (res >> 8 & Z80_C);
        z->a = res & 0xFF;
        break;
    case 4:                                  /* AND */
        z->a &= v;
        z->f = sz53p[z->a] | Z80_H;
        break;
    case 5:                                  /* XOR */
        z->a ^= v;
        z->f = sz53p[z->a];
        break;
    case 6:                                  /* OR */
        z->a |= v;
        z->f = sz53p[z->a];
        break;
    default:                                 /* CP: som SUB, men A ändras inte, */
        res = a - v;                         /* och Y och X kommer från v */
        z->f = (sz53[res & 0xFF] & ~(Z80_Y | Z80_X)) | (v & (Z80_Y | Z80_X))
             | ((a ^ v ^ res) & Z80_H)
             | ((a ^ v) & (a ^ res) & 0x80 ? Z80_P : 0) | Z80_N | (res >> 8 & Z80_C);
        break;
    }
}

static uint8_t inc8(Z80 *z, uint8_t v)
{
    v++;
    z->f = (z->f & Z80_C) | sz53[v] | ((v & 0x0F) == 0 ? Z80_H : 0)
         | (v == 0x80 ? Z80_P : 0);
    return v;
}

static uint8_t dec8(Z80 *z, uint8_t v)
{
    uint8_t f = (z->f & Z80_C) | Z80_N | ((v & 0x0F) == 0 ? Z80_H : 0)
              | (v == 0x80 ? Z80_P : 0);
    v--;
    z->f = f | sz53[v];
    return v;
}

/* ADD HL,rr: S, Z och P ändras inte; H är minnessiffran från bit 11. */
static uint16_t add16(Z80 *z, uint16_t a, uint16_t b)
{
    unsigned res = a + b;
    z->f = (z->f & (Z80_S | Z80_Z | Z80_P)) | (res >> 8 & (Z80_Y | Z80_X))
         | ((a ^ b ^ res) >> 8 & Z80_H) | (res >> 16 & Z80_C);
    return res & 0xFFFF;
}

/* ADC HL,rr och SBC HL,rr ger alla flaggorna, för 16 bitar. */
static uint16_t adc16(Z80 *z, uint16_t a, uint16_t b, int sub)
{
    unsigned c = z->f & Z80_C;
    unsigned res = sub ? (unsigned)a - b - c : (unsigned)a + b + c;
    uint16_t r = res & 0xFFFF;
    uint8_t f = (r >> 8) & (Z80_S | Z80_Y | Z80_X);
    if (r == 0) f |= Z80_Z;
    if ((a ^ b ^ r) & 0x1000) f |= Z80_H;
    if (sub ? (a ^ b) & (a ^ r) & 0x8000 : ~(a ^ b) & (a ^ r) & 0x8000) f |= Z80_P;
    if (sub) f |= Z80_N;
    if (res & 0x10000) f |= Z80_C;
    z->f = f;
    return r;
}

/* RLC RRC RL RR SLA SRA SLL SRL. SLL (6) är odokumenterad: som SLA,
 * men bit 0 blir 1. */
static uint8_t rot(Z80 *z, int op, uint8_t v)
{
    unsigned c, u = v, gc = z->f & Z80_C;
    switch (op) {
    case 0: c = u >> 7; u = u << 1 | c; break;
    case 1: c = u & 1; u = u >> 1 | c << 7; break;
    case 2: c = u >> 7; u = u << 1 | gc; break;
    case 3: c = u & 1; u = u >> 1 | gc << 7; break;
    case 4: c = u >> 7; u <<= 1; break;
    case 5: c = u & 1; u = u >> 1 | (u & 0x80); break;
    case 6: c = u >> 7; u = u << 1 | 1; break;
    default: c = u & 1; u >>= 1; break;
    }
    v = u & 0xFF;
    z->f = sz53p[v] | c;
    return v;
}

static void bit(Z80 *z, int b, uint8_t v)
{
    uint8_t m = v & (1 << b);
    z->f = (z->f & Z80_C) | Z80_H | (v & (Z80_Y | Z80_X))
         | (m ? (m & Z80_S) : (Z80_Z | Z80_P));
}

static void daa(Z80 *z)
{
    uint8_t a = z->a, korr = 0, c = z->f & Z80_C, n = z->f & Z80_N, h;
    if ((z->f & Z80_H) || (a & 0x0F) > 9)
        korr |= 0x06;
    if (c || a > 0x99) {
        korr |= 0x60;
        c = Z80_C;
    }
    if (n)
        h = (z->f & Z80_H) && (a & 0x0F) < 6;
    else
        h = (a & 0x0F) > 9;
    z->a = n ? a - korr : a + korr;
    z->f = sz53p[z->a] | n | (h ? Z80_H : 0) | c;
}

/* ---------------------------------------------------------------- */
/* $CB: rotationerna och bitarna                                     */

static int cb(Z80 *z)
{
    uint16_t adr = 0;
    uint8_t op, v;
    if (z->pfx) {                            /* $DD $CB d op */
        adr = (uint16_t)(hl(z) + (int8_t)hamta(z));
        op = hamta(z);
    } else
        op = hamtaM1(z);
    int x = op >> 6, y = op >> 3 & 7, r = op & 7;
    int minne = z->pfx || r == 6;
    if (minne) {
        if (!z->pfx) adr = HL(z);
        v = las(z, adr);
    } else
        v = regu(z, r);
    switch (x) {
    case 0: v = rot(z, y, v); break;
    case 1:
        bit(z, y, v);
        if (z->pfx)                          /* Y och X ur adressens höga byte */
            z->f = (z->f & ~(Z80_Y | Z80_X)) | (adr >> 8 & (Z80_Y | Z80_X));
        return z->pfx ? 16 : minne ? 12 : 8;
    case 2: v &= ~(1 << y); break;
    default: v |= 1 << y; break;
    }
    if (minne)
        skriv(z, adr, v);
    if (r != 6)                              /* med (IX+d) även till registret */
        setregu(z, r, v);                    /* (odokumenterat) */
    return z->pfx ? 19 : minne ? 15 : 8;
}

/* ---------------------------------------------------------------- */
/* $ED                                                               */

static int ed(Z80 *z)
{
    uint8_t op = hamtaM1(z), v;
    int x = op >> 6, y = op >> 3 & 7, r = op & 7, p = y >> 1, q = y & 1;
    z->pfx = 0;                              /* $DD $ED: prefixet gäller inte */

    if (x == 1) switch (r) {
    case 0:                                  /* IN r,(C) */
        v = z->in(z->dator, BC(z));
        if (y != 6) setreg(z, y, v);
        z->f = (z->f & Z80_C) | sz53p[v];
        return 12;
    case 1:                                  /* OUT (C),r */
        z->ut(z->dator, BC(z), y == 6 ? 0 : reg(z, y));
        return 12;
    case 2:                                  /* SBC HL,rr / ADC HL,rr */
        setHL(z, adc16(z, HL(z), rp(z, p), !q));
        return 15;
    case 3:                                  /* LD (nn),rr / LD rr,(nn) */
        {
            uint16_t nn = hamta16(z);
            if (q) setrp(z, p, las16(z, nn));
            else skriv16(z, nn, rp(z, p));
        }
        return 20;
    case 4:                                  /* NEG */
        v = z->a;
        z->a = 0;
        alu(z, 2, v);
        return 8;
    case 5:                                  /* RETN, RETI */
        z->pc = pop(z);
        z->iff1 = z->iff2;
        return 14;
    case 6:                                  /* IM 0, 1, 2 */
        z->im = (y & 3) < 2 ? 0 : (y & 3) - 1;
        return 8;
    default:
        switch (y) {
        case 0: z->i = z->a; return 9;       /* LD I,A */
        case 1: z->r = z->a; return 9;       /* LD R,A */
        case 2: case 3:                      /* LD A,I / LD A,R */
            z->a = y == 2 ? z->i : z->r;
            z->f = (z->f & Z80_C) | sz53[z->a] | (z->iff2 ? Z80_P : 0);
            return 9;
        case 4: case 5:                      /* RRD / RLD */
            v = las(z, HL(z));
            if (y == 4) {
                skriv(z, HL(z), (uint8_t)(z->a << 4 | v >> 4));
                z->a = (z->a & 0xF0) | (v & 0x0F);
            } else {
                skriv(z, HL(z), (uint8_t)(v << 4 | (z->a & 0x0F)));
                z->a = (z->a & 0xF0) | v >> 4;
            }
            z->f = (z->f & Z80_C) | sz53p[z->a];
            return 18;
        }
        return 8;
    }

    if (x == 2 && y >= 4 && r <= 3) {        /* blockinstruktionerna */
        int steg = y & 1 ? -1 : 1, upprepa = y >= 6, igen;
        uint16_t bc;
        switch (r) {
        case 0:                              /* LDI LDD LDIR LDDR */
            v = las(z, HL(z));
            skriv(z, DE(z), v);
            setHL(z, HL(z) + steg);
            setDE(z, DE(z) + steg);
            setBC(z, bc = BC(z) - 1);
            v += z->a;
            z->f = (z->f & (Z80_S | Z80_Z | Z80_C)) | (bc ? Z80_P : 0)
                 | (v & Z80_X) | (v << 4 & Z80_Y);
            igen = bc != 0;
            break;
        case 1:                              /* CPI CPD CPIR CPDR */
            {
                v = las(z, HL(z));
                uint8_t res = z->a - v;
                setHL(z, HL(z) + steg);
                setBC(z, bc = BC(z) - 1);
                z->f = (z->f & Z80_C) | Z80_N | (sz53[res] & (Z80_S | Z80_Z))
                     | ((z->a ^ v ^ res) & Z80_H) | (bc ? Z80_P : 0);
                uint8_t n = res - ((z->f & Z80_H) != 0);   /* Y och X: */
                z->f |= (n & Z80_X) | (n << 4 & Z80_Y);    /* ur A - v - H */
                igen = bc != 0 && res != 0;
            }
            break;
        case 2:                              /* INI IND INIR INDR */
            v = z->in(z->dator, BC(z));
            skriv(z, HL(z), v);
            setHL(z, HL(z) + steg);
            z->b--;
            z->f = (z->f & Z80_C) | sz53[z->b] | Z80_N;
            igen = z->b != 0;
            break;
        default:                             /* OUTI OUTD OTIR OTDR */
            v = las(z, HL(z));
            z->b--;                          /* B räknas ned före OUT */
            z->ut(z->dator, BC(z), v);
            setHL(z, HL(z) + steg);
            z->f = (z->f & Z80_C) | sz53[z->b] | Z80_N;
            igen = z->b != 0;
            break;
        }
        if (upprepa && igen) {
            z->pc -= 2;                      /* samma instruktion en gång till */
            return 21;
        }
        return 16;
    }
    return 8;                                /* övriga: som två NOP */
}

/* ---------------------------------------------------------------- */
/* Instruktionerna utan $CB och $ED                                  */

static int huvud(Z80 *z, uint8_t op)
{
    int x = op >> 6, y = op >> 3 & 7, r = op & 7, p = y >> 1, q = y & 1;
    uint16_t adr, nn;
    uint8_t v;

    switch (x) {
    case 0:
        switch (r) {
        case 0:
            switch (y) {
            case 0: return 4;                                    /* NOP */
            case 1:                                              /* EX AF,AF' */
                v = z->a; z->a = z->a2; z->a2 = v;
                v = z->f; z->f = z->f2; z->f2 = v;
                return 4;
            case 2:                                              /* DJNZ e */
                {
                    int8_t e = (int8_t)hamta(z);
                    if (--z->b) { z->pc += e; return 13; }
                }
                return 8;
            case 3:                                              /* JR e */
                {
                    int8_t e = (int8_t)hamta(z);
                    z->pc += e;
                }
                return 12;
            default:                                             /* JR cc,e */
                {
                    int8_t e = (int8_t)hamta(z);
                    if (villkor(z, y - 4)) { z->pc += e; return 12; }
                }
                return 7;
            }
        case 1:
            if (!q) {                                            /* LD rr,nn */
                setrp(z, p, hamta16(z));
                return 10;
            }
            sethl(z, add16(z, hl(z), rp(z, p)));                 /* ADD HL,rr */
            return 11;
        case 2:
            switch (p) {
            case 0:                                              /* (BC) */
                if (q) z->a = las(z, BC(z)); else skriv(z, BC(z), z->a);
                return 7;
            case 1:                                              /* (DE) */
                if (q) z->a = las(z, DE(z)); else skriv(z, DE(z), z->a);
                return 7;
            case 2:                                              /* LD HL,(nn) */
                nn = hamta16(z);
                if (q) sethl(z, las16(z, nn)); else skriv16(z, nn, hl(z));
                return 16;
            default:                                             /* LD A,(nn) */
                nn = hamta16(z);
                if (q) z->a = las(z, nn); else skriv(z, nn, z->a);
                return 13;
            }
        case 3:                                                  /* INC/DEC rr */
            setrp(z, p, rp(z, p) + (q ? -1 : 1));
            return 6;
        case 4: case 5:                                          /* INC/DEC r */
            if (y == 6) {
                adr = madr(z);
                v = las(z, adr);
                skriv(z, adr, r == 4 ? inc8(z, v) : dec8(z, v));
                return 11;
            }
            v = reg(z, y);
            setreg(z, y, r == 4 ? inc8(z, v) : dec8(z, v));
            return 4;
        case 6:                                                  /* LD r,n */
            if (y == 6) {
                adr = madr(z);
                if (z->pfx) z->extra -= 3;   /* d och n hämtas i samma tid */
                skriv(z, adr, hamta(z));
                return 10;
            }
            setreg(z, y, hamta(z));
            return 7;
        default:
            switch (y) {
            case 0:                                              /* RLCA */
                v = z->a >> 7;
                z->a = (uint8_t)(z->a << 1 | v);
                break;
            case 1:                                              /* RRCA */
                v = z->a & 1;
                z->a = (uint8_t)(z->a >> 1 | v << 7);
                break;
            case 2:                                              /* RLA */
                v = z->a >> 7;
                z->a = (uint8_t)(z->a << 1 | (z->f & Z80_C));
                break;
            case 3:                                              /* RRA */
                v = z->a & 1;
                z->a = (uint8_t)(z->a >> 1 | (z->f & Z80_C) << 7);
                break;
            case 4: daa(z); return 4;
            case 5:                                              /* CPL */
                z->a = ~z->a;
                z->f = (z->f & (Z80_S | Z80_Z | Z80_P | Z80_C)) | Z80_H | Z80_N
                     | (z->a & (Z80_Y | Z80_X));
                return 4;
            case 6:                                              /* SCF */
                z->f = (z->f & (Z80_S | Z80_Z | Z80_P)) | Z80_C
                     | (z->a & (Z80_Y | Z80_X));
                return 4;
            default:                                             /* CCF */
                z->f = ((z->f & (Z80_S | Z80_Z | Z80_P | Z80_C)) | (z->f & Z80_C ? Z80_H : 0)
                     | (z->a & (Z80_Y | Z80_X))) ^ Z80_C;
                return 4;
            }
            z->f = (z->f & (Z80_S | Z80_Z | Z80_P)) | (z->a & (Z80_Y | Z80_X)) | v;
            return 4;
        }

    case 1:
        if (y == 6 && r == 6) {                                  /* HALT */
            z->halt = 1;
            z->pc--;
            return 4;
        }
        if (r == 6) {                                            /* LD r,(HL) */
            setregu(z, y, las(z, madr(z)));
            return 7;
        }
        if (y == 6) {                                            /* LD (HL),r */
            adr = madr(z);
            skriv(z, adr, regu(z, r));
            return 7;
        }
        setreg(z, y, reg(z, r));                                 /* LD r,r */
        return 4;

    case 2:                                                      /* ALU A,r */
        if (r == 6) {
            alu(z, y, las(z, madr(z)));
            return 7;
        }
        alu(z, y, reg(z, r));
        return 4;

    default:
        switch (r) {
        case 0:                                                  /* RET cc */
            if (villkor(z, y)) { z->pc = pop(z); return 11; }
            return 5;
        case 1:
            if (!q) {                                            /* POP rr */
                setrp2(z, p, pop(z));
                return 10;
            }
            switch (p) {
            case 0: z->pc = pop(z); return 10;                   /* RET */
            case 1:                                              /* EXX */
                v = z->b; z->b = z->b2; z->b2 = v;
                v = z->c; z->c = z->c2; z->c2 = v;
                v = z->d; z->d = z->d2; z->d2 = v;
                v = z->e; z->e = z->e2; z->e2 = v;
                v = z->h; z->h = z->h2; z->h2 = v;
                v = z->l; z->l = z->l2; z->l2 = v;
                return 4;
            case 2: z->pc = hl(z); return 4;                     /* JP (HL) */
            default: z->sp = hl(z); return 6;                    /* LD SP,HL */
            }
        case 2:                                                  /* JP cc,nn */
            nn = hamta16(z);
            if (villkor(z, y)) z->pc = nn;
            return 10;
        case 3:
            switch (y) {
            case 0: z->pc = hamta16(z); return 10;               /* JP nn */
            case 2:                                              /* OUT (n),A */
                v = hamta(z);
                z->ut(z->dator, (uint16_t)(z->a << 8 | v), z->a);
                return 11;
            case 3:                                              /* IN A,(n) */
                v = hamta(z);
                z->a = z->in(z->dator, (uint16_t)(z->a << 8 | v));
                return 11;
            case 4:                                              /* EX (SP),HL */
                nn = las16(z, z->sp);
                skriv16(z, z->sp, hl(z));
                sethl(z, nn);
                return 19;
            case 5:                                              /* EX DE,HL */
                nn = DE(z);                  /* (gäller alltid HL) */
                setDE(z, HL(z));
                setHL(z, nn);
                return 4;
            case 6: z->iff1 = z->iff2 = 0; return 4;             /* DI */
            default:                                             /* EI */
                z->iff1 = z->iff2 = 1;
                z->eivant = 1;
                return 4;
            }
        case 4:                                                  /* CALL cc,nn */
            nn = hamta16(z);
            if (villkor(z, y)) {
                push(z, z->pc);
                z->pc = nn;
                return 17;
            }
            return 10;
        case 5:
            if (!q) {                                            /* PUSH rr */
                push(z, rp2(z, p));
                return 11;
            }
            nn = hamta16(z);                                     /* CALL nn */
            push(z, z->pc);
            z->pc = nn;
            return 17;
        case 6:                                                  /* ALU A,n */
            alu(z, y, hamta(z));
            return 7;
        default:                                                 /* RST */
            push(z, z->pc);
            z->pc = y * 8;
            return 11;
        }
    }
}

/* ---------------------------------------------------------------- */

void z80_reset(Z80 *z)
{
    tabeller();
    z->pc = 0;
    z->i = z->r = 0;
    z->iff1 = z->iff2 = 0;
    z->im = 0;
    z->halt = z->eivant = 0;
    z->a = z->f = 0xFF;
    z->sp = 0xFFFF;
}

int z80_steg(Z80 *z)
{
    int t = 0;
    z->eivant = 0;
    z->pfx = 0;
    z->extra = 0;
    uint8_t op = hamtaM1(z);
    while (op == 0xDD || op == 0xFD) {       /* det sista prefixet gäller */
        z->pfx = op;
        t += 4;
        op = hamtaM1(z);
    }
    if (op == 0xCB)
        t += cb(z);
    else if (op == 0xED)
        t += ed(z);
    else
        t += huvud(z, op);
    t += z->extra;
    z->pfx = 0;
    return t;
}

/* Ett avbrott på en HALT fortsätter efter den. */
static void avbrott(Z80 *z, uint16_t till)
{
    if (z->halt) {
        z->halt = 0;
        z->pc++;
    }
    z->r = (z->r & 0x80) | ((z->r + 1) & 0x7F);
    push(z, z->pc);
    z->pc = till;
}

int z80_nmi(Z80 *z)
{
    z->iff1 = 0;                             /* iff2 minns om avbrott var tillåtna */
    avbrott(z, 0x0066);
    return 11;
}

/* INT med vektorn som enheten lägger på databussen. I läge 2 är adressen
 * till rutinen ordet på I * 256 + vektor; i läge 0 tolkas vektorn som
 * en RST (det enda ABC80:s enheter skulle kunna ge). */
int z80_int(Z80 *z, uint8_t vektor)
{
    if (!z->iff1 || z->eivant)
        return 0;
    z->iff1 = z->iff2 = 0;
    switch (z->im) {
    case 2:
        avbrott(z, las16(z, (uint16_t)(z->i << 8 | vektor)));
        return 19;
    case 1:
        avbrott(z, 0x0038);
        return 13;
    default:
        avbrott(z, vektor & 0x38);
        return 13;
    }
}
