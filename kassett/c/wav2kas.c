/*
 * wav2kas.c  WAV från ett band (ABC80, ABC 800) -> blockfil (.kas) och en rapport.
 *
 *   wav2kas BAND.wav                  skriver BAND.kas
 *   wav2kas -o UT.kas -r BAND.wav     och rapporten block för block
 *
 * Varje kanal avkodas flera gånger med olika inställningar (-q: en gång),
 * och ett block godtas när summan stämmer. -v visar omgångarna.
 * Avslutas med 1 om någon fil saknar namnblock eller datablock.
 *
 * Ett band från ABC 800-serien avkodas också: blocken med deras synk
 * (00 00 SYN STX), filhuvudet i 700 baud, och sedan en omgång till med
 * bitcellen 32 x tk T för varje tidkonstant som finns i filhuvudena.
 *
 * Samma som python/wav2kas.py.
 *
 * S. Lönnert 2023, 2026. Public domain.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abckas.h"

static void usage(void)
{
    fprintf(stderr, "användning: wav2kas [-o UT.kas] [-r] [-q] [-v] BAND.wav\n");
    exit(2);
}

static void logerr(const char *s)
{
    fprintf(stderr, "%s\n", s);
}

int main(int argc, char **argv)
{
    const char *wav = NULL, *outarg = NULL;
    char *out;
    int rapport = 0, quick = 0, verbose = 0, i, fel = 0;
    Blocks bl;
    Report *rep;
    long nrep, nf, bad = 0, j;
    TapeFile *f;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if ((!strcmp(a, "-o") || !strcmp(a, "--output")) && i + 1 < argc)
            outarg = argv[++i];
        else if (!strcmp(a, "-r") || !strcmp(a, "--rapport"))
            rapport = 1;
        else if (!strcmp(a, "-q") || !strcmp(a, "--quick"))
            quick = 1;
        else if (!strcmp(a, "-v") || !strcmp(a, "--verbose"))
            verbose = 1;
        else if (a[0] == '-' && a[1])
            usage();
        else if (!wav)
            wav = a;
        else
            usage();
    }
    if (!wav)
        usage();
    if (outarg) {
        out = (char *) malloc(strlen(outarg) + 1);
        strcpy(out, outarg);
    } else {
        /* som os.path.splitext: ändelsen efter sista punkten i filnamnet */
        const char *base = strrchr(wav, '/'), *dot;
        size_t n;
        base = base ? base + 1 : wav;
        while (*base == '.')
            base++;
        dot = strrchr(base, '.');
        n = dot ? (size_t) (dot - wav) : strlen(wav);
        out = (char *) malloc(n + 5);
        memcpy(out, wav, n);
        strcpy(out + n, ".kas");
    }

    blocks_init(&bl);
    if (decode_wav(wav, quick, verbose ? logerr : NULL, &bl, &rep, &nrep))
        return 2;
    if (write_kas(out, &bl))
        return 2;
    if (rapport)
        for (j = 0; j < nrep; j++)
            printf("  %3d:%05.2f  %s\n", (int) floor(rep[j].t / 60),
                   fmod(rep[j].t, 60.0), rep[j].status);
    for (j = 0; j < nrep; j++)
        if (strcmp(rep[j].status, "ok"))
            bad++;
    f = group_files(&bl, &nf);
    printf("%s: %ld block, %ld fil(er), %ld trasiga\n", out, bl.n, nf, bad);
    for (j = 0; j < nf; j++) {
        char name[64];
        long nm = file_missing(&f[j], NULL);
        file_name(&f[j], name);
        if (nm || !f[j].header)
            fel = 1;
        fputs("  ", stdout);
        print_padded(name[0] ? name : "(namn saknas)", 12);
        printf(" %-4s %4ld block", kind_name(file_kind(&f[j])), f[j].count);
        if (nm) {
            long *m = (long *) malloc((size_t) nm * sizeof(long)), k;
            file_missing(&f[j], m);
            fputs("  saknas: ", stdout);
            for (k = 0; k < nm; k++)
                printf(k ? ",%ld" : "%ld", m[k]);
            free(m);
        }
        putchar('\n');
    }
    free_files(f, nf);
    free(rep);
    blocks_free(&bl);
    free(out);
    return fel;
}
