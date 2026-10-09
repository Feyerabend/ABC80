/*
 * kassett.c -- bandspelaren och kassettgränssnittet, till boken ABC80 inifrån
 *
 * Se kassett.h för bitarna och varifrån uppgifterna kommer.
 */

#include <string.h>

#include "kassett.h"

void kassett_starta(Kassett *k)
{
    memset(k, 0, sizeof *k);
}

void kassett_spela(Kassett *k, const long long *flanker, long antal)
{
    k->spelas = flanker;
    k->antal_spelas = antal;
    k->nasta = 0;
}

void kassett_spela_in(Kassett *k, long long *buffert, long storlek)
{
    k->inspelning = buffert;
    k->storlek_inspelning = storlek;
    k->antal_inspelat = 0;
    k->spelar_in = 1;
}

long long kassett_lage(const Kassett *k, long long tid)
{
    return k->motor ? k->band + (tid - k->motor_start) : k->band;
}

int kassett_fram(Kassett *k, long long tid)
{
    if (!k->motor || k->spelar_in)
        return 0;
    long long lage = kassett_lage(k, tid);
    int flank = 0;
    while (k->nasta < k->antal_spelas && k->spelas[k->nasta] <= lage) {
        k->nasta++;
        flank = 1;
    }
    /* Latchen hålls nollställd så länge skrivsignalen är 0. */
    if (!flank || !k->skrivsignal || k->latch)
        return 0;
    k->latch = 1;
    return 1;
}

void kassett_skriv(Kassett *k, long long tid, uint8_t varde)
{
    int motor = varde >> 5 & 1, skrivsignal = varde >> 6 & 1;
    kassett_fram(k, tid);
    long long lage = kassett_lage(k, tid);
    if (lage > k->langst)
        k->langst = lage;

    if (k->spelar_in && k->motor && skrivsignal != k->skrivsignal) {
        if (k->antal_inspelat < k->storlek_inspelning)
            k->inspelning[k->antal_inspelat++] = lage;
        else
            k->full = 1;
    }
    k->motorrelaet = motor;
    if (k->spelar_vidare && k->motor)        /* PLAY är nere: bandet går vidare */
        motor = 1;
    if (motor != k->motor) {                 /* bandet startar eller stannar här */
        k->band = lage;
        k->motor_start = tid;
        k->motor = motor;
    }
    k->skrivsignal = skrivsignal;
    if (!skrivsignal)
        k->latch = 0;
}
