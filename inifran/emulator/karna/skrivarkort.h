/*
 * skrivarkort.h -- skrivarna, till boken ABC80 inifrån
 *
 * Två sätt att skriva ut, båda fångade i en buffert som värden visar:
 *
 * Skrivarkortet (kort 60) på ABC-bussen, som skrivarkretsen 9704
 * (typerna C, P och U) talar med: valt kort 60 svarar på IN 1 med en
 * status (de första läsningarna kan ge en annan, "inte klar"), och
 * OUT 0 är ett tecken till skrivaren. Port 2-4 tas inte om hand.
 *
 * V24-skrivaren på PIO B (BASIC II:s PR:, PROUT $7247). Port $3A läser
 * bit 1 som 0 (klar), bit 2 som 1 och bit 3-6 som det senast skrivna.
 * TxD är bit 3 i varje OUT medan bit 4 = 0, en OUT per bit: vila,
 * start, 8 data (låg först), stopp; bit 4 = 1 avslutar tecknet.
 */

#ifndef SKRIVARKORT_H
#define SKRIVARKORT_H

#include <stdint.h>

#include "bussen.h"
#include "maskin.h"

#define SKRIVARE_BUFFERT 4096

typedef struct Skrivarkort {
    Kort kort;                               /* kopplas till bussen */
    uint8_t status;                          /* IN 1 */
    uint8_t status_upptagen;                 /* IN 1 de första gångerna */
    int antal_upptagen;                      /* så många gånger till */
    uint8_t utskrift[SKRIVARE_BUFFERT];
    int antal;
} Skrivarkort;

typedef struct V24Skrivare {
    PioBEnhet enhet;                         /* maskin.pio_b_enhet = &enhet */
    int bitar[16], antal_bitar;
    uint8_t utskrift[SKRIVARE_BUFFERT];
    int antal;
} V24Skrivare;

void skrivarkort_starta(Skrivarkort *s, uint8_t status, uint8_t status_upptagen,
                        int antal_upptagen);
void v24_skrivare_starta(V24Skrivare *s);

#endif
