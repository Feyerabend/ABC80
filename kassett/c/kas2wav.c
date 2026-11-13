/*
 * kas2wav.c  blockfil (.kas) eller vanliga filer -> WAV med ABC80:s signal.
 *
 *   kas2wav -o BAND.wav BAND.kas
 *   kas2wav -o BAND.wav PROG.BAS DATA.BIN     (packas som kas packa)
 *
 * Flaggor: -r/--rate (44100), --bits 8|16, --lead sekunder före varje
 * namnblock (2), --gap sekunder före varje datablock (0,5), och som i
 * kas packa -t/--text, -b/--binar och --tk.
 * Samma som python/kas2wav.py, och samma WAV byte för byte.
 *
 * S. Lönnert 2026. Public domain.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abckas.h"

static void usage(void)
{
    fprintf(stderr, "användning: kas2wav -o BAND.wav [-r RATE] [--bits 8|16] "
            "[--lead S] [--gap S] [-t|-b] [--tk N] FIL ...\n");
    exit(2);
}

static int is_kas(const char *p)
{
    size_t n = strlen(p);
    return n >= 4 && p[n - 4] == '.' && (p[n - 3] | 32) == 'k' &&
        (p[n - 2] | 32) == 'a' && (p[n - 1] | 32) == 's';
}

int main(int argc, char **argv)
{
    const char *out = NULL;
    long rate = 44100;
    int bits = 16, i, rc, kind = KIND_GUESS, tk = -1;
    double lead = 2.0, gap = 0.5;
    Blocks bl;
    char **files = (char **) malloc((size_t) argc * sizeof(char *));
    int nfiles = 0;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i];
        if ((!strcmp(a, "-o") || !strcmp(a, "--output")) && i + 1 < argc)
            out = argv[++i];
        else if ((!strcmp(a, "-r") || !strcmp(a, "--rate")) && i + 1 < argc)
            rate = atol(argv[++i]);
        else if (!strcmp(a, "--bits") && i + 1 < argc)
            bits = atoi(argv[++i]);
        else if (!strcmp(a, "--lead") && i + 1 < argc)
            lead = atof(argv[++i]);
        else if (!strcmp(a, "--gap") && i + 1 < argc)
            gap = atof(argv[++i]);
        else if (!strcmp(a, "-t") || !strcmp(a, "--text"))
            kind = KIND_TEXT;
        else if (!strcmp(a, "-b") || !strcmp(a, "--binar"))
            kind = KIND_BIN;
        else if (!strcmp(a, "--tk") && i + 1 < argc)
            tk = atoi(argv[++i]);
        else if (a[0] == '-' && a[1])
            usage();
        else
            files[nfiles++] = argv[i];
    }
    if (!out || !nfiles || (bits != 8 && bits != 16) || rate <= 0)
        usage();
    blocks_init(&bl);
    for (i = 0; i < nfiles; i++) {
        if (is_kas(files[i])) {
            if (read_kas(files[i], &bl))
                return 1;
        } else {
            long len;
            byte *d = read_file(files[i], &len);
            if (!d) {
                perror(files[i]);
                return 1;
            }
            make_file(&bl, files[i], d, len, kind, tk);
            free(d);
        }
    }
    rc = write_wav(out, &bl, rate, lead, gap, 0.7, bits);
    if (!rc)
        fprintf(stderr, "%s: %ld block\n", out, bl.n);
    blocks_free(&bl);
    free(files);
    return rc;
}
