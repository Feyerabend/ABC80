/*
 * ctc.h -- Z80 CTC, till ABC 802 inifrån
 *
 * Fyra kanaler som räknar ned från en tidkonstant (1-256). I timerläget
 * räknar kanalen systemklockans T-cykler genom en förskalning på 16
 * eller 256; i räknarläget räknar den flanker på CLK/TRG. När räknaren
 * når noll laddas tidkonstanten igen, utgången ZC/TO ger en puls och
 * kanalen begär avbrott om det är på. Kanal 0 har högst prioritet, och
 * vektorn är basen (skriven till kanal 0) + 2 x kanalen.
 *
 * I ABC 802 (servicemanualen s. 47, 54-55) får kanal 0-2 1,5 MHz på
 * CLK/TRG och ger klockorna till de seriella kanalerna; kanal 3 ger
 * avbrott 93,75 gånger i sekunden till realtidsklockan.
 *
 * Styrordet: bit 7 avbrott, 6 räknarläge, 5 förskalning 256 (annars
 * 16), 4 stigande flank, 3 timern väntar på trigger, 2 tidkonstant
 * följer, 1 återställ, 0 = 1 (annars är det vektorn, till kanal 0).
 */

#ifndef CTC_H
#define CTC_H

#include <stdint.h>

typedef struct CtcKanal {
    uint8_t styrord;
    int konstant;                            /* 1-256 */
    int konstant_kommer;
    int gar;                                 /* räknar */
    int raknare;
    long long rest;                          /* T-cykler (eller halva flanker)
                                                som inte har räknats än */
    long period_klocka;                      /* T-cykler per flank på CLK/TRG;
                                                0 = inget anslutet */
    int vantar, betjanas;
    long long pulser;                        /* ZC/TO sedan start */
} CtcKanal;

typedef struct Ctc {
    CtcKanal k[4];
    uint8_t vektor;
} Ctc;

void ctc_starta(Ctc *c);

uint8_t ctc_las(Ctc *c, int kanal);
void ctc_skriv(Ctc *c, int kanal, uint8_t v);

/* Tiden går fram t T-cykler. */
void ctc_fram(Ctc *c, long t);

/* T-cykler mellan två pulser på kanalens ZC/TO, 0 om den står. */
long ctc_period(const Ctc *c, int kanal);

/* Avbrotten i kedjan, som i sio.h. */
int ctc_begar(const Ctc *c);
int ctc_betjanad(const Ctc *c);
uint8_t ctc_kvittera(Ctc *c, int kanal);
void ctc_reti(Ctc *c);

#endif
