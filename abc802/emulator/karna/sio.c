/*
 * sio.c -- Z80 SIO/2 och Z80 DART, till ABC 802 inifrån
 *
 * Se sio.h. Det asynkrona och de teckensynkrona lägena; CRC och SDLC
 * finns inte.
 */

#include <string.h>

#include "sio.h"

static void kanal_starta(SioKanal *k)
{
    uint8_t ingangar = k->ingangar;
    long tid = k->tecken_tid;
    uint8_t senast = k->senast;              /* kanalens återställning tömmer inte registret */
    memset(k, 0, sizeof *k);
    k->ingangar = ingangar;
    k->tecken_tid = tid ? tid : 1;
    k->senast = senast;
}

void sio_starta(Sio *s, int dart)
{
    memset(s, 0, sizeof *s);
    s->dart = dart;
    for (int k = 0; k < 2; k++) {
        s->k[k].senast = 0xFF;
        kanal_starta(&s->k[k]);
    }
}

/* ---------------------------------------------------------------- */
/* Avbrotten                                                         */

/* Väntar mottagningen på kanalen på att ge avbrott? 1 tecken, 2
 * specialvillkor, 0 inget. */
static int mottagning_begar(const SioKanal *k)
{
    int lage = k->wr[1] >> 3 & 3;
    if (!lage)
        return 0;
    if (k->overskridning)
        return 2;
    if (!k->antal)
        return 0;
    return lage == 1 ? k->forsta : 1;
}

static int begar(const Sio *s, int niva)
{
    const SioKanal *k = &s->k[niva / 3];
    switch (niva % 3) {
    case 0:  return mottagning_begar(k) != 0;
    case 1:  return (k->wr[1] & 0x02) && k->sandavbrott;
    default: return (k->wr[1] & 0x01) && k->extavbrott;
    }
}

int sio_begar(const Sio *s)
{
    for (int n = 0; n < SIO_NIVAER; n++)
        if (begar(s, n))
            return n;
    return -1;
}

int sio_betjanad(const Sio *s)
{
    for (int n = 0; n < SIO_NIVAER; n++)
        if (s->betjanas[n])
            return n;
    return -1;
}

/* Vektorbitarna 1-3 för nivån (sio.h). */
static uint8_t vektorbitar(const Sio *s, int niva)
{
    static const uint8_t bitar[SIO_NIVAER] = { 6, 4, 5, 2, 0, 1 };
    uint8_t b = bitar[niva];
    if (niva % 3 == 0 && mottagning_begar(&s->k[niva / 3]) == 2)
        b |= 1;                              /* specialvillkor */
    return b;
}

static uint8_t vektor(const Sio *s, int niva)
{
    uint8_t v = s->k[1].wr[2];
    if (!(s->k[1].wr[1] & 0x04))             /* status ändrar inte vektorn */
        return v;
    uint8_t b = niva < 0 ? 3 : vektorbitar(s, niva);
    return (uint8_t)((v & 0xF1) | b << 1);
}

uint8_t sio_kvittera(Sio *s, int niva)
{
    uint8_t v = vektor(s, niva);
    s->betjanas[niva] = 1;
    if (niva % 3 == 0)
        s->k[niva / 3].forsta = 0;
    return v;
}

void sio_reti(Sio *s)
{
    int n = sio_betjanad(s);
    if (n >= 0)
        s->betjanas[n] = 0;
}

/* ---------------------------------------------------------------- */
/* Sändningen och mottagningen                                       */

/* Bufferten till skiftregistret, om det är ledigt. */
static void borja_sanda(SioKanal *k, long long nu)
{
    if (k->skiftar || !k->buffert_full || !(k->wr[5] & 0x08))
        return;
    k->skift = k->buffert;
    k->buffert_full = 0;
    k->skiftar = 1;
    k->skift_klart = nu + k->tecken_tid;
    k->sandavbrott = 1;                      /* bufferten är tom igen */
}

int sio_synkron(const Sio *s, int kanal)
{
    return !s->dart && !(s->k[kanal].wr[4] & 0x0C);
}

/* Synkront: är sändaren på och bufferten tom skickas ett synktecken
 * (WR6, eller WR6 och WR7 om vartannat i bisynk). */
static void fyll(Sio *s, int kanal, long long nu)
{
    SioKanal *k = &s->k[kanal];
    if (k->skiftar || k->buffert_full || !(k->wr[5] & 0x08) || !sio_synkron(s, kanal))
        return;
    int bisynk = (k->wr[4] & 0x30) == 0x10;
    k->skift = k->wr[bisynk && k->fyll ? 7 : 6];
    k->fyll = bisynk && !k->fyll;
    k->skiftar = 1;
    k->skift_klart = nu + k->tecken_tid;
}

void sio_fram(Sio *s, long long nu)
{
    for (int n = 0; n < 2; n++) {
        SioKanal *k = &s->k[n];
        if (k->skiftar && nu >= k->skift_klart) {
            k->skiftar = 0;
            if (s->sant)
                s->sant(s->data, n, k->skift);
            borja_sanda(k, k->skift_klart);
            fyll(s, n, k->skift_klart);
        } else
            fyll(s, n, nu);
    }
}

int sio_ta_emot(Sio *s, int kanal, uint8_t tecken)
{
    SioKanal *k = &s->k[kanal];
    if (!(k->wr[3] & 0x01))
        return 0;
    if (sio_synkron(s, kanal)) {
        int lage = k->wr[4] >> 4 & 3;        /* 0 monosynk, 1 bisynk */
        if (k->jakt) {
            if (lage == 0 ? tecken == k->wr[7]
                : lage == 1 && k->forra == k->wr[6] && tecken == k->wr[7]) {
                k->jakt = 0;
                k->extavbrott = 1;           /* SYNC/HUNT ändras */
            }
            k->forra = tecken;
            return 1;
        }
        if (lage == 0 && (k->wr[3] & 0x02) && tecken == k->wr[7])
            return 1;                        /* synktecknet läggs inte i kön */
    }
    if (k->antal == 3) {
        k->ko[2] = tecken;                   /* det sista skrivs över */
        k->overskridning = 1;
    } else
        k->ko[k->antal++] = tecken;
    return 1;
}

void sio_ingang(Sio *s, int kanal, uint8_t bit, int varde)
{
    SioKanal *k = &s->k[kanal];
    uint8_t ny = varde ? (uint8_t)(k->ingangar | bit) : (uint8_t)(k->ingangar & ~bit);
    if (ny != k->ingangar)
        k->extavbrott = 1;
    k->ingangar = ny;
}

/* ---------------------------------------------------------------- */
/* Portarna                                                          */

static void kommando(Sio *s, int kanal, uint8_t v)
{
    SioKanal *k = &s->k[kanal];
    switch (v >> 3 & 7) {
    case 2:                                  /* återställ extern/status */
        k->extavbrott = 0;
        break;
    case 3:                                  /* återställ kanalen */
        kanal_starta(k);
        s->betjanas[kanal * 3] = s->betjanas[kanal * 3 + 1] = s->betjanas[kanal * 3 + 2] = 0;
        break;
    case 4:                                  /* avbrott vid nästa tecken */
        k->forsta = 1;
        break;
    case 5:                                  /* återställ sändavbrottet */
        k->sandavbrott = 0;
        break;
    case 6:                                  /* återställ felen */
        k->overskridning = 0;
        break;
    case 7:                                  /* RETI, bara kanal A */
        if (kanal == 0)
            sio_reti(s);
        break;
    }
}

uint8_t sio_las(Sio *s, int kanal, int kontroll, long long nu)
{
    sio_fram(s, nu);
    SioKanal *k = &s->k[kanal];
    if (!kontroll) {
        /* Utan nytt tecken ger dataregistret det förra igen (kretsens
           kö har kvar det). Tangentbordet på DART B läses så av INP(34)
           efter avbrottet: SNAKE-proven gör ABS(SYS(8)*INP(34)). */
        if (!k->antal)
            return k->senast;
        k->senast = k->ko[0];
        k->antal--;
        memmove(k->ko, k->ko + 1, (size_t)k->antal);
        return k->senast;
    }
    int r = k->pekare;
    k->pekare = 0;
    switch (r) {
    case 0: {
        uint8_t v = k->ingangar;
        if (sio_synkron(s, kanal))           /* bit 4 är SYNC/HUNT */
            v = (uint8_t)((v & ~SIO_RI) | (k->jakt ? SIO_RI : 0));
        if (k->antal)
            v |= SIO_RX;
        if (!k->buffert_full)
            v |= SIO_TBE;
        if (!k->skiftar)
            v |= SIO_UNDERRUN;
        if (kanal == 0 && sio_begar(s) >= 0)
            v |= SIO_INT;
        return v;
    }
    case 1:
        return (uint8_t)((!k->skiftar && !k->buffert_full) | 0x06
                         | (k->overskridning ? 0x20 : 0));
    case 2:
        if (kanal == 1)
            return vektor(s, sio_begar(s));
        /* fallthrough */
    default:
        return 0xFF;
    }
}

void sio_skriv(Sio *s, int kanal, int kontroll, uint8_t v, long long nu)
{
    sio_fram(s, nu);
    SioKanal *k = &s->k[kanal];
    if (!kontroll) {
        k->buffert = v;
        k->buffert_full = 1;
        k->sandavbrott = 0;
        borja_sanda(k, nu);
        return;
    }
    int r = k->pekare;
    k->pekare = 0;
    if (r == 0) {
        k->pekare = v & 7;
        kommando(s, kanal, v);
        return;
    }
    if (r == 2)                              /* vektorn finns bara i B */
        s->k[1].wr[2] = v;
    else
        k->wr[r] = v;
    if (r == 3 && (v & 0x10)) {              /* börja leta efter synktecknet */
        k->jakt = 1;
        k->forra = (uint8_t)~k->wr[6];
    }
    if (r == 5)
        borja_sanda(k, nu);                  /* sändaren kan ha slagits på */
}
