/*
 * kassett802.c -- bandspelaren på SIO B, till ABC 802 inifrån
 *
 * Se kassett802.h. Motorn och tiden sköts av abc802.c, som vet om SIO:n
 * och CTC:n; här finns bara bandet.
 */

#include <string.h>

#include "kassett802.h"

void kassett802_starta(Kassett802 *k)
{
    memset(k, 0, sizeof *k);
    k->nasta = -1;
}

void kassett802_spela(Kassett802 *k, const uint8_t *band, long antal)
{
    k->spelas = band;
    k->antal_spelas = antal;
    k->spelar_in = 0;
    k->lage = 0;
}

void kassett802_spela_in(Kassett802 *k, uint8_t *buffert, long storlek)
{
    k->inspelning = buffert;
    k->storlek_inspelning = storlek;
    k->antal_inspelat = 0;
    k->spelar_in = 1;
    k->full = 0;
    k->lage = 0;
}

void kassett802_skriv(Kassett802 *k, uint8_t tecken)
{
    if (!k->spelar_in)
        return;
    if (k->lage < k->storlek_inspelning) {
        k->inspelning[k->lage] = tecken;
        if (k->lage >= k->antal_inspelat)
            k->antal_inspelat = k->lage + 1;
    } else
        k->full = 1;
    k->lage++;
}

int kassett802_las(Kassett802 *k, uint8_t *tecken)
{
    *tecken = 0;
    if (k->spelar_in)
        return 0;
    if (k->lage < k->antal_spelas)
        *tecken = k->spelas[k->lage];
    k->lage++;
    return k->lage <= k->antal_spelas;
}
