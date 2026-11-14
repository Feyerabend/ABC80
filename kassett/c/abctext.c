/*
 * abctext.c  text mellan ABC och dagens filer: teckenkod och radslut.
 *
 *   abctext [--kod 7|dos|utf8] [--rad cr|crlf|lf] [-o UT] FIL ...
 *   abctext --abc [--kod 7|dos|utf8] [--rad cr|crlf|lf] [-o UT] FIL ...
 *
 * Utan --abc: från ABC (7 bitar, radslut CR eller CR LF) till dagens fil.
 *   --kod  7     som den är ([\]{|} och så vidare)
 *          dos   ÄÖÅÜÉ äöåüé i teckentabell 850 (MS-DOS)
 *          utf8  i UTF-8 (standard)
 *   --rad  cr, crlf eller lf (standard)
 *
 * Med --abc: från dagens fil till ABC. --kod säger vad filen har (utan:
 * gissa; UTF-8 om det går, annars 850 om där finns byte över $7F,
 * annars 7). Tecken som ABC inte har blir '?'. --rad är radslutet i
 * filen som skrivs (standard cr, som ABC).
 *
 * Filerna läses efter varandra (inga: stdin, '-' också) och skrivs till
 * UT (eller stdout). '$' är alltid '$': ¤ står bara på tangenten.
 *
 * S. Lonnert 2026. Public domain.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "abckas.h"

static byte *las_allt(FILE *f, long *n)
{
    byte *d = NULL;
    long cap = 0;
    size_t k;
    *n = 0;
    for (;;) {
        if (*n == cap) {
            cap = cap ? 2 * cap : 65536L;
            d = (byte *) realloc(d, (size_t) cap);
            if (!d) {
                fprintf(stderr, "abctext: minnet tog slut\n");
                exit(2);
            }
        }
        k = fread(d + *n, 1, (size_t) (cap - *n), f);
        if (k == 0)
            break;
        *n += (long) k;
    }
    return d;
}

static void anvandning(void)
{
    fprintf(stderr, "användning: abctext [--abc] [--kod 7|dos|utf8] "
            "[--rad cr|crlf|lf] [-o UT] FIL ...\n");
    exit(2);
}

int main(int argc, char **argv)
{
    int i, kod = -1, abc = 0, filer = 0, fel = 0;
    const char *rad = NULL, *utnamn = NULL;
    FILE *ut = stdout;

    for (i = 1; i < argc; i++) {
        const char *a = argv[i], *v;
        if (!strcmp(a, "--abc"))
            abc = 1;
        else if (!strncmp(a, "--kod", 5) || !strncmp(a, "--rad", 5)) {
            if (a[5] == '=')
                v = a + 6;
            else if (!a[5] && i + 1 < argc) {
                argv[i] = NULL;
                v = argv[++i];
            } else
                anvandning();
            if (a[2] == 'k') {
                if ((kod = kod_nr(v)) < 0)
                    anvandning();
            } else if (!(rad = rad_text(v)))
                anvandning();
        } else if (!strcmp(a, "-o")) {
            if (i + 1 >= argc)
                anvandning();
            argv[i] = NULL;
            utnamn = argv[++i];
        } else if (a[0] == '-' && a[1])
            anvandning();
        else {
            filer++;
            continue;
        }
        argv[i] = NULL;
    }
    if (utnamn && !(ut = fopen(utnamn, "wb"))) {
        fprintf(stderr, "abctext: %s går inte att skriva\n", utnamn);
        return 2;
    }
    if (!filer)
        argv[0] = (char *) "-";             /* inga filer: stdin */
    for (i = filer ? 1 : 0; i < (filer ? argc : 1); i++) {
        byte *d;
        long n;
        FILE *f;
        if (!argv[i])
            continue;
        if (!strcmp(argv[i], "-"))
            d = las_allt(stdin, &n);
        else {
            if (!(f = fopen(argv[i], "rb"))) {
                fprintf(stderr, "abctext: %s: går inte att öppna\n", argv[i]);
                fel++;
                continue;
            }
            d = las_allt(f, &n);
            fclose(f);
        }
        if (abc) {                          /* varje fil för sig */
            long m;
            byte *t = text_in(d, n, kod, &m);
            text_ut(ut, t, m, KOD_7, rad ? rad : "\r");
            free(t);
        } else
            text_ut(ut, d, n, kod < 0 ? KOD_UTF8 : kod, rad ? rad : "\n");
        free(d);
    }
    if (ut != stdout)
        fclose(ut);
    return fel ? 1 : 0;
}
