/*
 * sio.h -- Z80 SIO/2 och Z80 DART, till ABC 802 inifrån
 *
 * Två seriella kanaler, A och B, i en krets. DART:en är SIO:n utan de
 * synkrona lägena; i RR0 har den RI där SIO:n har SYNC. ABC 802 har en
 * av varje (servicemanualen s. 50, kretsschemat blad 1):
 *     DART  $20-$23  A = CH A (V24, skrivaren), B = tangentbordet
 *     SIO/2 $40-$43  A = CH B (V24), B = kassetten
 * Det som emuleras är det asynkrona: skrivregistren WR0-WR7, läsregistren
 * RR0-RR2, mottagningens kö (tre tecken), sändbufferten och
 * skiftregistret, ingångarna DCD, CTS och RI, och avbrotten med vektorn
 * (WR2, bara kanal B) som status kan ändra (WR1 bit 2). I SIO:n finns
 * också de teckensynkrona lägena (WR4 bit 2-3 = 0), som kassetten
 * använder, tecken för tecken: mottagaren letar (hunt) efter
 * synktecknet, WR7 (monosynk) eller WR6 följt av WR7 (bisynk), och tar
 * emot tecknen efter det; synktecknen läggs inte i kön. Sändaren skickar
 * WR6 (bisynk: WR6 och WR7 om vartannat) när bufferten är tom. CRC och
 * SDLC finns inte.
 *
 * Avbrotten i kretsen, högst först (Zilog, Z80 SIO Technical Manual):
 *     0 A mottagning (och specialvillkor)   vektorbitar 110 (111)
 *     1 A sändning                                      100
 *     2 A extern/status                                 101
 *     3 B mottagning (och specialvillkor)               010 (011)
 *     4 B sändning                                      000
 *     5 B extern/status                                 001
 * Vektorbitarna ersätter bit 1-3 i vektorn när WR1 bit 2 är satt i B.
 *
 * Tiden räknas i processorns T-cykler. Kretsen vet hur lång tid ett
 * tecken tar (tecken_tid, som maskinen ställer in efter klockorna) och
 * sio_fram flyttar fram den.
 */

#ifndef SIO_H
#define SIO_H

#include <stdint.h>

#define SIO_NIVAER 6

/* Bitarna i RR0. */
enum {
    SIO_RX = 0x01,        /* ett tecken finns att läsa */
    SIO_INT = 0x02,       /* avbrott väntar (bara kanal A) */
    SIO_TBE = 0x04,       /* sändbufferten är tom */
    SIO_DCD = 0x08,
    SIO_RI = 0x10,        /* RI i DART:en, SYNC/HUNT i SIO:n */
    SIO_CTS = 0x20,
    SIO_UNDERRUN = 0x40,
    SIO_BREAK = 0x80
};

typedef struct SioKanal {
    uint8_t wr[8];                           /* skrivregistren */
    int pekare;                              /* nästa kontrollbyte går till WRn */
    uint8_t ko[3];                           /* mottagna tecken */
    int antal;
    uint8_t senast;                          /* dataregistret: senast lästa tecken */
    int overskridning;                       /* ett tecken gick förlorat */
    int forsta;                              /* WR1 läge 01: väntar på nästa */
    uint8_t ingangar;                        /* DCD, RI, CTS som RR0 visar dem */
    int buffert_full;
    uint8_t buffert;
    int skiftar;                             /* ett tecken är på väg ut */
    uint8_t skift;
    long long skift_klart;                   /* när det är ute */
    int sandavbrott, extavbrott;             /* väntar */
    long tecken_tid;                         /* T-cykler per tecken */
    int jakt;                                /* synkront: letar efter synktecknet */
    uint8_t forra;                           /* det förra tecknet (bisynk) */
    int fyll;                                /* nästa fyllnadstecken är WR7 */
} SioKanal;

typedef struct Sio {
    SioKanal k[2];                           /* A och B */
    int dart;                                /* 1: Z80 DART */
    int betjanas[SIO_NIVAER];                /* avbrott tagna, inte RETI än */
    /* Ett tecken har gått ut på kanalen (0 = A, 1 = B). */
    void *data;
    void (*sant)(void *data, int kanal, uint8_t tecken);
} Sio;

void sio_starta(Sio *s, int dart);

/* Porten: kanal 0 eller 1, data eller kontroll. */
uint8_t sio_las(Sio *s, int kanal, int kontroll, long long nu);
void sio_skriv(Sio *s, int kanal, int kontroll, uint8_t v, long long nu);

/* Ett tecken kommer in på kanalen. 0 om mottagningen är av. I ett
 * synkront läge går tecknen till sökandet efter synktecknet först. */
int sio_ta_emot(Sio *s, int kanal, uint8_t tecken);

/* En ingång (SIO_DCD, SIO_CTS eller SIO_RI) får värdet 0 eller 1, som
 * RR0 visar det. En ändring ger ett externt avbrott om det är på. */
void sio_ingang(Sio *s, int kanal, uint8_t bit, int varde);

/* Är kanalen i ett synkront läge? (Aldrig i DART:en.) */
int sio_synkron(const Sio *s, int kanal);

/* Tiden går fram till nu: sändningen blir klar. Synkront skickas
 * fyllnadstecken när sändaren är på och bufferten tom. */
void sio_fram(Sio *s, long long nu);

/* Avbrotten i kedjan: den högsta nivå som begär avbrott och den högsta
 * som betjänas, -1 om ingen. */
int sio_begar(const Sio *s);
int sio_betjanad(const Sio *s);

/* Processorn tar avbrottet på nivån: vektorn. */
uint8_t sio_kvittera(Sio *s, int niva);

/* RETI: den högsta nivån som betjänas är klar. */
void sio_reti(Sio *s);

#endif
