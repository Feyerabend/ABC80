/*
 * kor.c -- utanför boken: kör en ABC80 med ROM (och krets) och skriver
 * in raderna i en fil, kommandon och programrader som de står. Allt som
 * skrivs ut på skärmen (via PRTXT) kommer på stdout. Efter varje rad
 * körs maskinen tills den väntar på en tangent igen (GETKEY), högst
 * 10 minuter. En rad som börjar med "RUN" mäts: T-cykler från CR till
 * nästa GETKEY skrivs på stderr.
 *
 *   ./kor rom fil.txt [krets@adr ...]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "maskin.h"

#define PRTECKEN 0x01B5     /* PRTXT: A = tecknet */
#define GETKEY   0x02F1     /* L02F1: väntar på en tangent */

static int vantar;

static int fore(void *d, Maskin *m)
{
    (void)d;
    if (m->z80.pc == PRTECKEN) {
        uint8_t a = m->z80.a;
        if (a == 13)
            putchar('\n');
        else if (a >= 32 && a < 127)
            putchar(a);
        else if (a >= 128)
            putchar('~');
    }
    if (m->z80.pc == GETKEY && tangentko_tom(m)) {
        vantar = 1;
        return 1;
    }
    return 0;
}

static void kor_tills_vantar(Maskin *m)
{
    vantar = 0;
    for (int i = 0; i < 600 * 50 && !vantar; i++) {
        maskin_kor(m, MASKIN_BILD);
        m->stoppad = 0;
    }
}

static size_t las(const char *namn, uint8_t *buf, size_t max)
{
    FILE *f = fopen(namn, "rb");
    size_t n = f ? fread(buf, 1, max, f) : 0;
    if (f)
        fclose(f);
    if (!n) {
        fprintf(stderr, "kor: kan inte läsa %s\n", namn);
        exit(1);
    }
    return n;
}

int main(int argc, char **argv)
{
    static Maskin m;
    static uint8_t rom[32768], k[32768];
    if (argc < 3) {
        fprintf(stderr, "kor rom fil.txt [krets@adr ...]\n");
        return 1;
    }
    size_t n = las(argv[1], rom, sizeof rom);
    maskin_starta(&m, 16);
    maskin_rom(&m, rom, n);
    for (int i = 3; i < argc; i++) {
        char namn[512];
        snprintf(namn, sizeof namn, "%s", argv[i]);
        char *at = strrchr(namn, '@');
        if (!at)
            return 1;
        *at = 0;
        size_t kn = las(namn, k, sizeof k);
        maskin_krets(&m, (uint16_t)strtol(at + 1, NULL, 16), k, kn);
    }
    maskin_aterstall(&m);
    m.krokar.fore_steg = fore;
    kor_tills_vantar(&m);
    FILE *f = fopen(argv[2], "r");
    if (!f) {
        fprintf(stderr, "kor: kan inte läsa %s\n", argv[2]);
        return 1;
    }
    char rad[256];
    while (fgets(rad, sizeof rad, f)) {
        size_t r = strcspn(rad, "\r\n");
        rad[r] = 0;
        for (char *s = rad; *s; s++)
            tangentko_lagg(&m, (uint8_t)*s);
        long long t0 = 0;
        if (!strncmp(rad, "RUN", 3)) {
            /* tiden räknas från CR */
            kor_tills_vantar(&m);
            t0 = m.tid;
        }
        tangentko_lagg(&m, 13);
        kor_tills_vantar(&m);
        if (t0)
            fprintf(stderr, "%s: %lld T, %.3f s%s\n", rad, m.tid - t0,
                    (double)(m.tid - t0) / MASKIN_KLOCKA, vantar ? "" : " (ej klar)");
    }
    fclose(f);
    putchar('\n');
    return 0;
}
