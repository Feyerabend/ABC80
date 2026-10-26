/*
 * hr80v24.c -- grafikkortet i HR-80:s anda på V24, till ABC80 inifrån
 *
 * Utanför boken; se hr80v24.h.
 */

#include <string.h>

#include "hr80v24.h"

static void hr80_v24_skriv(void *data, uint8_t varde)
{
    Hr80V24 *h = data;
    int tecken = v24_avkoda(&h->avkodare, varde);
    if (tecken >= 0) {
        hr80_tecken(&h->kort, (uint8_t)tecken);
        h->antal++;
    }
}

void hr80_v24_starta(Hr80V24 *h)
{
    memset(h, 0, sizeof *h);
    hr80_starta(&h->kort);
    h->enhet = (PioBEnhet){ h, v24_las_klar, hr80_v24_skriv };
}
