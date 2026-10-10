/*
 * png.c -- en bild till en PNG-fil, till boken ABC80 inifrån
 *
 * Svartvitt, 8 bitar grått per punkt, utan komprimering: zlib-strömmen
 * har bara okomprimerade block (högst 65 535 bytes vart), så att inget
 * bibliotek behövs. Filerna blir större än nödvändigt men går att öppna
 * överallt. Används för P40:s papper (-CP).
 */

#include <stdio.h>
#include <stdlib.h>

#include "png.h"

static unsigned long crc_tabell[256];

static void crc_starta(void)
{
    for (unsigned long n = 0; n < 256; n++) {
        unsigned long c = n;
        for (int k = 0; k < 8; k++)
            c = c & 1 ? 0xEDB88320UL ^ (c >> 1) : c >> 1;
        crc_tabell[n] = c;
    }
}

static unsigned long crc(unsigned long c, const uint8_t *data, size_t n)
{
    c ^= 0xFFFFFFFFUL;
    while (n--)
        c = crc_tabell[(c ^ *data++) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFUL;
}

static void skriv32(uint8_t *p, unsigned long v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

/* Ett stycke: längden, typen, datat och CRC över typ och data. */
static void stycke(FILE *f, const char *typ, const uint8_t *data, size_t n)
{
    uint8_t huvud[8], svans[4];
    skriv32(huvud, (unsigned long)n);
    for (int k = 0; k < 4; k++)
        huvud[4 + k] = (uint8_t)typ[k];
    unsigned long c = crc(crc(0, huvud + 4, 4), data, n);
    skriv32(svans, c);
    fwrite(huvud, 1, 8, f);
    fwrite(data, 1, n, f);
    fwrite(svans, 1, 4, f);
}

int png_skriv(const char *fil, const uint8_t *bild, int bredd, int hojd)
{
    crc_starta();
    /* Raderna med filterbyte 0 först; 1 i bilden blir svart. */
    size_t radlangd = (size_t)bredd + 1, ra = radlangd * hojd;
    uint8_t *radata = malloc(ra);
    if (!radata)
        return 0;
    for (int y = 0; y < hojd; y++) {
        radata[y * radlangd] = 0;
        for (int x = 0; x < bredd; x++)
            radata[y * radlangd + 1 + x] = bild[(size_t)y * bredd + x] ? 0x20 : 0xFF;
    }
    /* zlib: huvud, okomprimerade block, Adler-32. */
    size_t block = (ra + 65534) / 65535, zn = 2 + ra + 5 * (block ? block : 1) + 4;
    uint8_t *z = malloc(zn);
    if (!z) {
        free(radata);
        return 0;
    }
    size_t p = 0, kvar = ra, i = 0;
    z[p++] = 0x78;
    z[p++] = 0x01;
    do {
        size_t n = kvar < 65535 ? kvar : 65535;
        z[p++] = kvar == n;                  /* sista blocket */
        z[p++] = (uint8_t)n;  z[p++] = (uint8_t)(n >> 8);
        z[p++] = (uint8_t)~n; z[p++] = (uint8_t)(~n >> 8);
        for (size_t k = 0; k < n; k++)
            z[p++] = radata[i++];
        kvar -= n;
    } while (kvar);
    unsigned long a = 1, b = 0;
    for (size_t k = 0; k < ra; k++) {
        a = (a + radata[k]) % 65521;
        b = (b + a) % 65521;
    }
    skriv32(z + p, b << 16 | a);
    p += 4;

    FILE *f = fopen(fil, "wb");
    if (!f) {
        free(radata);
        free(z);
        return 0;
    }
    static const uint8_t signatur[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
    uint8_t ihdr[13];
    skriv32(ihdr, (unsigned long)bredd);
    skriv32(ihdr + 4, (unsigned long)hojd);
    ihdr[8] = 8;                             /* 8 bitar */
    ihdr[9] = 0;                             /* grått */
    ihdr[10] = ihdr[11] = ihdr[12] = 0;
    fwrite(signatur, 1, 8, f);
    stycke(f, "IHDR", ihdr, 13);
    stycke(f, "IDAT", z, p);
    stycke(f, "IEND", NULL, 0);
    int ok = !ferror(f);
    fclose(f);
    free(radata);
    free(z);
    return ok;
}
