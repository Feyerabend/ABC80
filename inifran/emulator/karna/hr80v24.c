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
        uint8_t svar[4];
        if (hr80_svar(&h->kort, svar) && h->maskin) {
            v24_ny(h->maskin);
            h->maskin->baud = 9600;          /* kortets hastighet */
            for (int i = 0; i < 4; i++)
                v24_lagg(h->maskin, svar[i]);
            v24_starta(h->maskin, h->maskin->tid + MASKIN_KLOCKA / 1000);
        }
    }
}

void hr80_v24_starta(Hr80V24 *h, Maskin *m)
{
    memset(h, 0, sizeof *h);
    h->maskin = m;
    hr80_starta(&h->kort);
    h->enhet = (PioBEnhet){ h, v24_las_klar, hr80_v24_skriv };
}
