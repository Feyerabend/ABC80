/*
 * wav.c -- kassettbandet som WAV i minnet, till boken ABC80 inifrån
 *
 * Inläsningen klarar PCM med 8 eller 16 bitar, en eller flera kanaler
 * (den första används) och vilken provtakt som helst. En flank är där
 * signalen går genom noll, men bara om den sedan når en tröskel på
 * andra sidan: en fjärdedel av den senaste toppen, men aldrig under en
 * fjärdedel av hela inspelningens nivå (som räknas ut i en första
 * genomgång) eller 2 % av full styrka. Den fasta delen gör att brus i
 * tystnaden mellan blocken inte blir flanker. Likspänningen
 * tas bort först med ett högpassfilter kring 20 Hz, och brus över bandets
 * övre gräns med ett lågpassfilter i två steg kring 4 kHz. Det liknar ABC80:s
 * deriverande länk och två komparatorer, som reagerar på branta
 * övergångar åt båda hållen.
 *
 * Utskriften är en fyrkantvåg, 44 100 prov/s, mono, 16 bitar, med halv
 * styrka: den vänder vid varje flank, som skrivsignalen på bit 6.
 */

#include <math.h>
#include <string.h>

#include "../karna/maskin.h"
#include "wav.h"

#define WAV_TAKT 44100

static uint32_t las_32(const uint8_t *p)
{
    return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24;
}

static uint16_t las_16(const uint8_t *p)
{
    return (uint16_t)(p[0] | p[1] << 8);
}

/* Ett prov, -1 till 1. */
static double prov_varde(const uint8_t *p, int bitar)
{
    return bitar == 8 ? (p[0] - 128) / 128.0 : (int16_t)las_16(p) / 32768.0;
}

long wav_till_flanker(const uint8_t *fil, long storlek,
                      long long *flanker, long storsta, const char **fel)
{
    if (storlek < 12 || memcmp(fil, "RIFF", 4) || memcmp(fil + 8, "WAVE", 4)) {
        *fel = "ingen WAV-fil: ";
        return -1;
    }

    /* Bitarna fmt och data. */
    int kanaler = 0, bitar = 0;
    long takt = 0, prov = 0;
    const uint8_t *data = NULL;
    for (long p = 12; p + 8 <= storlek; ) {
        long langd = (long)las_32(fil + p + 4);
        if (langd < 0 || langd > storlek - p - 8)
            langd = storlek - p - 8;
        if (!memcmp(fil + p, "fmt ", 4) && langd >= 16) {
            if (las_16(fil + p + 8) != 1) {
                *fel = "bara PCM: ";
                return -1;
            }
            kanaler = las_16(fil + p + 10);
            takt = (long)las_32(fil + p + 12);
            bitar = las_16(fil + p + 22);
        } else if (!memcmp(fil + p, "data", 4)) {
            data = fil + p + 8;
            prov = langd;
        }
        p += 8 + langd + (langd & 1);
    }
    if (!data || !kanaler || !takt || (bitar != 8 && bitar != 16)) {
        *fel = "WAV-filen ska vara PCM med 8 eller 16 bitar: ";
        return -1;
    }
    int steg = kanaler * bitar / 8;
    prov /= steg;

    /* Filtren: lågpass i två steg, sedan högpass. */
    const double pi = 3.14159265358979;
    double hogpass = exp(-2 * pi * 20.0 / (double)takt);
    double lagpass = exp(-2 * pi * 4000.0 / (double)takt);

    /* Första genomgången: hela inspelningens nivå, den som bara 0,5 % av
     * proven når över (ett stapeldiagram över |y| i steg om 1/1000). */
    enum { STAPLAR = 2000 };
    static long stapel[STAPLAR];
    memset(stapel, 0, sizeof stapel);
    double lag1 = 0, lag2 = 0, forra_in = 0, forra_ut = 0;
    for (long k = 0; k < prov; k++) {
        double x = prov_varde(data + k * steg, bitar);
        lag1 = lagpass * lag1 + (1 - lagpass) * x;
        lag2 = lagpass * lag2 + (1 - lagpass) * lag1;
        double y = hogpass * (forra_ut + lag2 - forra_in);
        forra_in = lag2;
        forra_ut = y;
        int s = (int)(fabs(y) * 1000);
        stapel[s < STAPLAR ? s : STAPLAR - 1]++;
    }
    double niva = 0;
    for (long s = STAPLAR - 1, over = 0; s >= 0; s--)
        if ((over += stapel[s]) > prov / 200) {
            niva = s / 1000.0;
            break;
        }

    /* Andra genomgången: flankerna. */
    long n = 0;
    double topp_avklingning = exp(-1.0 / (0.05 * (double)takt));  /* 50 ms */
    double golv = niva / 4 > 0.02 ? niva / 4 : 0.02;
    double topp = 0;
    double nollgang = 0;                     /* senaste nollgången, i prov */
    int sida = 0;                            /* -1, 0 (okänd) eller 1 */
    lag1 = lag2 = forra_in = forra_ut = 0;
    for (long k = 0; k < prov; k++) {
        double x = prov_varde(data + k * steg, bitar);
        lag1 = lagpass * lag1 + (1 - lagpass) * x;
        lag2 = lagpass * lag2 + (1 - lagpass) * lag1;
        double y = hogpass * (forra_ut + lag2 - forra_in);
        if ((y >= 0) != (forra_ut >= 0) && k > 0)  /* mellan två prov */
            nollgang = k - 1 + forra_ut / (forra_ut - y);
        forra_in = lag2;
        forra_ut = y;

        topp *= topp_avklingning;
        if (fabs(y) > topp)
            topp = fabs(y);
        double troskel = topp / 4 > golv ? topp / 4 : golv;
        int ny = y > troskel ? 1 : y < -troskel ? -1 : sida;
        if (ny == sida)
            continue;
        if (sida) {                          /* en flank, vid nollgången */
            if (flanker && n < storsta)
                flanker[n] = (long long)(nollgang * MASKIN_KLOCKA / takt);
            n++;
        }
        sida = ny;
    }
    return n;
}

long wav_fran_flanker(const long long *flanker, long antal, long long slut,
                      uint8_t *ut)
{
    long long prov = slut * WAV_TAKT / MASKIN_KLOCKA + WAV_TAKT / 2;  /* + 0,5 s */
    uint32_t data = (uint32_t)(prov * 2);
    if (!ut)
        return 44 + (long)data;
    memcpy(ut, "RIFF....WAVEfmt ....\1\0\1\0........\2\0\20\0data....", 44);
    uint32_t tal[] = { 36 + data, 16, WAV_TAKT, WAV_TAKT * 2, data };
    int plats[] = { 4, 16, 24, 28, 40 };
    for (int k = 0; k < 5; k++)
        for (int b = 0; b < 4; b++)
            ut[plats[k] + b] = (uint8_t)(tal[k] >> 8 * b);

    uint8_t *p = ut + 44;
    long nasta = 0;
    int niva = -1;
    for (long long k = 0; k < prov; k++) {
        long long tid = k * MASKIN_KLOCKA / WAV_TAKT;
        while (nasta < antal && flanker[nasta] <= tid) {
            niva = -niva;
            nasta++;
        }
        int16_t x = (int16_t)(niva * 16384);
        *p++ = (uint8_t)(x & 0xFF);
        *p++ = (uint8_t)(x >> 8 & 0xFF);
    }
    return 44 + (long)data;
}
