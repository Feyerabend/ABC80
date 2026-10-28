/*
 * hr80fil.c -- grafikkortet hr80 utan maskinen, till ABC80 inifrån
 *
 * Utanför boken; se karna/hr80.h. Skickar tecken till tolken och
 * skriver bilden som PNG (hr80bild.c). I emulatorn är kortet kopplat
 * till V24 med abc80 -H.
 *
 * Användning:
 *     hr80 [-s text].. [-f fil].. [-u dx,dy,k].. [-m] [-e] fil.png
 *
 *     -s text   tecken till kortet; \xHH är koden HH, \r 13, \n 10,
 *               \e 27 och \\ ett \
 *     -f fil    filens bytes till kortet
 *     -u dx,dy,k  musen: en rörelse i bildens punkter, knappen (1 nere)
 *     -m        rita markören
 *     -e        skriv händelserna i kön och omrita på stdout
 *
 * -s, -f och -u går till kortet i den ordning de står.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../karna/hr80.h"
#include "hr80bild.h"

static Hr80 kort;

static void fel(const char *text, const char *tillagg)
{
    fprintf(stderr, "hr80: %s%s\n", text, tillagg ? tillagg : "");
    exit(1);
}

static int hex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void skicka_text(const char *s)
{
    while (*s) {
        if (*s != '\\') {
            hr80_tecken(&kort, (uint8_t)*s++);
            continue;
        }
        s++;
        switch (*s) {
        case 'x':
            if (hex(s[1]) < 0 || hex(s[2]) < 0)
                fel("\\x vill ha två hexsiffror: ", s - 1);
            hr80_tecken(&kort, (uint8_t)(hex(s[1]) * 16 + hex(s[2])));
            s += 3;
            break;
        case 'r':  hr80_tecken(&kort, 13); s++; break;
        case 'n':  hr80_tecken(&kort, 10); s++; break;
        case 'e':  hr80_tecken(&kort, 27); s++; break;
        case '\\': hr80_tecken(&kort, '\\'); s++; break;
        default:   fel("okänd \\-kod: ", s - 1);
        }
    }
}

static void skicka_fil(const char *namn)
{
    FILE *f = fopen(namn, "rb");
    if (!f)
        fel("kan inte öppna ", namn);
    int c;
    while ((c = getc(f)) != EOF)
        hr80_tecken(&kort, (uint8_t)c);
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *ut = NULL;
    int markor = 0, handelser = 0;
    hr80_starta(&kort);
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-s") && i + 1 < argc)
            skicka_text(argv[++i]);
        else if (!strcmp(argv[i], "-f") && i + 1 < argc)
            skicka_fil(argv[++i]);
        else if (!strcmp(argv[i], "-u") && i + 1 < argc) {
            int dx, dy, k;
            if (sscanf(argv[++i], "%d,%d,%d", &dx, &dy, &k) != 3)
                fel("-u vill ha dx,dy,knapp: ", argv[i]);
            hr80_mus(&kort, dx, dy, k);
        } else if (!strcmp(argv[i], "-m"))
            markor = 1;
        else if (!strcmp(argv[i], "-e"))
            handelser = 1;
        else if (argv[i][0] != '-' && !ut)
            ut = argv[i];
        else
            fel("användning: hr80 [-s text].. [-f fil].. [-u dx,dy,k].. [-m] [-e] fil.png", NULL);
    }
    if (!ut)
        fel("användning: hr80 [-s text].. [-f fil].. [-u dx,dy,k].. [-m] [-e] fil.png", NULL);
    if (!hr80_bild_skriv(&kort, ut, markor))
        fel("kan inte skriva ", ut);
    uint8_t e[4];
    while (handelser && hr80_handelse(&kort, e))
        printf("%c %d %d %d\n", e[0], e[1], e[2], e[3]);
    if (handelser)
        printf("omrita %03X\n", kort.omrita);
    return 0;
}
