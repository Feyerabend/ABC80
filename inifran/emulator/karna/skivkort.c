/*
 * skivkort.c -- skivkorten på ABC-bussen, till boken ABC80 inifrån
 *
 * Se skivkort.h för protokollen. Kontrollern körs inte (dess Z80 och
 * FD1791); kortet svarar direkt, som om skivan alltid vore klar.
 */

#include <string.h>

#include "skivkort.h"

#define FD2 45

/* ---------------------------------------------------------------- */
/* Gemensamt                                                         */

static int sektorn_trasig(const Skivkort *s, long sektor, int vid_skrivning)
{
    for (int k = 0; k < s->antal_trasiga; k++)
        if (s->trasig[k] == sektor || (vid_skrivning && s->trasig[k] == -sektor - 1))
            return 1;
    return 0;
}

static int svarar(void *data, int nummer)
{
    const Skivkort *s = data;
    for (int k = 0; k < s->antal_enheter; k++)
        if (s->enhet[k].kort == nummer)
            return 1;
    return 0;
}

/* ---------------------------------------------------------------- */
/* Kort 44 och 36 (SECIO)                                            */

/* Sektorn i kommandot som en löpande sektor i avbilden. */
static long secio_sektor(const Skivkort *s)
{
    long sektor = s->kommando[2] << 8 | s->kommando[3];
    if (s->enhet[s->aktuell].kort == 36)
        return sektor;
    return (sektor >> 5) * 4 + (sektor & 3);
}

/* Fyra bytes kommando har kommit. */
static void secio_utfor(Skivkort *s)
{
    s->aktuell = -1;
    for (int k = 0; k < s->antal_enheter; k++)
        if (s->enhet[k].kort == s->valt && s->enhet[k].enhet == s->kommando[1])
            s->aktuell = k;
    s->antal = 0;
    s->lage = 3;
    if (s->aktuell < 0) {
        s->status = 0x80;                    /* enheten finns inte */
        return;
    }
    long sektor = secio_sektor(s);
    int las = s->kommando[0] == 3, skriv = s->kommando[0] == 0x0C;
    if (sektor < 0 || sektor >= s->enhet[s->aktuell].sektorer || (!las && !skriv)
        || sektorn_trasig(s, sektor, skriv)) {
        s->status = 0x01;
        return;
    }
    if (las) {
        memcpy(s->buffert, s->enhet[s->aktuell].data + sektor * 256, 256);
        s->lage = 1;
    } else
        s->lage = 2;
}

static uint8_t secio_in(Skivkort *s, int port)
{
    if (port == 1)
        return s->lage == 1 || s->lage == 2 ? 0x01 : 0x81;
    if (s->lage == 1) {                      /* data ut */
        uint8_t v = s->buffert[s->antal++];
        if (s->antal == 256) {
            s->status = 0;
            s->lage = 3;
        }
        return v;
    }
    if (s->lage == 3) {
        s->lage = 0;
        s->antal = 0;
        return s->status;
    }
    return 0xFF;
}

static void secio_ut(Skivkort *s, int port, uint8_t varde)
{
    if (port == 2 || port == 4) {            /* nytt kommando, återställ */
        s->lage = 0;
        s->antal = 0;
        return;
    }
    if (port != 0)
        return;
    if (s->lage == 0) {
        s->kommando[s->antal++] = varde;
        if (s->antal == 4)
            secio_utfor(s);
    } else if (s->lage == 2) {
        s->buffert[s->antal++] = varde;
        if (s->antal == 256) {
            Skivenhet *e = &s->enhet[s->aktuell];
            if (e->skrivskydd)
                s->status = 0x40;
            else {
                memcpy(e->data + secio_sektor(s) * 256, s->buffert, 256);
                s->status = 0;
            }
            s->lage = 3;
        }
    }
}

/* ---------------------------------------------------------------- */
/* Kort 45 (FD2)                                                     */

static void fd2_klart(Skivkort *s, uint8_t status)
{
    s->status = status;
    s->fd2_utan_fel = status == 0;
    s->fd2_lage = 3;
    s->fd2_status_olast = 1;
}

static uint8_t fd2_in(Skivkort *s, int port)
{
    if (port == 1)
        switch (s->fd2_lage) {
        case 0:  return 0x7D;                /* tar emot kommandot */
        case 1:  return 0x3B;                /* data ut */
        case 2:  return 0x7B;                /* data in */
        case 4:  return 0x76;                /* hänger */
        default: return (uint8_t)(0xB6 | (s->fd2_utan_fel ? 8 : 0) | s->fd2_status_olast);
        }
    if (s->fd2_lage == 1) {
        uint8_t v = s->buffert[s->antal++];
        if (s->antal == 256)
            fd2_klart(s, 0);
        return v;
    }
    s->fd2_status_olast = 0;
    return s->status;
}

/* Sektorn i kommandot, eller -1 med felet i *fel. */
static long fd2_sektor(Skivkort *s, uint8_t *fel)
{
    int enhet = s->kommando[1] & 7;
    s->aktuell = -1;
    for (int k = 0; k < s->antal_enheter; k++)
        if (s->enhet[k].kort == FD2 && s->enhet[k].enhet == enhet)
            s->aktuell = k;
    *fel = 0;
    if (enhet >= 2 || s->aktuell < 0)
        *fel = 0x80;                         /* ingen skiva */
    else if (s->kommando[2] >= 40)
        *fel = 0x02;                         /* spåret finns inte */
    else {
        long sektor = s->kommando[2] * 8 + (s->kommando[3] >> 5);
        if (sektor < s->enhet[s->aktuell].sektorer)
            return sektor;
        *fel = 0x10;                         /* sektorn hittas inte */
    }
    return -1;
}

static void fd2_ut(Skivkort *s, int port, uint8_t varde)
{
    uint8_t fel;
    if (port == 2) {                         /* C1: nytt kommando */
        s->antal = 0;
        s->fd2_lage = 0;
        return;
    }
    if (port == 4) {                         /* C3: återställ */
        s->antal = 0;
        fd2_klart(s, 0);
        s->fd2_status_olast = 0;
        return;
    }
    if (port != 0)
        return;
    if (s->fd2_lage == 0) {
        s->kommando[s->antal++] = varde;
        if (s->antal < 4)
            return;
        s->antal = 0;
        if (++s->fd2_kommandon >= s->hang_forsta && s->fd2_kommandon <= s->hang_sista) {
            s->fd2_lage = 4;
            return;
        }
        if (s->kommando[0] & 4) {            /* ta emot bufferten först */
            s->fd2_lage = 2;
            return;
        }
        if (s->kommando[0] & 1) {            /* läs från skivan */
            long sektor = fd2_sektor(s, &fel);
            if (fel) {
                fd2_klart(s, fel);
                return;
            }
            if (sektorn_trasig(s, sektor, 0)) {
                fd2_klart(s, 0x08);          /* CRC-fel */
                return;
            }
            memcpy(s->buffert, s->enhet[s->aktuell].data + sektor * 256, 256);
        }
        if (s->kommando[0] & 2) {            /* skicka bufferten */
            s->fd2_lage = 1;
            return;
        }
        fd2_klart(s, 0);
    } else if (s->fd2_lage == 2) {
        s->buffert[s->antal++] = varde;
        if (s->antal < 256)
            return;
        if (!(s->kommando[0] & 8)) {
            fd2_klart(s, 0);
            return;
        }
        long sektor = fd2_sektor(s, &fel);
        if (fel)
            fd2_klart(s, fel);
        else if (s->enhet[s->aktuell].skrivskydd)
            fd2_klart(s, 0x40);
        else if (sektorn_trasig(s, sektor, 1))
            fd2_klart(s, 0x20);              /* skrivfel */
        else {
            memcpy(s->enhet[s->aktuell].data + sektor * 256, s->buffert, 256);
            fd2_klart(s, 0);
        }
    }
}

/* ---------------------------------------------------------------- */
/* Kortet på bussen                                                  */

static void valj(void *data, int nummer)
{
    Skivkort *s = data;
    if (nummer != FD2) {                     /* FD2 behåller sitt läge */
        s->lage = 0;
        s->antal = 0;
    }
    s->valt = nummer;
}

static uint8_t in(void *data, int port)
{
    Skivkort *s = data;
    if (port > 1)
        return 0xFF;
    return s->valt == FD2 ? fd2_in(s, port) : secio_in(s, port);
}

static void ut(void *data, int port, uint8_t varde)
{
    Skivkort *s = data;
    if (port > 4)
        return;
    if (s->valt == FD2)
        fd2_ut(s, port, varde);
    else
        secio_ut(s, port, varde);
}

void skivkort_starta(Skivkort *s)
{
    memset(s, 0, sizeof *s);
    s->kort = (Kort){ s, svarar, valj, in, ut, 0 };
    s->valt = -1;
    s->aktuell = -1;
    s->fd2_lage = 3;
    s->fd2_utan_fel = 1;
}

int skivkort_enhet(Skivkort *s, int kort, int enhet, uint8_t *data, long storlek,
                   int skrivskydd)
{
    if (s->antal_enheter == SKIVKORT_ENHETER)
        return 0;
    s->enhet[s->antal_enheter++] = (Skivenhet){ kort, enhet, skrivskydd, storlek / 256, data };
    return 1;
}
