/*
 * natkort.c -- nodkortet och centralen i LUXNET (se natkort.h)
 */

#include <stdlib.h>
#include <string.h>

#include "natkort.h"

/* Platserna i ROM:en och i DOS:ens minne (../../dis/build/abc802net.asm). */
#define NETOUT  0x6009
#define NETBAK  0x6DDE
#define VFD3F   0xFD3F                       /* kortet som NETHK valde */
#define DSLOT   0xFD00                       /* kanalens arbetskopia */
#define DUNIT   0xFD01
#define VFD02   0xFD02                       /* posten (från 1) */
#define VFD0A   0xFD0A                       /* antal poster */
#define VFD0E   0xFD0E                       /* platsen i bufferten */
#define VFD0F   0xFD0F
#define DBUFP   0xFD12                       /* buffertarna ($F500) */
#define VFD15   0xFD15                       /* sektorns status (SECIO) */
#define DSYSUN  0xFFF7                       /* UFD: ordet och enheten */
#define VFFF9   0xFFF9

#define UFD     0x1E
#define NODKORT 3

/* Katalogen som sektorer, för program som LIB.BAC som läser den rått
 * (CALL $6066 med enheten 30 i DUNIT): sektorn KATALOG - 1 har antalet
 * poster per katalogsektor på $EF-$FE, sektorerna KATALOG till
 * KATALOG + 15 har 16 poster à 16 bytes som i DOS v.20 (byte 0 = första
 * klustret, inte 0 eller $FF; byte 1 skydden; 2-3 storleken; 4-14
 * namnet). DSYSUN pekar hit. LIB räknar de nollställda bitarna i
 * $00-$A0 i sektor 15 (hela centralen, 1288 kluster à 4 sektorer) och
 * 14 (det lediga); där sätts en bit per använt kluster. Andra sektorer
 * är tomma. */
#define KATALOG 32
#define KLUSTER 1288

/* Biblioteket som en fil, för Dataindustriers L.BAC: OPEN "UFD:" utan
 * namn. Posterna 1-17 är sektorerna KATALOG - 1 till KATALOG + 15 råa,
 * alla 256 bytes (L läser dem i DOS-bufferten). Byte 0 i en post i
 * katalogen är fil nr i i UFD: (från 1), och L läser filens RIB i
 * posten 32 i + 1: storleken (sektorer) på 233-234, gruppen på 235-236,
 * ägaren på 237-244, tiden på 245-250 (år - 1900, månad, dag, timme,
 * minut, sekund), Ne på 251 och skydden på 252 (0 = alla får allt).
 * Resten av posterna är nollor. */
#define BIBLIOTEKET  (-2)
#define BIB_POSTER   0x2000
#define RIB_STORLEK  233
#define RIB_GRUPP    235
#define RIB_AGARE    237
#define RIB_TID      245

/* Felen (koden + $80 i BASICINI.SYS). */
#define FEL_HITTAR_EJ  0x15                  /* 21 Hittar ej filen */
#define FEL_UTANFOR    0x26                  /* 38 Sektornummer utanför filen */
#define FEL_FULL       0x29                  /* 41 Skivan full */
#define FEL_SKYDD      0x2B                  /* 43 Skrivskyddad */
#define FEL_MINNE      0x80                  /* Slut på minnet i centralen */
/* Finns inte: CY = 1 och A = 0, som den lokala OPEN:en. Då går DEXIT
 * tillbaka genast med DE orört, DOSDRV (L699F) gör fel 21 av det och
 * BASIC:en kan pröva .BAS efter .BAC med samma namn (OPENIX). */
#define FEL_FINNS_EJ   0x100

#define AVBOJD (-1)                          /* kortet tar inte funktionen */

/* ---- små hjälpare utan C-biblioteket ----------------------------- */

static int lika(const char *a, const char *b)
{
    while (*a && *a == *b)
        a++, b++;
    return *a == *b;
}

static void kopiera(char *ut, const char *in, int max)
{
    int k = 0;
    while (in[k] && k < max - 1) {
        ut[k] = in[k];
        k++;
    }
    ut[k] = 0;
}

static char stor(char c)
{
    return c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c;
}

/* "prov.bac" -> "PROV    BAC". 0 om namnet inte går. */
static int dosnamn(const char *in, char *ut)
{
    memset(ut, ' ', 11);
    ut[11] = 0;
    int k = 0;
    while (*in && *in != '.') {
        if (k == 8)
            return 0;
        ut[k++] = stor(*in++);
    }
    if (!k)
        return 0;
    if (*in == '.') {
        in++;
        for (k = 8; *in; k++) {
            if (k == 11)
                return 0;
            ut[k] = stor(*in++);
        }
    }
    return 1;
}

void natkort_filnamn(const NatFil *f, char *ut)
{
    int k = 0;
    for (int i = 0; i < 8 && f->namn[i] != ' '; i++)
        ut[k++] = f->namn[i];
    if (f->namn[8] != ' ') {
        ut[k++] = '.';
        for (int i = 8; i < 11 && f->namn[i] != ' '; i++)
            ut[k++] = f->namn[i];
    }
    ut[k] = 0;
}

/* ---- centralen ---------------------------------------------------- */

void natkort_starta(Natkort *n)
{
    for (int k = 0; k < n->antal; k++)
        free(n->filer[k].data);
    memset(n, 0, sizeof *n);
    for (int k = 0; k < 8; k++)
        n->kanal[k] = -1;
}

static int finns_katalog(Natkort *n, const char *katalog)
{
    if (!*katalog)
        return 1;
    for (int k = 0; k < n->antal; k++)
        if (n->filer[k].ar_katalog && !n->filer[k].borta
            && lika(n->filer[k].katalog, katalog))
            return 1;
    return 0;
}

static int hitta(Natkort *n, const char *katalog, const char *namn11)
{
    for (int k = 0; k < n->antal; k++) {
        NatFil *f = &n->filer[k];
        if (!f->ar_katalog && !f->borta && lika(f->katalog, katalog)
            && !memcmp(f->namn, namn11, 11))
            return k;
    }
    return -1;
}

static int ny_post(Natkort *n)
{
    for (int k = 0; k < n->antal; k++)
        if (n->filer[k].borta && !n->filer[k].andrad) {
            free(n->filer[k].data);
            memset(&n->filer[k], 0, sizeof n->filer[k]);
            return k;
        }
    if (n->antal == NATKORT_FILER)
        return -1;
    memset(&n->filer[n->antal], 0, sizeof n->filer[0]);
    return n->antal++;
}

int natkort_katalog(Natkort *n, const char *katalog)
{
    if (!*katalog || finns_katalog(n, katalog))
        return 1;
    int k = ny_post(n);
    if (k < 0)
        return 0;
    kopiera(n->filer[k].katalog, katalog, NATKORT_KATALOG);
    n->filer[k].ar_katalog = 1;
    return 1;
}

/* Plats för minst langd bytes. */
static int rymmer(NatFil *f, long langd)
{
    if (langd <= f->plats)
        return 1;
    long plats = f->plats ? f->plats : 16 * NATKORT_POST;
    while (plats < langd)
        plats *= 2;
    uint8_t *data = malloc((size_t)plats);
    if (!data)
        return 0;
    if (f->langd)
        memcpy(data, f->data, (size_t)f->langd);
    free(f->data);
    f->data = data;
    f->plats = plats;
    return 1;
}

int natkort_fil(Natkort *n, const char *katalog, const char *namn,
                const uint8_t *data, long langd)
{
    char namn11[12];
    if (!dosnamn(namn, namn11) || !natkort_katalog(n, katalog))
        return 0;
    int k = hitta(n, katalog, namn11);
    if (k < 0)
        k = ny_post(n);
    if (k < 0)
        return 0;
    NatFil *f = &n->filer[k];
    kopiera(f->katalog, katalog, NATKORT_KATALOG);
    memcpy(f->namn, namn11, 12);
    memcpy(f->tid, n->nu, sizeof f->tid);
    f->langd = 0;
    if (!rymmer(f, langd))
        return 0;
    if (langd)
        memcpy(f->data, data, (size_t)langd);
    f->langd = langd;
    return 1;
}

/* ---- loggen ------------------------------------------------------- */

static int rad_k;

static void lagg(Natkort *n, const char *s)
{
    while (*s && rad_k < NATKORT_RAD - 1)
        n->rad[rad_k++] = *s++;
    n->rad[rad_k] = 0;
}

static void lagg_hex(Natkort *n, unsigned v, int siffror)
{
    char s[9];
    for (int k = siffror - 1; k >= 0; k--, v >>= 4)
        s[k] = "0123456789ABCDEF"[v & 15];
    s[siffror] = 0;
    lagg(n, s);
}

static void lagg_tal(Natkort *n, long v)
{
    char s[12];
    int k = 11;
    s[k] = 0;
    do {
        s[--k] = (char)('0' + v % 10);
        v /= 10;
    } while (v && k);
    lagg(n, s + k);
}

/* ---- kortet ------------------------------------------------------- */

static uint16_t ord(Abc802 *m, uint16_t a)
{
    return (uint16_t)(abc802_las(m, a) | abc802_las(m, (uint16_t)(a + 1)) << 8);
}

static void skriv_ord(Abc802 *m, uint16_t a, uint16_t v)
{
    abc802_skriv(m, a, (uint8_t)v);
    abc802_skriv(m, (uint16_t)(a + 1), (uint8_t)(v >> 8));
}

static long poster(const NatFil *f)
{
    return (f->langd + NATKORT_POST - 1) / NATKORT_POST;
}

/* Kanalens buffert: sidan i DBUFP plus kanalen (SLTBUF). */
static uint16_t buffert(Abc802 *m, uint8_t b)
{
    return (uint16_t)((abc802_las(m, DBUFP + 1) + (b >> 4)) << 8);
}

/* Den öppnade filen i DSLOT (som den lokala OPEN:en lämnar den). */
static void oppnad(Abc802 *m, const NatFil *f)
{
    abc802_skriv(m, DSLOT, 0);
    abc802_skriv(m, DUNIT, UFD);
    skriv_ord(m, VFD02, 1);
    skriv_ord(m, VFD0A, (uint16_t)poster(f));
    abc802_skriv(m, VFD0E, 3);
    abc802_skriv(m, VFD0F, UFD);
}

/* Filen som kanalen har, BIBLIOTEKET eller -1 (kanalen är inte
 * nätets). */
static int kanalens(Natkort *n, Abc802 *m, uint8_t b)
{
    int k = n->kanal[(b >> 4) & 7];
    if (k == -1 || (abc802_las(m, DUNIT) & 0x1F) != UFD)
        return -1;
    return k;
}

/* Fil nr i (från 0) i UFD: just nu, i katalogens ordning, eller -1. */
static int ufd_fil(Natkort *n, int i)
{
    for (int k = 0; k < n->antal; k++) {
        NatFil *f = &n->filer[k];
        if (f->ar_katalog || f->borta || !lika(f->katalog, n->ufd))
            continue;
        if (!i--)
            return k;
    }
    return -1;
}

static int las_namn(Abc802 *m, uint16_t de, char *namn11)
{
    for (int k = 0; k < 11; k++)
        namn11[k] = (char)abc802_las(m, (uint16_t)(de + k));
    namn11[11] = 0;
    return 1;
}

/* CD: "NAMN" ned, ".." upp, "" eller "/" hem. */
static int cd(Natkort *n, Abc802 *m, uint16_t de, int langd)
{
    char ny[NATKORT_KATALOG];
    char led[NATKORT_KATALOG];
    if (langd >= NATKORT_KATALOG)
        return FEL_HITTAR_EJ;
    for (int k = 0; k < langd; k++)
        led[k] = stor((char)abc802_las(m, (uint16_t)(de + k)));
    while (langd && led[langd - 1] == ' ')
        langd--;
    led[langd] = 0;
    lagg(n, " \"");
    lagg(n, led);
    lagg(n, "\"");
    if (!langd || lika(led, "/")) {
        n->ufd[0] = 0;
        return 0;
    }
    if (lika(led, "..")) {
        int k = (int)strlen(n->ufd);
        while (k && n->ufd[k - 1] != '/')
            k--;
        n->ufd[k ? k - 1 : 0] = 0;
        return 0;
    }
    int k = 0;
    if (n->ufd[0]) {
        kopiera(ny, n->ufd, NATKORT_KATALOG);
        k = (int)strlen(ny);
        ny[k++] = '/';
    }
    if (k + langd >= NATKORT_KATALOG)
        return FEL_HITTAR_EJ;
    memcpy(ny + k, led, (size_t)langd + 1);
    if (!finns_katalog(n, ny))
        return FEL_HITTAR_EJ;
    kopiera(n->ufd, ny, NATKORT_KATALOG);
    return 0;
}

/* Sektor nr i katalogen över UFD: just nu (se KATALOG) till buf. */
static void katalogsektor(Natkort *n, Abc802 *m, uint16_t nr, uint16_t buf)
{
    uint8_t s[256];
    memset(s, 0, sizeof s);
    int i = 0;
    if (nr == 14) {                          /* det använda, hela centralen */
        long sektorer = 0;
        for (int k = 0; k < n->antal; k++)
            if (!n->filer[k].ar_katalog && !n->filer[k].borta)
                sektorer += poster(&n->filer[k]) + 1;
        long kluster = (sektorer + 3) / 4;
        for (long b = 0; b < kluster && b < KLUSTER; b++)
            s[b / 8] |= (uint8_t)(0x80 >> (b % 8));
    }
    for (int k = 0; k < n->antal && nr >= KATALOG - 1; k++) {
        NatFil *f = &n->filer[k];
        if (f->ar_katalog || f->borta || !lika(f->katalog, n->ufd))
            continue;
        int sek = i / 16, plats = (i % 16) * 16;
        i++;
        if (sek >= 16)
            break;
        if (nr == KATALOG - 1)
            s[0xEF + sek]++;
        else if (nr == KATALOG + sek) {
            long p = poster(f) ? poster(f) : 1;
            s[plats] = (uint8_t)(i < 0xFF ? i : 0xFE);   /* se BIBLIOTEKET */
            s[plats + 2] = (uint8_t)p;
            s[plats + 3] = (uint8_t)(p >> 8);
            memcpy(s + plats + 4, f->namn, 11);
        }
    }
    for (i = 0; i < 256; i++)
        abc802_skriv(m, (uint16_t)(buf + i), s[i]);
}

/* Post nr i biblioteket (se BIBLIOTEKET) till buf. */
static void bibliotekspost(Natkort *n, Abc802 *m, uint16_t post, uint16_t buf)
{
    if (post <= 17) {
        katalogsektor(n, m, (uint16_t)(KATALOG - 2 + post), buf);
        return;
    }
    uint8_t s[256];
    memset(s, 0, sizeof s);
    s[1] = (uint8_t)(post - 1);
    s[2] = (uint8_t)((post - 1) >> 8);
    int k = (post - 1) % 32 ? -1 : ufd_fil(n, (post - 1) / 32 - 1);
    if (k >= 0) {
        NatFil *f = &n->filer[k];
        uint8_t *rib = s + 3;
        long p = poster(f) ? poster(f) : 1;
        rib[RIB_STORLEK] = (uint8_t)p;
        rib[RIB_STORLEK + 1] = (uint8_t)(p >> 8);
        rib[RIB_GRUPP] = 1;
        memcpy(rib + RIB_AGARE, "ABC802  ", 8);
        memcpy(rib + RIB_TID, f->tid, 6);
    }
    for (int i = 0; i < 256; i++)
        abc802_skriv(m, (uint16_t)(buf + i), s[i]);
}

static int funktion(Natkort *n, Abc802 *m, int fn, uint8_t b, uint8_t c,
                    uint16_t de, uint8_t h)
{
    char namn11[12];
    int k;
    NatFil *f;
    int kan = (b >> 4) & 7;
    (void)h;

    switch (fn) {
    case 9:                                  /* starten: logga in */
        skriv_ord(m, DSYSUN, KATALOG);       /* UFD: finns (NETUNT) */
        abc802_skriv(m, VFFF9, UFD);
        n->ufd[0] = 0;
        for (k = 0; k < 8; k++)
            n->kanal[k] = -1;
        lagg(n, "starten, inloggad");
        return 0;

    case 2:                                  /* PREPARE */
    case 3:                                  /* OPEN */
        if ((c & 0x1F) != UFD)
            return AVBOJD;
        las_namn(m, de, namn11);
        lagg(n, fn == 2 ? "PREPARE " : "OPEN ");
        if (!memcmp(namn11, "           ", 11)) {
            lagg(n, "biblioteket");
            if (fn == 2)
                return FEL_SKYDD;
            n->kanal[kan] = BIBLIOTEKET;
            n->ta_bort[kan] = 0;
            abc802_skriv(m, DSLOT, 0);
            abc802_skriv(m, DUNIT, UFD);
            skriv_ord(m, VFD02, 1);
            skriv_ord(m, VFD0A, BIB_POSTER);
            abc802_skriv(m, VFD0E, 3);
            abc802_skriv(m, VFD0F, UFD);
            lagg(n, ", kanal ");
            lagg_tal(n, kan);
            return 0;
        }
        lagg(n, namn11);
        k = hitta(n, n->ufd, namn11);
        if (k < 0) {
            if (fn == 3)
                return FEL_FINNS_EJ;
            k = ny_post(n);
            if (k < 0)
                return FEL_FULL;
            f = &n->filer[k];
            kopiera(f->katalog, n->ufd, NATKORT_KATALOG);
            memcpy(f->namn, namn11, 12);
        }
        f = &n->filer[k];
        if (fn == 2) {
            f->langd = 0;
            f->andrad = 1;
            memcpy(f->tid, n->nu, sizeof f->tid);
        }
        n->kanal[kan] = k;
        n->ta_bort[kan] = 0;
        oppnad(m, f);
        lagg(n, ", kanal ");
        lagg_tal(n, kan);
        lagg(n, ", ");
        lagg_tal(n, poster(f));
        lagg(n, " poster");
        return 0;

    case 4:                                  /* CLOSE */
        k = kanalens(n, m, b);
        if (k == -1)
            return AVBOJD;
        lagg(n, "CLOSE kanal ");
        lagg_tal(n, kan);
        if (k >= 0 && n->ta_bort[kan]) {
            f = &n->filer[k];
            f->borta = 1;
            f->andrad = 1;
            lagg(n, ", borttagen");
        }
        n->kanal[kan] = -1;
        n->ta_bort[kan] = 0;
        abc802_skriv(m, DUNIT, 0xFF);        /* som S6603 */
        return 0;

    case 5:                                  /* läs post */
    case 6: {                                /* skriv post */
        k = kanalens(n, m, b);
        if (k == -1)
            return AVBOJD;
        uint16_t post = ord(m, VFD02);
        uint16_t buf = buffert(m, b);
        lagg(n, fn == 5 ? "läs post " : "skriv post ");
        lagg_tal(n, post);
        lagg(n, ", kanal ");
        lagg_tal(n, kan);
        if (post == 0)
            return FEL_UTANFOR;
        if (k == BIBLIOTEKET) {
            lagg(n, ", biblioteket");
            if (fn == 6)
                return FEL_SKYDD;
            if (post > BIB_POSTER)
                return FEL_UTANFOR;
            bibliotekspost(n, m, post, buf);
            skriv_ord(m, VFD02, (uint16_t)(post + 1));
            return 0;
        }
        f = &n->filer[k];
        long plats = (long)(post - 1) * NATKORT_POST;
        if (fn == 5) {
            if (post > poster(f))
                return FEL_UTANFOR;
            abc802_skriv(m, buf, 0);
            skriv_ord(m, (uint16_t)(buf + 1), (uint16_t)(post - 1));
            for (int i = 0; i < NATKORT_POST; i++) {
                long p = plats + i;
                abc802_skriv(m, (uint16_t)(buf + 3 + i),
                             p < f->langd ? f->data[p] : 0);
            }
        } else {
            long slut = plats + NATKORT_POST;
            if (!rymmer(f, slut))
                return FEL_MINNE;
            if (plats > f->langd)
                memset(f->data + f->langd, 0, (size_t)(plats - f->langd));
            for (int i = 0; i < NATKORT_POST; i++)
                f->data[plats + i] = abc802_las(m, (uint16_t)(buf + 3 + i));
            if (slut > f->langd)
                f->langd = slut;
            f->andrad = 1;
            memcpy(f->tid, n->nu, sizeof f->tid);
            skriv_ord(m, VFD0A, (uint16_t)poster(f));
        }
        skriv_ord(m, VFD02, (uint16_t)(post + 1));
        return 0;
    }

    case 7:                                  /* SETEND, $FFFF = KILL */
        k = kanalens(n, m, b);
        if (k == -1)
            return AVBOJD;
        lagg(n, "SETEND ");
        lagg_hex(n, de, 4);
        if (k == BIBLIOTEKET)
            return FEL_SKYDD;
        f = &n->filer[k];
        if (de == 0xFFFF) {
            n->ta_bort[kan] = 1;
            return 0;
        }
        if (de > poster(f))
            return FEL_UTANFOR;
        if ((long)de * NATKORT_POST < f->langd) {
            f->langd = (long)de * NATKORT_POST;
            f->andrad = 1;
            memcpy(f->tid, n->nu, sizeof f->tid);
        }
        skriv_ord(m, VFD0A, de);
        return 0;

    case 1:                                  /* sektorn, C = 3 läs */
        if ((abc802_las(m, DUNIT) & 0x1F) != UFD)
            return AVBOJD;
        lagg(n, c == 3 ? "läs sektor " : "skriv sektor ");
        lagg_tal(n, de);
        if (c != 3)
            return FEL_SKYDD;
        katalogsektor(n, m, de, buffert(m, b));
        abc802_skriv(m, VFD15, 0);
        return 0;

    case 8: {                                /* NAME: DE = det nya namnet */
        k = kanalens(n, m, b);
        if (k == -1)
            return AVBOJD;
        if (k == BIBLIOTEKET) {
            lagg(n, "NAME biblioteket");
            return FEL_SKYDD;
        }
        f = &n->filer[k];
        las_namn(m, de, namn11);
        lagg(n, "NAME ");
        lagg(n, f->namn);
        lagg(n, " AS ");
        lagg(n, namn11);
        if (hitta(n, f->katalog, namn11) >= 0)
            return FEL_FINNS_EJ;             /* CY: fel 64, finns redan */
        int gammal = ny_post(n);             /* den gamla filen bort hos värden */
        if (gammal >= 0) {
            f = &n->filer[k];
            NatFil *g = &n->filer[gammal];
            kopiera(g->katalog, f->katalog, NATKORT_KATALOG);
            memcpy(g->namn, f->namn, 12);
            g->borta = g->andrad = 1;
        }
        memcpy(f->namn, namn11, 12);
        f->andrad = 1;
        return 0;
    }

    case 0x12: {                             /* CD */
        lagg(n, "CD");
        int fel = cd(n, m, de, (b << 8) | c);
        lagg(n, " -> UFD:");
        lagg(n, n->ufd[0] ? n->ufd : "(hemma)");
        return fel;
    }
    }
    return AVBOJD;
}

static int krok(void *data, Abc802 *m)
{
    Natkort *n = data;
    Z80 *z = &m->z80;
    if (abc802_las(m, VFD3F) != NODKORT)
        return 0;                            /* kort $30: inget här */

    /* Stacken från NETHK: BC, DE, AF och återhoppet. */
    uint16_t sp = z->sp;
    uint8_t c = abc802_las(m, sp), b = abc802_las(m, (uint16_t)(sp + 1));
    uint16_t de = ord(m, (uint16_t)(sp + 2));
    int fn = z->l;

    rad_k = 0;
    n->rad[0] = 0;
    int fel = funktion(n, m, fn, b, c, de, z->h);
    if (fel == AVBOJD) {                     /* OUT som vanligt, lokalt */
        if (n->loggat && fn != 1) {          /* 1 (sektorerna) är för tätt */
            rad_k = 0;
            lagg(n, "funktion ");
            lagg_hex(n, (unsigned)fn, 2);
            lagg(n, " lokalt, BC=");
            lagg_hex(n, (unsigned)(b << 8 | c), 4);
            lagg(n, " DE=");
            lagg_hex(n, de, 4);
            n->loggat(n->logg_data, n->rad);
        }
        return 0;
    }

    uint8_t f = abc802_las(m, (uint16_t)(sp + 4));
    uint8_t a = 0;
    if (fel) {
        f |= 1;
        a = (uint8_t)fel;
        if (fel == FEL_FINNS_EJ)
            lagg(n, fn == 8 ? ": finns redan" : ": finns inte");
        else {
            lagg(n, ": fel ");
            lagg_tal(n, fel);
        }
    } else
        f &= (uint8_t)~1;
    abc802_skriv(m, (uint16_t)(sp + 4), f);
    abc802_skriv(m, (uint16_t)(sp + 5), a);

    n->anrop++;
    if (n->loggat)
        n->loggat(n->logg_data, n->rad);

    z->l = 0x80;                             /* klart */
    z->pc = NETBAK;
    return 200;
}

void natkort_koppla(Natkort *n, Abc802 *m)
{
    m->krok = krok;
    m->krok_data = n;
    m->krok_pc = NETOUT;
}
