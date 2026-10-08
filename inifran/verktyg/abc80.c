/*
 * abc80.c -- en ABC80 i terminalen, till boken ABC80 inifrån
 *
 * Användning:
 *     abc80 [flaggor]                  kör ABC80 i terminalen
 *     abc80 -t ms [flaggor]            kör utan terminal och skriv skärmen
 *
 * Flaggor:
 *     -r fil         ROM:en, 16 KB ($0000-$3FFF) eller 32 KB (BASIC II,
 *                    $0000-$7BFF); utan -r ../disass/abc80/abc80new.rom
 *                    bredvid programmet
 *     -l fil@adr     en krets (skrivskyddad) på adressen, i hex; flera går bra
 *     -b f0,f1,..@adr  en krets med banker (högst 8 à 4 KB); skrivning till
 *                    adr+$40+n väljer bank n
 *     -m kb          RAM överst i minnet: 16 (standard), 32 eller 48
 *     -k text        tangenter att skriva efter starten
 *     -f fil         en textfil att skriva som tangenter (ett BASIC-program);
 *                    -k och -f skrivs i den ordning de står
 *     -K ms,ms       hur länge en tangent hålls nere och släppt (40,60)
 *     -s text        tecken som tas emot på V24 efter tangenterna
 *     -S baud        hastigheten på V24 (1200)
 *     -W h@ms,...    labplattan på V24: strömbrytarna på ingångarna, bit
 *                    0-2 (hex, 7 = öppna) från tiden ms (@0 kan utelämnas);
 *                    i batchläget skrivs utgångarna (bit 3, 4) efter skärmen
 *     -t ms          batchläge: kör så länge efter tangenterna och skriv
 *                    sedan skärmen på standard ut
 *     -d adr,n       batchläge: skriv också n bytes av minnet från adr
 *
 * I texten till -k och -s är \r RETURN, \xHH koden HH, \\ ett \ och
 * \wN en paus på N ms (bara -k). Å, Ä, Ö, É, Ü och små å, ä, ö, é, ü
 * i UTF-8 blir ABC80:s koder för dem.
 *
 * I terminalen: Ctrl-] avslutar, Ctrl-C är BREAK, piltangenterna vänster
 * och höger är ABC80:s ← och →, backsteg är ←. Medan det finns kvar av
 * -k och -f (och deras pauser) kör emulatorn så fort den kan.
 *
 * Maskinen:
 *     klockan        2,9952 MHz (11,9808 MHz / 4), 59 904 T-cykler per 20 ms
 *     NMI            var 20:e ms (bildens vertikalsignal)
 *     minnet         ROM från $0000, bildminnet $7C00-$7FFF, RAM överst;
 *                    allt annat läser $FF och går inte att skriva
 *     port $38       tangentbordet (PIO A): koden, bit 7 = tangenten är nere
 *     port $39       PIO A:s styrord: läge, avbrott och vektor
 *     port $3A       PIO B: bit 0 = V24 in (RxD); de andra bitarna läser 1.
 *                    Med -W: bit 0-2 V24:s ingångar, bit 3-6 det senast
 *                    skrivna och bit 7 = 1, som på maskinen
 *     avbrott        PIO A i läge 3 med avbrott på: INT när en tangent
 *                    trycks ned; i läge 1: INT från linjesignalen på ASTB,
 *                    7 812,5 gånger i sekunden. Vektorn är PIO A:s.
 * Kassetten, skrivaren och korten på ABC-bussen finns inte med.
 */

#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "z80.h"

#define KLOCKA 2995200L                      /* T-cykler per sekund */
#define BILD   (KLOCKA / 50)                 /* T-cykler per 20 ms */
#define BILDMINNE 0x7C00

typedef struct {
    Z80 z;
    uint8_t minne[65536];
    uint8_t skydd[65536];                    /* 1: går inte att skriva */
    long long t;                             /* T-cykler sedan start */
    long long nmi;                           /* nästa NMI */
    long long linje;                         /* linjesignalen, i T * 15625 */

    uint8_t bank[8][4096];                   /* -b */
    int nbank, bankstorlek, bankadr;

    uint8_t tangent;                         /* port $38 */
    int pio_lage, pio_avbrott, pio_mask;     /* PIO A */
    uint8_t pio_vektor;
    int int_vantar;                          /* INT som inte har tagits */

    int ko[65536];                           /* tangenter: kod, eller 256+ms paus */
    int ko_forst, ko_sist;
    int nere;                                /* tangenten ur kön är nere */
    long long ko_nasta;                      /* när kön går vidare */
    long tryck, slapp;                       /* -K, i ms */

    uint8_t v24[4096];                       /* -s */
    int nv24;
    long long v24_start;                     /* -1: inte börjat */
    long baud;

    int lab[16];                             /* -W: bit 0-2 på ingångarna */
    long long lab_t[16];                     /* från tiden, i T-cykler */
    int nlab;
    uint8_t pio_b;                           /* PIO B: senast skrivet */
} Dator;

static void fel(const char *s, const char *t)
{
    fprintf(stderr, "abc80: %s%s\n", s, t ? t : "");
    exit(1);
}

/* ---------------------------------------------------------------- */
/* Minnet och portarna                                               */

static uint8_t las(void *p, uint16_t a)
{
    return ((Dator *)p)->minne[a];
}

static void skriv(void *p, uint16_t a, uint8_t v)
{
    Dator *d = p;
    if (d->nbank && a >= d->bankadr + 0x40 && a < d->bankadr + 0x40 + d->nbank) {
        memcpy(d->minne + d->bankadr, d->bank[a - d->bankadr - 0x40], d->bankstorlek);
        return;
    }
    if (!d->skydd[a])
        d->minne[a] = v;
}

/* V24 in: 8 bitar, ingen paritet, en stoppbit. Linjen är 1 i vila. */
static int rxd(Dator *d)
{
    if (d->v24_start < 0 || d->t < d->v24_start)
        return 1;
    long long bit = (d->t - d->v24_start) * d->baud / KLOCKA;
    long long tecken = bit / 10;
    int b = bit % 10;
    if (tecken >= d->nv24 || b == 9)
        return 1;                            /* stoppbit */
    if (b == 0)
        return 0;                            /* startbit */
    return d->v24[tecken] >> (b - 1) & 1;
}

/* Labplattan (-W): strömbrytarna på V24:s ingångar, bit 0-2 som PIO B
 * läser dem. Öppna ingångar läses som 1. */
static int lab(Dator *d)
{
    int v = 7;
    for (int k = 0; k < d->nlab; k++)
        if (d->t >= d->lab_t[k])
            v = d->lab[k];
    return v;
}

static uint8_t in(void *p, uint16_t port)
{
    Dator *d = p;
    switch (port & 0xFF) {
    case 0x38: return d->tangent;
    case 0x3A:                               /* bit 3-6 läses tillbaka */
        if (d->nlab)
            return (d->pio_b & 0x78) | 0x80 | (lab(d) & (6 | rxd(d)));
        return 0xFE | rxd(d);
    default: return 0xFF;
    }
}

/* PIO A:s styrord. Efter läge 3 och efter ett avbrottsord med bit 4
 * kommer en mask, som här inte används. */
static void pio_styr(Dator *d, uint8_t v)
{
    if (d->pio_mask) {
        d->pio_mask = 0;
        return;
    }
    if ((v & 0x0F) == 0x0F) {                /* läge */
        d->pio_lage = v >> 6;
        d->pio_mask = d->pio_lage == 3;
    } else if ((v & 0x0F) == 0x07) {         /* avbrott, med eller utan mask */
        d->pio_avbrott = v >> 7;
        d->pio_mask = (v & 0x10) != 0;
    } else if ((v & 0x0F) == 0x03)           /* avbrott på eller av */
        d->pio_avbrott = v >> 7;
    else if (!(v & 1))                       /* vektorn */
        d->pio_vektor = v;
    if (!d->pio_avbrott)
        d->int_vantar = 0;
}

static void ut(void *p, uint16_t port, uint8_t v)
{
    Dator *d = p;
    if ((port & 0xFF) == 0x39)
        pio_styr(d, v);
    else if ((port & 0xFF) == 0x3A)
        d->pio_b = v;
}

/* ---------------------------------------------------------------- */
/* Tangenterna                                                       */

static void ko_in(Dator *d, int v)
{
    int n = (d->ko_sist + 1) % 65536;
    if (n != d->ko_forst) {
        d->ko[d->ko_sist] = v;
        d->ko_sist = n;
    }
}

static int ko_tom(Dator *d)
{
    return d->ko_forst == d->ko_sist && !d->nere && d->t >= d->ko_nasta;
}

/* Nästa steg i kön: släpp tangenten som är nere, tryck ned nästa
 * eller vänta. En nedtryckt tangent ger INT om PIO A är i läge 3. */
static void ko_steg(Dator *d)
{
    if (d->nere) {
        d->tangent &= 0x7F;
        d->nere = 0;
        d->ko_nasta = d->t + d->slapp * (KLOCKA / 1000);
        return;
    }
    if (d->ko_forst == d->ko_sist)
        return;
    int v = d->ko[d->ko_forst];
    d->ko_forst = (d->ko_forst + 1) % 65536;
    if (v >= 256) {
        d->ko_nasta = d->t + (long long)(v - 256) * (KLOCKA / 1000);
        return;
    }
    d->tangent = v | 0x80;
    d->nere = 1;
    if (d->pio_lage == 3 && d->pio_avbrott)
        d->int_vantar = 1;
    d->ko_nasta = d->t + d->tryck * (KLOCKA / 1000);
}

/* Å Ä Ö É Ü och små å ä ö é ü (UTF-8, andra byten efter $C3) till
 * ABC80:s koder; 0 för andra tecken. */
static uint8_t svensk(uint8_t c)
{
    switch (c) {
    case 0x85: return 0x5D; case 0x84: return 0x5B; case 0x96: return 0x5C;
    case 0x89: return 0x40; case 0x9C: return 0x5E;
    case 0xA5: return 0x7D; case 0xA4: return 0x7B; case 0xB6: return 0x7C;
    case 0xA9: return 0x60; case 0xBC: return 0x7E;
    }
    return 0;
}

/* Texten till -k eller -s, med \r, \xHH, \wN och \\, till kön
 * (pauser bara om paus är satt) eller till V24-bufferten. */
static void text(Dator *d, const char *s, int till_v24)
{
    while (*s) {
        int v;
        if (*s == '\\' && s[1]) {
            s++;
            if (*s == 'r') { v = 0x0D; s++; }
            else if (*s == 'x') {
                char h[3] = { s[1], s[1] ? s[2] : 0, 0 };
                v = (int)strtol(h, NULL, 16);
                s += 1 + strlen(h);
            } else if (*s == 'w') {
                char *e;
                long ms = strtol(s + 1, &e, 10);
                s = e;
                if (!till_v24) ko_in(d, 256 + (int)ms);
                continue;
            } else if (*s == '.') { s++; continue; }   /* \. skiljer \w5\.0 */
            else v = (uint8_t)*s++;
        } else if ((uint8_t)*s == 0xC3 && s[1]) {
            v = svensk((uint8_t)s[1]);
            if (!v) v = '?';
            s += 2;
        } else
            v = (uint8_t)*s++;
        if (till_v24) {
            if (d->nv24 < (int)sizeof d->v24) d->v24[d->nv24++] = v;
        } else
            ko_in(d, v);
    }
}

/* En textfil (-f): radslut blir RETURN, med en paus efter. */
static void textfil(Dator *d, const char *fn)
{
    FILE *f = fopen(fn, "rb");
    if (!f)
        fel("kan inte öppna ", fn);
    int c;
    while ((c = getc(f)) != EOF) {
        if (c == '\r')
            continue;
        if (c == '\n') {
            ko_in(d, 0x0D);
            ko_in(d, 256 + 200);
        } else if (c == 0xC3) {
            int c2 = getc(f), v = c2 == EOF ? 0 : svensk((uint8_t)c2);
            ko_in(d, v ? v : '?');
        } else
            ko_in(d, c);
    }
    fclose(f);
}

/* ---------------------------------------------------------------- */
/* Körningen                                                         */

/* Kör minst n T-cykler. */
static void kor(Dator *d, long long n)
{
    long long slut = d->t + n;
    while (d->t < slut) {
        int t = z80_steg(&d->z);
        if (d->t >= d->nmi) {
            d->nmi += BILD;
            t += z80_nmi(&d->z);
        } else if (d->int_vantar) {
            int ta = z80_int(&d->z, d->pio_vektor);
            if (ta) {
                d->int_vantar = 0;
                t += ta;
            }
        }
        d->t += t;
        /* linjesignalen: 15625 / 2 per sekund */
        d->linje += (long long)t * 15625;
        if (d->linje >= 2 * KLOCKA) {
            d->linje -= 2 * KLOCKA;
            if (d->pio_lage == 1 && d->pio_avbrott)
                d->int_vantar = 1;
        }
        if (d->t >= d->ko_nasta)
            ko_steg(d);
    }
}

static long las_fil(Dator *d, const char *fn, int adr, uint8_t *till, long max)
{
    FILE *f = fopen(fn, "rb");
    if (!f)
        fel("kan inte öppna ", fn);
    long n = (long)fread(till, 1, (size_t)max, f);
    fclose(f);
    if (till == d->minne + adr)
        memset(d->skydd + adr, 1, (size_t)n);
    return n;
}

static void starta(Dator *d, const char *rom, int ramkb)
{
    memset(d->minne, 0xFF, sizeof d->minne);
    memset(d->skydd, 1, sizeof d->skydd);
    memset(d->skydd + BILDMINNE, 0, 0x400);
    memset(d->minne + BILDMINNE, ' ', 0x400);
    int ram = 0x10000 - ramkb * 1024;
    if (ram < 0x8000)
        ram = 0x8000;
    memset(d->skydd + ram, 0, (size_t)(0x10000 - ram));

    long n = las_fil(d, rom, 0, d->minne, 0x8000);
    if (n == 0x8000) {                       /* BASIC II: bildminnet är kvar */
        memset(d->skydd + BILDMINNE, 0, 0x400);
        memset(d->minne + BILDMINNE, ' ', 0x400);
    } else if (n != 0x4000)
        fel("ROM:en ska vara 16 eller 32 KB: ", rom);

    d->pio_lage = 1;
    d->pio_vektor = 0x34;
    d->nmi = BILD;
    d->v24_start = -1;
    d->z.dator = d;
    d->z.las = las;
    d->z.skriv = skriv;
    d->z.in = in;
    d->z.ut = ut;
    z80_reset(&d->z);
}

/* ---------------------------------------------------------------- */
/* Skärmen                                                           */

/* Raden rad på skärmen som UTF-8. Bildkretsen (SAA5050) visar koderna
 * $00-$1F som mellanslag; $11-$17 slår på grafik från nästa tecken och
 * $01-$07 slår på text, och varje rad börjar med text. I grafikläget är
 * $20-$3F och $60-$7F 2 x 3 punkter (bit 0-4 och 6) och $40-$5F tecken.
 * Bit 7 är markören: inverterad i terminalen, annars '_'. */
static void skarmrad(Dator *d, int rad, char *ut, int terminal)
{
    static const char *tecken[128] = {
        [0x24] = "¤", [0x40] = "É", [0x5B] = "Ä", [0x5C] = "Ö", [0x5D] = "Å",
        [0x5E] = "Ü", [0x60] = "é", [0x7B] = "ä", [0x7C] = "ö", [0x7D] = "å",
        [0x7E] = "ü",
    };
    const uint8_t *p = d->minne + BILDMINNE + (rad & 7) * 0x80 + (rad >> 3) * 40;
    int grafik = 0;
    char *b = ut;
    for (int k = 0; k < 40; k++) {
        uint8_t c = p[k] & 0x7F, markor = p[k] & 0x80;
        char s[8];
        if (c < 0x20 || c == 0x7F)
            strcpy(s, " ");
        else if (grafik && (c < 0x40 || c >= 0x60)) {
            int m = (c & 0x1F) | (c & 0x40) >> 1;          /* sex punkter */
            unsigned u = m == 0 ? ' ' : m == 21 ? 0x258C : m == 42 ? 0x2590
                       : m == 63 ? 0x2588 : 0x1FB00 + m - 1 - (m > 21) - (m > 42);
            if (u < 0x80)
                sprintf(s, "%c", u);
            else if (u < 0x10000)
                sprintf(s, "%c%c%c", 0xE0 | u >> 12, 0x80 | (u >> 6 & 0x3F),
                        0x80 | (u & 0x3F));
            else
                sprintf(s, "%c%c%c%c", 0xF0 | u >> 18, 0x80 | (u >> 12 & 0x3F),
                        0x80 | (u >> 6 & 0x3F), 0x80 | (u & 0x3F));
        } else if (tecken[c])
            strcpy(s, tecken[c]);
        else
            sprintf(s, "%c", c);
        if (c >= 0x11 && c <= 0x17) grafik = 1;
        if (c >= 0x01 && c <= 0x07) grafik = 0;

        if (markor && terminal)
            b += sprintf(b, "\033[7m%s\033[27m", s);
        else
            b += sprintf(b, "%s", markor ? "_" : s);
    }
    *b = 0;
}

static void skriv_skarm(Dator *d)
{
    char rad[512];
    for (int r = 0; r < 24; r++) {
        skarmrad(d, r, rad, 0);
        size_t n = strlen(rad);
        while (n > 0 && rad[n - 1] == ' ')
            rad[--n] = 0;
        printf("%s\n", rad);
    }
}

static void skriv_minne(Dator *d, const char *arg)
{
    char *e;
    int adr = (int)strtol(arg, &e, 16), n = 16;
    if (*e == ',')
        n = (int)strtol(e + 1, NULL, 0);
    for (int k = 0; k < n; k++) {
        if (k % 16 == 0)
            printf("%s%04X:", k ? "\n" : "", (adr + k) & 0xFFFF);
        printf(" %02X", d->minne[(adr + k) & 0xFFFF]);
    }
    printf("\n");
}

/* ---------------------------------------------------------------- */
/* Terminalen                                                        */

static struct termios forra;

static void aterstall(void)
{
    tcsetattr(0, TCSANOW, &forra);
    printf("\033[?25h\033[?1049l");
    fflush(stdout);
}

static void ratt(void)
{
    if (tcgetattr(0, &forra) < 0)
        fel("standard in är ingen terminal (använd -t för batchläget)", NULL);
    struct termios t = forra;
    t.c_lflag &= ~(ICANON | ECHO | ISIG | IEXTEN);
    t.c_iflag &= ~(IXON | ICRNL | INLCR);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &t);
    atexit(aterstall);
    printf("\033[?1049h\033[?25l\033[H\033[2J");
}

/* Det som har kommit från tangentbordet till kön. Escapesekvenserna
 * för piltangenterna läses i ett stycke; en ensam ESC är ESC. */
static int terminal_in(Dator *d)
{
    uint8_t b[64];
    ssize_t n = read(0, b, sizeof b);
    for (ssize_t k = 0; k < n; k++) {
        uint8_t c = b[k];
        if (c == 0x1D)                       /* Ctrl-] */
            return 0;
        if (c == 0x1B && k + 2 < n && b[k + 1] == '[') {
            if (b[k + 2] == 'D') ko_in(d, 0x08);
            else if (b[k + 2] == 'C') ko_in(d, 0x09);
            k += 2;
            continue;
        }
        if (c == 0x7F) c = 0x08;
        if (c == 0xC3 && k + 1 < n) {
            c = svensk(b[++k]);
            if (!c) continue;
        } else if (c >= 0x80)
            continue;
        ko_in(d, c);
    }
    return 1;
}

static void rita(Dator *d, int allt)
{
    static uint8_t visad[0x400];
    if (!allt && !memcmp(visad, d->minne + BILDMINNE, sizeof visad))
        return;
    memcpy(visad, d->minne + BILDMINNE, sizeof visad);
    char rad[1024];
    printf("\033[H");
    for (int r = 0; r < 24; r++) {
        skarmrad(d, r, rad, 1);
        printf("%s\033[K\r\n", rad);
    }
    printf("\033[2m-- ABC80 -- Ctrl-] avslutar --\033[0m\033[K");
    fflush(stdout);
}

static double nu(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* Kör i realtid: en bild i taget (20 ms), sedan väntan. Så länge -k
 * och -f har kvar tangenter eller pauser i kön går det så fort datorn
 * kan, med skärmen ritad var 20:e ms. */
static void interaktiv(Dator *d)
{
    ratt();
    double nasta = nu();
    for (;;) {
        if (!terminal_in(d))
            return;
        kor(d, BILD);
        if (!ko_tom(d)) {
            double t0 = nu();
            while (!ko_tom(d) && nu() - t0 < 0.02)
                kor(d, BILD);
            rita(d, 0);
            nasta = nu();
            continue;
        }
        rita(d, 0);
        nasta += 0.02;
        double vanta = nasta - nu();
        if (vanta > 0) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(0, &fds);
            struct timeval tv = { 0, (long)(vanta * 1e6) };
            select(1, &fds, NULL, NULL, &tv);
        } else if (vanta < -0.5)
            nasta = nu();                    /* för långsam dator: hoppa */
    }
}

/* ---------------------------------------------------------------- */

/* -W h[@ms],...: strömbrytarna h (hex, bit 0-2) från tiden ms. */
static void labb(Dator *d, const char *s)
{
    while (*s && d->nlab < 16) {
        char *e;
        d->lab[d->nlab] = (int)strtol(s, &e, 16) & 7;
        long ms = 0;
        if (*e == '@')
            ms = strtol(e + 1, &e, 10);
        d->lab_t[d->nlab++] = (long long)ms * (KLOCKA / 1000);
        if (*e != ',')
            break;
        s = e + 1;
    }
}

static void hjalp(void)
{
    fputs("användning: abc80 [-r rom] [-l fil@adr]... [-b f0,f1..@adr] [-m 16|32|48]\n"
          "                  [-k text] [-f fil] [-K ms,ms] [-s text] [-S baud] [-W h@ms,..]\n"
          "                  [-t ms [-d adr,n]...]\n"
          "utan -t körs ABC80 i terminalen (Ctrl-] avslutar)\n", stderr);
    exit(1);
}

int main(int argc, char **argv)
{
    static Dator d;
    static char standard[1024];
    const char *rom = NULL, *v24 = NULL, *banker = NULL;
    const char *laddas[16], *visas[16];
    int nladdas = 0, nvisas = 0, ramkb = 16;
    long efter = -1;

    ko_in(&d, 256 + 1000);                   /* starten */
    d.tryck = 40;
    d.slapp = 60;
    d.baud = 1200;
    for (int k = 1; k < argc; k++) {
        const char *o = argv[k], *v = k + 1 < argc ? argv[k + 1] : NULL;
        if (o[0] != '-' || !o[1] || o[2] || !v)
            hjalp();
        k++;
        switch (o[1]) {
        case 'r': rom = v; break;
        case 'l': if (nladdas < 16) laddas[nladdas++] = v; break;
        case 'b': banker = v; break;
        case 'm': ramkb = atoi(v); break;
        case 'k': text(&d, v, 0); break;                 /* i ordning */
        case 'f': textfil(&d, v); break;
        case 'K': if (sscanf(v, "%ld,%ld", &d.tryck, &d.slapp) != 2) hjalp(); break;
        case 's': v24 = v; break;
        case 'S': d.baud = atol(v); break;
        case 'W': labb(&d, v); break;
        case 't': efter = atol(v); break;
        case 'd': if (nvisas < 16) visas[nvisas++] = v; break;
        default: hjalp();
        }
    }
    if (!rom) {                              /* bredvid programmet */
        const char *s = strrchr(argv[0], '/');
        int n = s ? (int)(s - argv[0] + 1) : 0;
        snprintf(standard, sizeof standard, "%.*s../disass/abc80/abc80new.rom", n, argv[0]);
        rom = standard;
    }

    starta(&d, rom, ramkb);
    for (int k = 0; k < nladdas; k++) {
        char fn[1024];
        snprintf(fn, sizeof fn, "%s", laddas[k]);
        char *at = strrchr(fn, '@');
        if (!at) fel("-l fil@adr: ", laddas[k]);
        *at = 0;
        int adr = (int)strtol(at + 1, NULL, 16);
        las_fil(&d, fn, adr, d.minne + adr, 0x10000 - adr);
    }
    if (banker) {
        char buf[1024];
        snprintf(buf, sizeof buf, "%s", banker);
        char *at = strrchr(buf, '@');
        if (!at) fel("-b fil,fil@adr: ", banker);
        *at = 0;
        d.bankadr = (int)strtol(at + 1, NULL, 16);
        memset(d.bank, 0xFF, sizeof d.bank);
        for (char *f = strtok(buf, ","); f && d.nbank < 8; f = strtok(NULL, ",")) {
            long n = las_fil(&d, f, 0, d.bank[d.nbank++], 4096);
            if (n > d.bankstorlek) d.bankstorlek = (int)n;
        }
        memcpy(d.minne + d.bankadr, d.bank[0], (size_t)d.bankstorlek);
        memset(d.skydd + d.bankadr, 1, (size_t)d.bankstorlek);
    }

    if (v24) text(&d, v24, 1);

    if (efter < 0) {
        if (v24) d.v24_start = 2 * KLOCKA;
        interaktiv(&d);
        return 0;
    }
    while (!ko_tom(&d))
        kor(&d, BILD);
    d.v24_start = d.t + KLOCKA / 10;         /* 100 ms efter tangenterna */
    if (v24)
        kor(&d, d.v24_start - d.t + (long long)d.nv24 * 10 * KLOCKA / d.baud);
    kor(&d, efter * (KLOCKA / 1000));
    skriv_skarm(&d);
    if (d.nlab)
        printf("V24 ut: bit 3 = %d, bit 4 = %d\n", d.pio_b >> 3 & 1, d.pio_b >> 4 & 1);
    for (int k = 0; k < nvisas; k++)
        skriv_minne(&d, visas[k]);
    return 0;
}
