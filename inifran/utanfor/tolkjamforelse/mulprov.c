/*
 * mulprov.c -- utanför boken: prövar b_mul (MULSNB på $0450) mot BASIC
 * II:s MUL16 ($4B19) i den olappade ROM:en: HL = HL * DE, CY vid spill.
 * Alla HL för steg 1-16, 32, 64 och 256, och slumpade par. Utan spill
 * jämförs HL och CY; med spill bara CY (ARRELM tittar inte på CY, och
 * spill når den inte: indexet är redan prövat mot fältets storlek).
 *
 *   ./mulprov ut/b_mul.rom [antal [frö]]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "maskin.h"

#define VAKT   0xC700
#define MUL16  0x4B19
#define MULSNB 0x0450

static int fore(void *d, Maskin *m) { (void)d; return m->z80.pc == VAKT; }

static void starta(Maskin *m, const char *rom)
{
    static uint8_t r[32768];
    FILE *f = fopen(rom, "rb");
    size_t n = f ? fread(r, 1, sizeof r, f) : 0;
    if (!n) { fprintf(stderr, "mulprov: kan inte läsa %s\n", rom); exit(1); }
    fclose(f);
    maskin_starta(m, 32);
    maskin_rom(m, r, n);
    maskin_aterstall(m);
    maskin_kor(m, MASKIN_MS(1000));
    m->krokar.fore_steg = fore;
    m->utan_nmi = 1;
}

static int anropa(Maskin *m, uint16_t rutin, uint16_t hl, uint16_t de, uint16_t *ut)
{
    Z80 *z = &m->z80;
    z->iff1 = z->iff2 = 0;
    z->halt = 0;
    z->sp = 0xD000;
    m->minne[--z->sp] = VAKT >> 8;
    m->minne[--z->sp] = VAKT & 0xFF;
    z->pc = rutin;
    z->h = hl >> 8; z->l = hl & 0xFF;
    z->d = de >> 8; z->e = de & 0xFF;
    m->stoppad = 0;
    maskin_kor(m, MASKIN_MS(10));
    if (z->pc != VAKT) { fprintf(stderr, "mulprov: kom inte tillbaka\n"); exit(1); }
    *ut = (uint16_t)(z->h << 8 | z->l);
    return z->f & 1;
}

static unsigned long long frö = 88172645463325252ULL;
static unsigned slump(void) { frö ^= frö << 13; frö ^= frö >> 7; frö ^= frö << 17; return (unsigned)frö; }

static long fall, olika, spill, spillhl;
static Maskin gammal, ny;

static void prova(uint16_t hl, uint16_t de)
{
    uint16_t g, n;
    int cg = anropa(&gammal, MUL16, hl, de, &g);
    int cn = anropa(&ny, MULSNB, hl, de, &n);
    fall++;
    if (cg) { spill++; spillhl += g != n; }
    if (cg != cn || (!cg && g != n)) {
        if (olika++ < 10)
            printf("olika: %04X * %04X: MUL16 %04X CY %d, MULSNB %04X CY %d\n", hl, de, g, cg, n, cn);
    }
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "mulprov lappad.rom [antal [frö]]\n"); return 1; }
    long antal = argc > 2 ? atol(argv[2]) : 200000;
    if (argc > 3) frö += strtoull(argv[3], NULL, 10);
    starta(&gammal, "../../emulator/roms/basicii80.rom");
    starta(&ny, argv[1]);
    static const uint16_t steg[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 32, 64, 256 };
    for (unsigned s = 0; s < sizeof steg / sizeof *steg; s++)
        for (unsigned h = 0; h < 0x10000; h++) {
            prova((uint16_t)h, steg[s]);
            prova(steg[s], (uint16_t)h);
        }
    for (long i = 0; i < antal; i++) {
        uint16_t a = (uint16_t)slump(), b = (uint16_t)slump();
        if (i % 3 == 0) b >>= slump() % 16;
        prova(a, b);
    }
    printf("%ld fall, %ld olika (%ld med spill, där HL skiljer i %ld)\n", fall, olika, spill, spillhl);
    return olika != 0;
}
