/*
 * uni80.c -- MyAB:s kort UNI-80 för CP/M, till boken ABC80 inifrån
 *
 * Se uni80.h för kartorna och varifrån uppgifterna kommer. Maskinen
 * (maskin.c) frågar uni80_var vid varje läsning och skrivning.
 */

#include <string.h>

#include "uni80.h"

void uni80_starta(Uni80 *u)
{
    memset(u, 0, sizeof *u);
    memset(u->ram, 0xFF, sizeof u->ram);
    memset(u->startkrets, 0xFF, sizeof u->startkrets);
    uni80_karta(u, UNI80_SYSTEM);
    u->abc80lage = 1;                        /* tills porten skrivs */
    for (int sida = 0x8000 / UNI80_SIDA; sida < 0xC000 / UNI80_SIDA; sida++)
        u->sidor[sida] = UNI80_EXTRA;
}

void uni80_startkrets(Uni80 *u, const uint8_t *data, size_t storlek)
{
    if (storlek > UNI80_STARTKRETS)
        storlek = UNI80_STARTKRETS;
    memcpy(u->startkrets, data, storlek);
}

void uni80_karta(Uni80 *u, uint8_t varde)
{
    if (varde < UNI80_CPM || varde > UNI80_START)
        return;
    u->karta = varde;
    u->abc80lage = 0;
    for (int sida = 0; sida < UNI80_SIDOR; sida++) {
        uint16_t adress = (uint16_t)(sida * UNI80_SIDA);
        if (varde == UNI80_CPM)
            u->sidor[sida] = UNI80_RAM;
        else if (varde == UNI80_SYSTEM)
            u->sidor[sida] = UNI80_ABC80;
        else if (adress < UNI80_STARTKRETS)
            u->sidor[sida] = UNI80_KRETS;
        else
            u->sidor[sida] = adress < 0x8000 ? UNI80_RAM : UNI80_ABC80;
    }
}
