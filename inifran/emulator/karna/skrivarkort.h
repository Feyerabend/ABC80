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
 * Avkodningen (V24Avkodare) delas med grafikkortet (hr80v24.c).
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

/* TxD på PIO B, en OUT per bit. */
typedef struct V24Avkodare {
    int bitar[16], antal_bitar;
} V24Avkodare;

typedef struct V24Skrivare {
    PioBEnhet enhet;                         /* maskin.pio_b_enhet = &enhet */
    V24Avkodare avkodare;
    uint8_t utskrift[SKRIVARE_BUFFERT];
    int antal;
} V24Skrivare;

void skrivarkort_starta(Skrivarkort *s, uint8_t status, uint8_t status_upptagen,
                        int antal_upptagen);
void v24_skrivare_starta(V24Skrivare *s);

/* En OUT till port $3A: tecknet när det är helt, annars -1. */
int v24_avkoda(V24Avkodare *a, uint8_t varde);

/* IN från port $3A för en mottagare som alltid är klar (bit 1 = 0). */
uint8_t v24_las_klar(void *data, uint8_t senast_skrivet, int rxd);

#endif
