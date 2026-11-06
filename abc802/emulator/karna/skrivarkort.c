/*
 * skrivarkort.c -- skrivarkortet på ABC-bussen, till ABC 802 inifrån
 *
 * Se skrivarkort.h.
 */

#include <string.h>

#include "skrivarkort.h"

static int kort_svarar(void *data, int nummer)
{
    (void)data;
    return nummer == SKRIVARKORT_NUMMER;
}

static uint8_t kort_in(void *data, int port)
{
    Skrivarkort *s = data;
    return port == 1 ? s->status : 0xFF;
}

static void kort_ut(void *data, int port, uint8_t varde)
{
    Skrivarkort *s = data;
    if (port != 0)
        return;
    s->antal++;
    if (s->tecken)
        s->tecken(s->data, varde);
}

void skrivarkort_starta(Skrivarkort *s)
{
    memset(s, 0, sizeof *s);
    s->kort = (Kort){ s, kort_svarar, NULL, kort_in, kort_ut, 0 };
    s->status = SKRIVARKORT_KLAR;
}
