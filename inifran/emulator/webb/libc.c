/*
 * libc.c -- det lilla C-bibliotek som emulatorn behöver i webbläsaren,
 * till boken ABC80 inifrån
 *
 * WebAssembly-versionen byggs utan C-bibliotek, så att den inte kräver
 * Emscripten eller WASI: bara clang och lld. Kärnan behöver några
 * minnesfunktioner och powf (ljud.c), WAV-läsningen exp och fabs, och
 * värden (webb.c) malloc och free till filerna som läses in.
 *
 * Minnet: blocken ligger efter varandra från __heap_base (där länkaren
 * slutar), vart och ett med sin storlek och om det är ledigt först.
 * malloc tar det första lediga block som räcker och delar det; free
 * slår ihop blocket med lediga grannar efter. Räcker inget block växer
 * det linjära minnet med memory.grow.
 *
 * exp och log räknas med serier efter att talet har delats upp i en
 * tvåpotens och en rest nära 0 (exp) eller 1 (log); det ger nästan full
 * dubbel precision.
 */

#include <stdint.h>

#include "libc/math.h"
#include "libc/stdlib.h"
#include "libc/string.h"

/* ---------------------------------------------------------------- */
/* Minnesfunktionerna                                                */

void *memset(void *mal, int varde, size_t n)
{
    uint8_t *p = mal;
    while (n--)
        *p++ = (uint8_t)varde;
    return mal;
}

void *memcpy(void *mal, const void *kalla, size_t n)
{
    uint8_t *p = mal;
    const uint8_t *q = kalla;
    while (n--)
        *p++ = *q++;
    return mal;
}

void *memmove(void *mal, const void *kalla, size_t n)
{
    uint8_t *p = mal;
    const uint8_t *q = kalla;
    if (p < q)
        while (n--)
            *p++ = *q++;
    else
        while (n--)
            p[n] = q[n];
    return mal;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *p = a, *q = b;
    for (; n--; p++, q++)
        if (*p != *q)
            return *p - *q;
    return 0;
}

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n])
        n++;
    return n;
}

char *strcpy(char *mal, const char *kalla)
{
    char *p = mal;
    while ((*p++ = *kalla++))
        ;
    return mal;
}

/* ---------------------------------------------------------------- */
/* malloc och free                                                   */

typedef struct Block {
    size_t storlek;                          /* hela blocket, med huvudet */
    size_t ledigt;
    size_t fyllnad[2];                       /* huvudet är 16 bytes */
} Block;

#define SIDA 65536

extern unsigned char __heap_base;
static Block *forsta, *slut;                 /* slut: efter det sista blocket */

static Block *nasta(Block *b)
{
    return (Block *)((uint8_t *)b + b->storlek);
}

void *malloc(size_t n)
{
    size_t behov = (n + sizeof(Block) + 15) & ~(size_t)15;
    if (!forsta) {
        forsta = slut = (Block *)(((uintptr_t)&__heap_base + 15) & ~(uintptr_t)15);
    }
    for (Block *b = forsta; b < slut; b = nasta(b)) {
        if (!b->ledigt)
            continue;
        while (nasta(b) < slut && nasta(b)->ledigt)
            b->storlek += nasta(b)->storlek;
        if (b->storlek < behov)
            continue;
        if (b->storlek - behov >= 2 * sizeof(Block)) {
            Block *rest = (Block *)((uint8_t *)b + behov);
            rest->storlek = b->storlek - behov;
            rest->ledigt = 1;
            b->storlek = behov;
        }
        b->ledigt = 0;
        return b + 1;
    }
    /* Inget ledigt block räckte: ett nytt i slutet, och mer minne. */
    uintptr_t ny_slut = (uintptr_t)slut + behov;
    uintptr_t har = (uintptr_t)__builtin_wasm_memory_size(0) * SIDA;
    if (ny_slut > har &&
        __builtin_wasm_memory_grow(0, (ny_slut - har + SIDA - 1) / SIDA) == (size_t)-1)
        return NULL;
    Block *b = slut;
    b->storlek = behov;
    b->ledigt = 0;
    slut = (Block *)ny_slut;
    return b + 1;
}

void free(void *p)
{
    if (!p)
        return;
    Block *b = (Block *)p - 1;
    b->ledigt = 1;
    while (nasta(b) < slut && nasta(b)->ledigt)
        b->storlek += nasta(b)->storlek;
    if (nasta(b) == slut)                    /* sist: minnet blir ledigt igen */
        slut = b;
}

/* ---------------------------------------------------------------- */
/* Matematiken                                                       */

static const double LN2 = 0.69314718055994530942;

/* x * 2^k, med k inom double:s exponenter. */
static double tvapotens(double x, int k)
{
    while (k > 1000) {
        x *= 0x1p1000;
        k -= 1000;
    }
    while (k < -1000) {
        x *= 0x1p-1000;
        k += 1000;
    }
    union { double d; uint64_t u; } t = { .u = (uint64_t)(1023 + k) << 52 };
    return x * t.d;
}

double exp(double x)
{
    if (x != x)
        return x;
    if (x > 709.8)
        return 1.0 / 0.0;
    if (x < -745.2)
        return 0.0;
    double avrundat = x / LN2;
    int k = (int)(avrundat < 0 ? avrundat - 0.5 : avrundat + 0.5);
    double r = x - k * LN2;                  /* |r| <= ln 2 / 2 */
    double summa = 1, term = 1;
    for (int n = 1; n < 18; n++) {
        term *= r / n;
        summa += term;
    }
    return tvapotens(summa, k);
}

double log(double x)
{
    if (x < 0 || x != x)
        return 0.0 / 0.0;
    if (x == 0)
        return -1.0 / 0.0;
    union { double d; uint64_t u; } t = { .d = x };
    int e = (int)(t.u >> 52 & 0x7FF);
    if (e == 0x7FF)
        return x;
    if (e == 0) {                            /* subnormalt */
        t.d = x * 0x1p54;
        e = (int)(t.u >> 52 & 0x7FF) - 54;
    }
    e -= 1023;
    t.u = (t.u & 0x000FFFFFFFFFFFFFull) | 0x3FF0000000000000ull;
    double m = t.d;                          /* 1 <= m < 2 */
    if (m > 1.41421356237309505) {
        m /= 2;
        e++;
    }
    /* log m = 2 atanh s, s = (m - 1) / (m + 1), |s| < 0,172 */
    double s = (m - 1) / (m + 1), s2 = s * s, term = s, summa = 0;
    for (int n = 1; n < 40; n += 2) {
        summa += term / n;
        term *= s2;
    }
    return 2 * summa + e * LN2;
}

double pow(double x, double y)
{
    if (y == 0)
        return 1;
    if (x == 0)
        return y > 0 ? 0 : 1.0 / 0.0;
    return exp(y * log(x));                  /* bara x > 0 behövs */
}

float powf(float x, float y)
{
    return (float)pow(x, y);
}

double fabs(double x)
{
    return __builtin_fabs(x);
}
