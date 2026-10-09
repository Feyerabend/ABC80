/*
 * kassettfil.c -- kassettbandet som WAV-fil, till boken ABC80 inifrån
 *
 * Kärnan (karna/kassett.c) ser bandet som flanker med sin plats i
 * T-cykler. Här läses en WAV-fil och görs om till flanker (-T), och
 * flankerna som har spelats in skrivs till en WAV-fil (-U). Själva
 * omvandlingen, med filtren och tröskeln, finns i wav.c, som också
 * används i webbläsaren.
 */

#include <stdio.h>
#include <stdlib.h>

#include "vard.h"
#include "wav.h"

/* Hela filen i minnet; antalet bytes i *storlek. */
static uint8_t *hela_filen(const char *namn, long *storlek)
{
    FILE *f = fopen(namn, "rb");
    if (!f)
        vard_fel("kan inte öppna ", namn);
    fseek(f, 0, SEEK_END);
    *storlek = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc((size_t)*storlek);
    if (!data || fread(data, 1, (size_t)*storlek, f) != (size_t)*storlek)
        vard_fel("kan inte läsa ", namn);
    fclose(f);
    return data;
}

long long *kassett_las_wav(const char *namn, long *antal)
{
    long storlek;
    uint8_t *fil = hela_filen(namn, &storlek);
    const char *fel;
    long n = wav_till_flanker(fil, storlek, NULL, 0, &fel);
    if (n < 0)
        vard_fel(fel, namn);
    long long *flanker = malloc((size_t)(n ? n : 1) * sizeof *flanker);
    if (!flanker)
        vard_fel("för lite minne till ", namn);
    wav_till_flanker(fil, storlek, flanker, n, &fel);
    free(fil);
    *antal = n;
    return flanker;
}

void kassett_skriv_wav(const char *namn, const long long *flanker, long antal,
                       long long slut)
{
    long storlek = wav_fran_flanker(flanker, antal, slut, NULL);
    uint8_t *data = malloc((size_t)storlek);
    if (!data)
        vard_fel("för lite minne till ", namn);
    wav_fran_flanker(flanker, antal, slut, data);
    FILE *f = fopen(namn, "wb");
    if (!f || fwrite(data, 1, (size_t)storlek, f) != (size_t)storlek || fclose(f))
        vard_fel("kan inte skriva ", namn);
    free(data);
}
