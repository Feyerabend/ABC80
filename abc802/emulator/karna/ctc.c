/*
 * ctc.c -- Z80 CTC, till ABC 802 inifrån
 *
 * Se ctc.h.
 */

#include <string.h>

#include "ctc.h"

void ctc_starta(Ctc *c)
{
    long klocka[4];
    for (int n = 0; n < 4; n++)
        klocka[n] = c->k[n].period_klocka;
    memset(c, 0, sizeof *c);
    for (int n = 0; n < 4; n++) {
        c->k[n].konstant = 256;
        c->k[n].period_klocka = klocka[n];
    }
}

/* T-cykler per steg i räknaren. */
static long steg(const CtcKanal *k)
{
    if (k->styrord & 0x40)
        return k->period_klocka;
    return k->styrord & 0x20 ? 256 : 16;
}

long ctc_period(const Ctc *c, int kanal)
{
    const CtcKanal *k = &c->k[kanal];
    if (!k->gar)
        return 0;
    return steg(k) * k->konstant;
}

uint8_t ctc_las(Ctc *c, int kanal)
{
    return (uint8_t)c->k[kanal].raknare;
}

void ctc_skriv(Ctc *c, int kanal, uint8_t v)
{
    CtcKanal *k = &c->k[kanal];
    if (k->konstant_kommer) {
        k->konstant_kommer = 0;
        k->konstant = v ? v : 256;
        if (!k->gar) {                       /* startar nu (trigger antas ha kommit) */
            k->gar = 1;
            k->raknare = k->konstant;
            k->rest = 0;
        }
        return;
    }
    if (!(v & 0x01)) {                       /* vektorn */
        if (kanal == 0)
            c->vektor = v & 0xF8;
        return;
    }
    k->styrord = v;
    if (v & 0x02) {                          /* återställ: står tills ny konstant */
        k->gar = 0;
        k->vantar = 0;
    }
    if (v & 0x04)
        k->konstant_kommer = 1;
    if (!(v & 0x80))
        k->vantar = 0;
}

void ctc_fram(Ctc *c, long t)
{
    for (int n = 0; n < 4; n++) {
        CtcKanal *k = &c->k[n];
        long s = steg(k);
        if (!k->gar || !s)
            continue;
        k->rest += t;
        if (k->rest < s)
            continue;
        long long antal = k->rest / s;
        k->rest -= antal * s;
        if (antal < k->raknare) {
            k->raknare -= (int)antal;
            continue;
        }
        antal -= k->raknare;
        k->pulser += 1 + antal / k->konstant;
        k->raknare = k->konstant - (int)(antal % k->konstant);
        if (k->styrord & 0x80)
            k->vantar = 1;
    }
}

int ctc_begar(const Ctc *c)
{
    for (int n = 0; n < 4; n++)
        if (c->k[n].vantar)
            return n;
    return -1;
}

int ctc_betjanad(const Ctc *c)
{
    for (int n = 0; n < 4; n++)
        if (c->k[n].betjanas)
            return n;
    return -1;
}

uint8_t ctc_kvittera(Ctc *c, int kanal)
{
    c->k[kanal].vantar = 0;
    c->k[kanal].betjanas = 1;
    return (uint8_t)(c->vektor | kanal << 1);
}

void ctc_reti(Ctc *c)
{
    int n = ctc_betjanad(c);
    if (n >= 0)
        c->k[n].betjanas = 0;
}
