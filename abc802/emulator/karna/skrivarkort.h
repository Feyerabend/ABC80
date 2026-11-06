/*
 * skrivarkort.h -- skrivarkortet på ABC-bussen, till ABC 802 inifrån
 *
 * Luxors Centronicskort (snabbskrivarinterfacet 55 20502-01, kort 60)
 * som det ses av PR: i ROM:en ($7000, typerna C, P och U; se
 * dis/build/abc802rom.asm, GETDEF och PROUT). Kortet väljs med OUT 1
 * och $BC eller $3C; det jämför bara bit 0-5, och OUT 2-4 (C1-C3) märks
 * inte. OUT 0 är ett tecken till skrivaren, och IN 1 är statusen:
 *     bit 3   SLCT vänd (0 = skrivaren är vald)
 *     bit 4   BUSY
 *     bit 5   PE, papperet slut
 *     bit 6   FAULT vänd (0 = inget fel)
 *     bit 7   låskretsen (0 = redo för nästa tecken)
 * (ABC80 inifrån, band 2, kapitlet Skrivarna, efter Westh 1980, s. 55,
 * bild 33.) Bit 0-2 är inte kopplade och läses som 1 (?), så att en klar
 * skrivare ger $07. Med den statusen gör GETDEF typen till P (bit 0-2
 * ettor), och både C (masken $70) och P (masken $C0) skriver genast.
 *
 * Skrivaren här är alltid klar: tecknen lämnas till värden med tecken().
 * IN 0 (det som PRGET läser) ger $FF.
 *
 * PR: väljer kortet av sig själv bara när inställningen är tom, alltså
 * vid den första OPEN efter RESET; annars med typen i namnet, t.ex.
 * LIST "PR:C".
 */

#ifndef SKRIVARKORT_H
#define SKRIVARKORT_H

#include <stdint.h>

#include "bussen.h"

#define SKRIVARKORT_NUMMER 60
#define SKRIVARKORT_KLAR 0x07

typedef struct Skrivarkort {
    Kort kort;                               /* kopplas till bussen */
    uint8_t status;                          /* IN 1, SKRIVARKORT_KLAR */
    long antal;                              /* tecken till skrivaren */
    void *data;
    void (*tecken)(void *data, uint8_t tecken);
} Skrivarkort;

void skrivarkort_starta(Skrivarkort *s);

#endif
