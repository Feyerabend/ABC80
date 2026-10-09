/*
 * ljud.c -- ljudkretsen SN76477, till boken ABC80 inifrån
 *
 * Se ljud.h. Modellen är den i dis/04/src/sn76477.c (Pico-varianten)
 * utan PWM och timer: varje prov räknar fram engångspulsen, SLF:en
 * (en triangelvåg), VCO:n, bruset, höljet och blandaren. Spänningarna
 * och tidskonstanterna kommer därifrån och ur MAME (sn76477.cpp).
 */

#include "ljud.h"

#include <math.h>

/* Spänningarna där kondensatorerna vänder (volt). */
#define ENGANGS_MIN    0.0f
#define ENGANGS_MAX    2.5f
#define SLF_MIN        0.33f
#define SLF_MAX        2.37f
#define VCO_MIN        0.33f
#define VCO_MAX        2.72f
#define BRUS_MAX       5.0f
#define BRUS_HOG       3.35f
#define BRUS_LAG       0.74f
#define HOLJE_MAX      4.44f
#define UT_MITT        2.57f                 /* utgången i vila */
#define UT_HOGST       3.51f
#define UT_LAGST       0.715f

/* ABC80:s komponenter. */
#define R_BRUSKLOCKA   47000.0f              /* R26 */
#define R_BRUSFILTER   330000.0f             /* R24 */
#define C_BRUSFILTER   390e-12f              /* C52 */
#define R_AVKLINGNING  47000.0f              /* R23 */
#define C_HOLJE        10e-6f                /* C50 */
#define R_STIGNING     2200.0f               /* R21 */
#define R_AMPLITUD     33000.0f              /* R19 */
#define R_ATERKOPPLING 10000.0f              /* R18 */
#define C_VCO          10e-9f                /* C48 */
#define R_VCO          100000.0f             /* R20 */
#define R_SLF          220000.0f             /* R22 */
#define C_SLF          1e-6f                 /* C51 */
#define C_ENGANGS      0.1e-6f               /* C53 */
#define R_ENGANGS      330000.0f             /* R25 */

/* Utgångens förstärkning som funktion av höljets spänning (tiondels
 * volt), uppmätt på en krets. */
static const float forstarkning_upp[45] = {
    0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.00f, 0.01f,
    0.03f, 0.11f, 0.15f, 0.19f, 0.21f, 0.23f, 0.26f, 0.29f, 0.31f, 0.33f,
    0.36f, 0.38f, 0.41f, 0.43f, 0.46f, 0.49f, 0.52f, 0.54f, 0.57f, 0.60f,
    0.62f, 0.65f, 0.68f, 0.70f, 0.73f, 0.76f, 0.80f, 0.82f, 0.84f, 0.87f,
    0.90f, 0.93f, 0.96f, 0.98f, 1.00f
};
static const float forstarkning_ned[45] = {
     0.00f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f,  0.00f, -0.01f,
    -0.02f, -0.09f, -0.13f, -0.15f, -0.17f, -0.19f, -0.22f, -0.24f, -0.26f, -0.28f,
    -0.30f, -0.32f, -0.34f, -0.37f, -0.39f, -0.41f, -0.44f, -0.46f, -0.48f, -0.51f,
    -0.53f, -0.56f, -0.58f, -0.60f, -0.62f, -0.65f, -0.67f, -0.69f, -0.72f, -0.74f,
    -0.76f, -0.78f, -0.81f, -0.84f, -0.85f
};

void ljud_starta(Ljud *l, long provtakt, long klocka, uint32_t fro)
{
    *l = (Ljud){ 0 };
    l->provtakt = provtakt;
    l->klocka = klocka;
    l->vco_extern = 1;
    l->holje_v = HOLJE_MAX;                  /* "alltid på" börjar fullt */

    l->brusregister = fro | 1;               /* får aldrig vara 0 */
    l->slf_v = SLF_MIN + (SLF_MAX - SLF_MIN) * (float)(fro >> 8 & 0xFF) / 255.0f;
    l->slf_vippa = fro >> 16 & 1;

    float p = (float)provtakt;
    l->engangs_laddning    = (ENGANGS_MAX - ENGANGS_MIN) / (0.8024f * R_ENGANGS * C_ENGANGS + 0.002079f) / p;
    l->engangs_urladdning  = (ENGANGS_MAX - ENGANGS_MIN) / (854.7f * C_ENGANGS + 0.00001795f) / p;
    l->slf_laddning        = (SLF_MAX - SLF_MIN) / (0.5885f * R_SLF * C_SLF + 0.001300f) / p;
    l->slf_urladdning      = (SLF_MAX - SLF_MIN) / (0.5413f * R_SLF * C_SLF + 0.001343f) / p;
    l->vco_steg            = 0.64f * 2.0f * (VCO_MAX - VCO_MIN) / (R_VCO * C_VCO) / p;
    l->brusfilter_laddning   = BRUS_MAX / (0.1571f * R_BRUSFILTER * C_BRUSFILTER + 0.00001430f) / p;
    l->brusfilter_urladdning = BRUS_MAX / (0.1331f * R_BRUSFILTER * C_BRUSFILTER + 0.00001734f) / p;
    l->stigning            = HOLJE_MAX / (R_STIGNING * C_HOLJE) / p;
    l->avklingning         = HOLJE_MAX / (R_AVKLINGNING * C_HOLJE) / p;
    l->topp_v              = 3.818f * (R_ATERKOPPLING / R_AMPLITUD) + 0.03f;
    l->brustakt            = (uint32_t)(339100000.0f * powf(R_BRUSKLOCKA, -0.8849f));
}

/* Verkställ en skrivning till port 6. */
static void verkstall(Ljud *l, uint8_t varde)
{
    int pa = varde & 1;
    if (pa && !l->pa) {                      /* påslaget startar engångspulsen */
        l->engangs_vippa = 1;
        l->engangs_v = ENGANGS_MIN;
    }
    l->pa = pa;
    l->vco_spanning = varde & 2 ? 2.0f : 0.0f;  /* delare 1,5k/1k: 0 eller 2 V */
    l->vco_extern = !(varde & 4);
    l->blandare = (varde >> 5 & 1) << 2 | (varde >> 3 & 1) << 1 | (varde >> 4 & 1);
    l->holje = (varde >> 6 & 1) << 1 | (varde >> 7 & 1);
}

long long ljud_prov_till(const Ljud *l, long long tid)
{
    return tid * l->provtakt / l->klocka;
}

void ljud_skriv(Ljud *l, long long tid, uint8_t varde)
{
    int nasta = (l->sist + 1) % LJUD_HANDELSER;
    if (nasta == l->forst) {                 /* kön är full: den äldsta nu */
        verkstall(l, l->handelse_varde[l->forst]);
        l->forst = (l->forst + 1) % LJUD_HANDELSER;
    }
    l->handelse_tid[l->sist] = tid;
    l->handelse_varde[l->sist] = varde;
    l->sist = nasta;
}

/* Ett prov: kretsen ett steg fram och utgången i volt. */
static float ett_prov(Ljud *l)
{
    /* Engångspulsen. */
    if (l->engangs_vippa) {
        l->engangs_v += l->engangs_laddning;
        if (l->engangs_v >= ENGANGS_MAX) {
            l->engangs_v = ENGANGS_MAX;
            l->engangs_vippa = 0;
        }
    } else {
        l->engangs_v -= l->engangs_urladdning;
        if (l->engangs_v < ENGANGS_MIN)
            l->engangs_v = ENGANGS_MIN;
    }

    /* SLF:en, en triangelvåg. */
    if (!l->slf_vippa) {
        l->slf_v += l->slf_laddning;
        if (l->slf_v >= SLF_MAX) {
            l->slf_v = SLF_MAX;
            l->slf_vippa = 1;
        }
    } else {
        l->slf_v -= l->slf_urladdning;
        if (l->slf_v <= SLF_MIN) {
            l->slf_v = SLF_MIN;
            l->slf_vippa = 0;
        }
    }

    /* VCO:n. Ju högre styrspänning, desto lägre ton: 0 V ger ungefär
       6 400 Hz och 2 V 640 Hz. Med SLF:en sveper tonen mellan dem. */
    float vco_topp;
    if (l->vco_extern) {
        vco_topp = VCO_MIN + (VCO_MAX - VCO_MIN) * (l->vco_spanning / 2.0f);
        if (vco_topp > VCO_MAX)
            vco_topp = VCO_MAX;
        if (vco_topp < VCO_MIN + 0.24f)
            vco_topp = VCO_MIN + 0.24f;
    } else {
        float andel = (l->slf_v - SLF_MIN) / (SLF_MAX - SLF_MIN);
        vco_topp = VCO_MIN + (VCO_MAX - VCO_MIN) * (0.1f + 0.9f * andel);
    }
    if (!l->vco_vippa) {
        l->vco_v += l->vco_steg;
        if (l->vco_v >= vco_topp) {
            l->vco_v = vco_topp;
            l->vco_vippa = 1;
            l->vco_varannan ^= 1;
        }
    } else {
        l->vco_v -= l->vco_steg;
        if (l->vco_v <= VCO_MIN) {
            l->vco_v = VCO_MIN;
            l->vco_vippa = 0;
        }
    }

    /* Bruset: skiftregistret stegas med brusklockan, och filtret gör
       en fyrkantsvåg av det. */
    l->brusfas += l->brustakt;
    while (l->brusfas >= (uint32_t)l->provtakt) {
        l->brusfas -= (uint32_t)l->provtakt;
        uint32_t bit = (l->brusregister >> 28 & 1) ^ (l->brusregister & 1);
        if ((l->brusregister & 0x1000001Fu) == 0)
            bit = 1;
        l->brusregister = l->brusregister >> 1 | bit << 30;
        l->brus_vippa = (int)bit;
    }
    if (l->brus_vippa) {
        l->brusfilter_v += l->brusfilter_laddning;
        if (l->brusfilter_v > BRUS_MAX)
            l->brusfilter_v = BRUS_MAX;
    } else {
        l->brusfilter_v -= l->brusfilter_urladdning;
        if (l->brusfilter_v < 0.0f)
            l->brusfilter_v = 0.0f;
    }
    if (l->brusfilter_v >= BRUS_HOG)
        l->brusfilter_vippa = 0;
    else if (l->brusfilter_v <= BRUS_LAG)
        l->brusfilter_vippa = 1;

    /* Höljet laddas eller laddas ur. */
    int laddar;
    switch (l->holje) {
    case 0:  laddar = l->vco_vippa; break;
    case 1:  laddar = l->engangs_vippa; break;
    case 3:  laddar = l->vco_vippa & l->vco_varannan; break;
    default: laddar = 1; break;
    }
    if (laddar) {
        l->holje_v += l->stigning;
        if (l->holje_v > HOLJE_MAX)
            l->holje_v = HOLJE_MAX;
    } else {
        l->holje_v -= l->avklingning;
        if (l->holje_v < 0.0f)
            l->holje_v = 0.0f;
    }

    /* Blandaren och utgången (som om kretsen vore på; av och på sköts
       i ljud_prov). */
    int ut;
    switch (l->blandare) {
    case 0:  ut = l->vco_vippa; break;
    case 1:  ut = l->slf_vippa; break;
    case 2:  ut = l->brusfilter_vippa; break;
    case 3:  ut = l->vco_vippa & l->brusfilter_vippa; break;
    case 4:  ut = l->slf_vippa & l->brusfilter_vippa; break;
    case 5:  ut = l->vco_vippa & l->slf_vippa & l->brusfilter_vippa; break;
    case 6:  ut = l->vco_vippa & l->slf_vippa; break;
    default: ut = 0; break;
    }
    int steg = (int)(l->holje_v * 10.0f);
    if (steg < 0)
        steg = 0;
    if (steg > 44)
        steg = 44;
    float v;
    if (ut) {
        v = UT_MITT + l->topp_v * forstarkning_upp[steg];
        if (v > UT_HOGST)
            v = UT_HOGST;
    } else {
        v = UT_MITT + l->topp_v * forstarkning_ned[steg];
        if (v < UT_LAGST)
            v = UT_LAGST;
    }
    return v;
}

void ljud_prov(Ljud *l, int16_t *buffert, int antal)
{
    for (int k = 0; k < antal; k++, l->prov++) {
        while (l->forst != l->sist
               && ljud_prov_till(l, l->handelse_tid[l->forst]) <= l->prov) {
            verkstall(l, l->handelse_varde[l->forst]);
            l->forst = (l->forst + 1) % LJUD_HANDELSER;
        }
        /* På- och avslagen tonas på några ms, så att de inte knäpper
           (Pico-versionens filter, men bara på styrkan: tonerna går ut
           med full styrka). */
        l->styrka += ((float)l->pa - l->styrka) * 0.0125f;
        float v = (ett_prov(l) - UT_MITT) * l->styrka;
        /* Utgången är kopplad med en kondensator: likspänningen tas bort
           (ett högpassfilter kring 20 Hz). */
        l->utgang = v - l->forra + 0.995f * l->utgang;
        l->forra = v;
        float s = l->utgang / (UT_MITT - UT_LAGST) * 32767.0f;
        buffert[k] = (int16_t)(s > 32767.0f ? 32767.0f : s < -32767.0f ? -32767.0f : s);
    }
}
