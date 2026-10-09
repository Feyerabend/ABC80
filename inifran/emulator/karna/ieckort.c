/*
 * ieckort.c -- IEC-kortet på ABC-bussen, till boken ABC80 inifrån
 *
 * Se ieckort.h. Registren i TMS 9914A som används här:
 *     läs 0  INT STATUS 0          skriv 3  AUX CMD
 *     läs 7  DATA IN               skriv 4  ADDRESS
 *                                  skriv 7  DATA OUT
 */

#include <stdio.h>
#include <string.h>

#include "ieckort.h"

enum { BI = 0x20, BO = 0x10, END = 0x08 };

static void logga(Ieckort *k, const char *post)
{
    int n = (int)strlen(post);
    if (k->logglangd + n + 8 >= IECKORT_LOGG)
        return;
    if (k->loggposter % 12 == 0)
        k->logglangd += snprintf(k->logg + k->logglangd, (size_t)(IECKORT_LOGG - k->logglangd),
                                 k->loggposter ? "\nIEC:" : "IEC:");
    k->logglangd += snprintf(k->logg + k->logglangd, (size_t)(IECKORT_LOGG - k->logglangd),
                             " %s", post);
    k->loggposter++;
}

/* AUX CMD: bit 0-4 är kommandot, bit 7 sätter eller släpper det. */
static void hjalpkommando(Ieckort *k, uint8_t varde)
{
    static const char *const namn[32] = {
        "swrst", 0, "rhdf", "hdfa", 0, 0, 0, 0,
        "feoi", "lon", "ton", "gts", "tca", "tcs", 0, "sic", "sre"
    };
    int kommando = varde & 0x1F, satt = varde >> 7;
    /* rhdf, feoi, gts, tca och tcs är pulser och har ingen 0-form. */
    int puls = kommando == 0x02 || kommando == 0x08
            || (kommando >= 0x0B && kommando <= 0x0D);
    char post[16];
    if (namn[kommando])
        snprintf(post, sizeof post, "%s%s", namn[kommando], satt || puls ? "" : "0");
    else
        snprintf(post, sizeof post, "aux%02X", varde);
    logga(k, post);

    switch (kommando) {
    case 0x00:                               /* swrst: nollställ */
        k->nollstalld = satt;
        if (satt)
            k->lyssnare = k->talare = k->atn = k->styr = k->haller = k->eoi_nasta
                = k->tecken_inne = k->avbrott0 = 0;
        break;
    case 0x02: k->haller = 0; break;         /* rhdf: släpp handskakningen */
    case 0x03: k->hdfa = satt; break;        /* hdfa: håll efter varje tecken */
    case 0x08: k->eoi_nasta = 1; break;      /* feoi: EOI med nästa tecken */
    case 0x09: k->lyssnare = satt; break;    /* lon */
    case 0x0A:                               /* ton */
        k->talare = satt;
        if (satt && !k->atn)
            k->avbrott0 |= BO;
        break;
    case 0x0B:                               /* gts: ATN falsk */
        k->atn = 0;
        if (k->talare)
            k->avbrott0 |= BO;
        break;
    case 0x0C: case 0x0D:                    /* tca, tcs: ATN sann */
        if (k->styr) {
            k->atn = 1;
            k->avbrott0 |= BO;
        }
        break;
    case 0x0F:                               /* sic: ta kontrollen */
        if (satt) {
            k->styr = k->atn = 1;
            k->avbrott0 |= BO;
        }
        break;
    }
}

static void data_ut(Ieckort *k, uint8_t varde)
{
    char post[8];
    if (k->nollstalld)
        return;
    if (k->atn && k->styr)
        snprintf(post, sizeof post, "C%02X", varde);
    else if (k->talare)
        snprintf(post, sizeof post, "D%02X%s", varde, k->eoi_nasta ? "E" : "");
    else
        snprintf(post, sizeof post, "X%02X", varde);
    logga(k, post);
    k->eoi_nasta = 0;
    if (!k->utan_lyssnare)
        k->avbrott0 |= BO;
}

static int svarar(void *data, int nummer)
{
    (void)data;
    return nummer == 49;
}

static uint8_t in(void *data, int port)
{
    Ieckort *k = data;
    if (port == 1)
        return (uint8_t)(0xE0 | k->adress);
    if (port != 0)
        return 0xFF;
    switch (k->valt_register) {
    case 0: {                                /* INT STATUS 0 */
        if (k->lyssnare && !k->atn && !k->haller && !k->tecken_inne
            && k->nasta_text < k->antal_text && !k->nollstalld) {
            k->data_in = k->text[k->nasta_text];
            k->avbrott0 |= BI | (k->text_eoi[k->nasta_text] ? END : 0);
            k->nasta_text++;
            k->tecken_inne = 1;
        }
        uint8_t v = k->avbrott0;
        k->avbrott0 = 0;
        return v;
    }
    case 7:                                  /* DATA IN */
        if (k->tecken_inne) {
            char post[8];
            snprintf(post, sizeof post, "R%02X%s", k->data_in,
                     k->text_eoi[k->nasta_text - 1] ? "E" : "");
            logga(k, post);
            k->tecken_inne = 0;
            if (k->hdfa)
                k->haller = 1;
        }
        return k->data_in;
    }
    return 0;
}

static void ut(void *data, int port, uint8_t varde)
{
    Ieckort *k = data;
    if (port == 2) {
        k->valt_register = varde & 7;
        return;
    }
    if (port != 0)
        return;
    if (k->valt_register == 3)
        hjalpkommando(k, varde);
    else if (k->valt_register == 7)
        data_ut(k, varde);
    else if (k->valt_register == 4) {
        char post[8];
        snprintf(post, sizeof post, "A%02d", varde & 0x1F);
        logga(k, post);
    }
}

void ieckort_starta(Ieckort *k, int adress)
{
    memset(k, 0, sizeof *k);
    k->kort = (Kort){ k, svarar, NULL, in, ut };
    k->adress = adress & 0x1F;
    k->nollstalld = 1;
    k->data_in = 0xFF;
}

void ieckort_text(Ieckort *k, const char *s)
{
    while (*s && k->antal_text < IECKORT_TEXT) {
        int eoi = 0;
        if (s[0] == '\\' && s[1] == 'e') {
            eoi = 1;
            s += 2;
            if (!*s)
                break;
        }
        unsigned tecken = (uint8_t)*s++;
        if (tecken == '\\' && *s == 'r') {
            tecken = 0x0D;
            s++;
        } else if (tecken == '\\' && *s == 'x') {
            sscanf(s + 1, "%2x", &tecken);
            s += 3;
        } else if (tecken == '\\' && *s == '\\')
            s++;
        k->text_eoi[k->antal_text] = (uint8_t)eoi;
        k->text[k->antal_text++] = (uint8_t)tecken;
    }
}
