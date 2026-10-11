/*
 * fprov.c -- utanför boken: prövar ABC80:s FMUL och FDIV i en lappad
 * ROM mot den olappade, med slumpade tal och kanttal.
 *
 * Två maskiner startas, den ena med ROM:en ur emulator/roms och den
 * andra med en lappad ROM och dess krets på $5000. Varje fall anropar
 * rutinen direkt i båda: a på $C600, b på $C608, resultatet på $C610
 * (DE, HL och BC pekar på exponenterna). Rutinen är klar när PC når
 * vaktadressen; når den $0701 (fel 4) räknas det som ett utfall. Utfall,
 * resultatets 5 byte och A, F, BC, DE, HL efteråt jämförs.
 *
 * Undantaget: blir produkten 0 genom underspill när den normaliseras
 * (FMRND, exponenten 1 till 0) går FMUL tillbaka till FMULE med DE och HL
 * någon annanstans, och teckenbyten blir XOR av två byte som råkar ligga
 * där. Den olappade ROM:en får olika teckenbyte mot sig själv (beror på
 * vad som ligger i minnet). FCMP ser bara på exponenten när båda är 0,
 * så en sådan nolla räknas för sig och inte som olika.
 *
 *   ./fprov lappad.rom lappad.krets [antal [frö]]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "maskin.h"

#define VAKT   0xC700
#define FEL4   0x0701
#define FMUL   0x34DF
#define FDIV   0x35EE
#define FLAGGA 0xFE19          /* bit 7: avrundningen vid exakt 5 */

static int fore(void *d, Maskin *m)
{
    (void)d;
    return m->z80.pc == VAKT || m->z80.pc == FEL4;
}

static void las(const char *namn, uint8_t *buf, size_t max, size_t *n)
{
    FILE *f = fopen(namn, "rb");
    *n = f ? fread(buf, 1, max, f) : 0;
    if (!*n) {
        fprintf(stderr, "fprov: kan inte läsa %s\n", namn);
        exit(1);
    }
    fclose(f);
}

static void starta(Maskin *m, const char *rom, const char *krets)
{
    static uint8_t r[16384], k[4096];
    size_t n;
    las(rom, r, sizeof r, &n);
    maskin_starta(m, 16);
    maskin_rom(m, r, n);
    if (krets) {
        las(krets, k, sizeof k, &n);
        maskin_krets(m, 0x5000, k, n);
    }
    maskin_aterstall(m);
    maskin_kor(m, MASKIN_MS(1000));
    m->krokar.fore_steg = fore;
    m->utan_nmi = 1;
}

typedef struct {
    int fel;
    uint8_t res[5], a, f, b, c, d, e, h, l;
} Utfall;

static Utfall anropa(Maskin *m, uint16_t rutin, const uint8_t *a,
                     const uint8_t *b, uint8_t flagga)
{
    Utfall u;
    memcpy(m->minne + 0xC600, a, 5);
    memcpy(m->minne + 0xC608, b, 5);
    memset(m->minne + 0xC610, 0xAA, 5);
    m->minne[FLAGGA] = flagga;
    Z80 *z = &m->z80;
    z->iff1 = z->iff2 = 0;
    z->halt = 0;
    z->sp = 0xD000;
    m->minne[--z->sp] = VAKT >> 8;
    m->minne[--z->sp] = VAKT & 0xFF;
    z->pc = rutin;
    z->d = 0xC6; z->e = 0x04;
    z->h = 0xC6; z->l = 0x0C;
    z->b = 0xC6; z->c = 0x14;
    m->stoppad = 0;
    maskin_kor(m, MASKIN_MS(100));
    memset(&u, 0, sizeof u);
    u.fel = z->pc == FEL4 ? 1 : z->pc == VAKT ? 0 : 2;
    if (!u.fel) {
        memcpy(u.res, m->minne + 0xC610, 5);
        u.a = z->a; u.f = z->f;
        u.b = z->b; u.c = z->c; u.d = z->d; u.e = z->e; u.h = z->h; u.l = z->l;
    }
    return u;
}

static unsigned long long frö = 88172645463325252ULL;

static unsigned slump(unsigned n)
{
    frö ^= frö << 13; frö ^= frö >> 7; frö ^= frö << 17;
    return (unsigned)(frö % n);
}

static uint8_t bcd(int x) { return (uint8_t)((x / 10) << 4 | x % 10); }

/* ett tal: m1 m2 m3 tecken exponent */
static void tal(uint8_t *t)
{
    static const int kant[] = { 0x01, 0x02, 0x40, 0x7F, 0x80, 0x81, 0x82,
                                0xBF, 0xC0, 0xFE, 0xFF };
    static const long mant[] = { 100000, 999999, 500000, 999995, 100001,
                                 123456, 200000, 250000, 999990, 500005,
                                 333333, 666667, 100005, 199999 };
    long m;
    int e;
    switch (slump(8)) {
    case 0:  m = mant[slump(sizeof mant / sizeof *mant)]; break;
    case 1:  m = (1 + slump(9)) * 100000L; break;              /* en siffra */
    case 2:  m = (10 + slump(90)) * 10000L; break;             /* två siffror */
    case 3:  m = (100 + slump(900)) * 1000L; break;            /* tre */
    default: m = 100000 + slump(900000); break;
    }
    switch (slump(6)) {
    case 0:  e = kant[slump(sizeof kant / sizeof *kant)]; break;
    case 1:  e = 0x78 + slump(17); break;
    default: e = 1 + slump(255); break;
    }
    if (slump(40) == 0) {                                      /* noll */
        e = 0;
        m = slump(2) ? 0 : m;
    }
    t[0] = bcd((int)(m / 10000));
    t[1] = bcd((int)(m / 100 % 100));
    t[2] = bcd((int)(m % 100));
    t[3] = (uint8_t)slump(2);
    t[4] = (uint8_t)e;
}

static void visa(const char *s, const uint8_t *t)
{
    printf("%s %02X%02X%02X %02X %02X", s, t[0], t[1], t[2], t[3], t[4]);
}

int main(int argc, char **argv)
{
    static Maskin gammal, ny;
    if (argc < 3) {
        fprintf(stderr, "fprov lappad.rom lappad.krets [antal [frö]]\n");
        return 1;
    }
    long antal = argc > 3 ? atol(argv[3]) : 100000;
    if (argc > 4)
        frö += strtoull(argv[4], NULL, 10);
    starta(&gammal, "../../emulator/roms/abc80old.rom", NULL);
    starta(&ny, argv[1], argv[2]);
    static const struct { const char *namn; uint16_t adr; } rutin[] = {
        { "FMUL", FMUL }, { "FDIV", FDIV } };
    int fel = 0;
    for (int r = 0; r < 2; r++) {
        long olika = 0, fel4 = 0, noll = 0, nolltecken = 0;
        for (long i = 0; i < antal; i++) {
            uint8_t a[5], b[5];
            tal(a);
            tal(b);
            if (i % 7 == 0)
                memcpy(b, a, 5);                               /* a*a, a/a */
            uint8_t flagga = slump(2) ? 0x80 : 0x00;
            Utfall g = anropa(&gammal, rutin[r].adr, a, b, flagga);
            Utfall n = anropa(&ny, rutin[r].adr, a, b, flagga);
            fel4 += g.fel == 1;
            noll += !g.fel && g.res[4] == 0;
            if (!g.fel && !n.fel && !g.res[4] && !n.res[4] &&
                !memcmp(g.res, n.res, 3) && g.res[3] != n.res[3]) {
                nolltecken++;
                n.res[3] = g.res[3];
            }
            if (memcmp(&g, &n, sizeof g)) {
                if (olika++ < 10) {
                    printf("%s olika:", rutin[r].namn);
                    visa(" a", a);
                    visa(" b", b);
                    printf(" flagga %02X\n", flagga);
                    visa("  gammal", g.res);
                    printf(" fel %d A %02X F %02X\n", g.fel, g.a, g.f);
                    visa("  ny    ", n.res);
                    printf(" fel %d A %02X F %02X\n", n.fel, n.a, n.f);
                }
            }
        }
        printf("%s: %ld fall, %ld olika (%ld fel 4, %ld blev 0, "
               "%ld nollor med annat teckenbyte)\n",
               rutin[r].namn, antal, olika, fel4, noll, nolltecken);
        fel |= olika != 0;
    }
    return fel;
}
