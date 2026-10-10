/*
 * p40.c -- nålskrivaren P40 på kort 60, till boken ABC80 inifrån
 *
 * Se p40.h: kortets PIO, vagnen och papperet, och vad som är antaget.
 */

#include <string.h>

#include "p40.h"

#define BANAN_T ((long long)P40_BANAN * P40_KOLUMN_T)

/* ---------------------------------------------------------------- */
/* Vagnen                                                            */

/* En utgång som inte är satt som utgång ligger hög, och motorn går bara
 * när dess bit är 0. */
static int motorn(const P40 *p)
{
    uint8_t a = p->ut_a | p->riktning_a;
    if (!(a & 1) && (a & 2))
        return 1;                            /* åt höger */
    if (!(a & 2) && (a & 1))
        return -1;                           /* åt vänster */
    return 0;
}

/* Flytta vagnen fram till nu. Når den en ände matas papperet en rad. */
static void flytta(P40 *p)
{
    long long nu = *p->tid, steg = nu - p->senast;
    p->senast = nu;
    switch (motorn(p)) {
    case 1:
        if (p->lage < BANAN_T && p->lage + steg >= BANAN_T)
            p->rad++;
        p->lage = p->lage + steg < BANAN_T ? p->lage + steg : BANAN_T;
        break;
    case -1:
        if (p->lage > 0 && p->lage - steg <= 0)
            p->rad++;
        p->lage = p->lage - steg > 0 ? p->lage - steg : 0;
        break;
    }
}

/* ---------------------------------------------------------------- */
/* PIO:n                                                             */

/* Styrord: läge 3 ($xF med bit 6-7 = 11) följs av riktningen, 1 = in.
 * Avbrottsorden och vektorn tas inte om hand. */
static void styrord(uint8_t *riktning, int *vantar, uint8_t varde)
{
    if (*vantar) {
        *riktning = varde;
        *vantar = 0;
    } else if ((varde & 0x0F) == 0x0F)
        *vantar = (varde & 0xC0) == 0xC0;
}

static uint8_t kanal_a_in(const P40 *p)
{
    uint8_t in = (uint8_t)(0x13 | p->teckenuppsattning << 6);   /* bit 5 = 0 */
    if (p->lage < BANAN_T)
        in |= 0x04;                          /* inte i högra änden */
    if (p->lage > 0)
        in |= 0x08;                          /* inte i vänstra änden */
    return (uint8_t)((p->ut_a & ~p->riktning_a) | (in & p->riktning_a));
}

/* ---------------------------------------------------------------- */
/* Kortet                                                            */

static int kort_svarar(void *data, int nummer)
{
    (void)data;
    return nummer == P40_KORT;
}

static uint8_t kort_in(void *data, int port)
{
    P40 *p = data;
    if ((port & 7) != 0)
        return 0xFF;
    flytta(p);
    switch (port & 0x60) {
    case 0x00: return kanal_a_in(p);
    case 0x20: return (uint8_t)(p->ut_b & ~p->riktning_b);    /* bit 7: inget fel */
    default:   return 0xFF;
    }
}

static void kort_ut(void *data, int port, uint8_t varde)
{
    P40 *p = data;
    if ((port & 7) != 0)
        return;
    flytta(p);
    switch (port & 0x60) {
    case 0x00:
        p->ut_a = varde;
        break;
    case 0x20: {
        uint8_t nalar = (uint8_t)(varde & ~p->riktning_b & 0x7F);
        p->ut_b = varde;
        if (nalar && p->antal < P40_ANSLAG) {
            P40Anslag *a = &p->anslag[p->antal++];
            a->x = (int32_t)(p->lage * P40_DELAR / P40_KOLUMN_T);
            a->rad = p->rad;
            a->nalar = nalar;
        }
        break;
    }
    case 0x40:
        styrord(&p->riktning_a, &p->vantar_riktning_a, varde);
        break;
    case 0x60:
        styrord(&p->riktning_b, &p->vantar_riktning_b, varde);
        break;
    }
}

void p40_starta(P40 *p, const long long *tid)
{
    memset(p, 0, sizeof *p);
    p->kort = (Kort){ p, kort_svarar, NULL, kort_in, kort_ut, 1 };
    p->tid = tid;
    p->senast = *tid;
    p->riktning_a = p->riktning_b = 0xFF;   /* efter RESET: läge 1, in */
    p->teckenuppsattning = 1;                /* svensk */
}

/* ---------------------------------------------------------------- */
/* Papperet                                                          */

#define RUTA      (4 / P40_DELAR > 0 ? 4 / P40_DELAR : 1)   /* rutor per läge */
#define NALRAD    4
#define RADHOJD   40
#define KANT      8

void p40_papper(const P40 *p, uint8_t *bild, int *bredd, int *hojd)
{
    int rader = 0;
    for (int k = 0; k < p->antal; k++)
        if (p->anslag[k].rad + 1 > rader)
            rader = p->anslag[k].rad + 1;
    int b = P40_BANAN * P40_DELAR * RUTA + 2 * KANT;
    int h = rader * RADHOJD + 2 * KANT;
    *bredd = b;
    *hojd = h;
    if (!bild)
        return;
    memset(bild, 0, (size_t)b * h);
    for (int k = 0; k < p->antal; k++) {
        const P40Anslag *a = &p->anslag[k];
        for (int n = 0; n < 7; n++) {
            if (!(a->nalar >> n & 1))
                continue;
            int x0 = KANT + a->x * RUTA, y0 = KANT + a->rad * RADHOJD + n * NALRAD;
            for (int y = y0; y < y0 + 3 && y < h; y++)
                for (int x = x0; x < x0 + 3 && x < b; x++)
                    bild[y * b + x] = 1;
        }
    }
}
