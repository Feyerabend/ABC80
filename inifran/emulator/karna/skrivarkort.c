/*
 * skrivarkort.c -- skrivarna, till boken ABC80 inifrån
 *
 * Se skrivarkort.h.
 */

#include <string.h>

#include "skrivarkort.h"

/* ---------------------------------------------------------------- */
/* Skrivarkortet, kort 60                                            */

static int kort_svarar(void *data, int nummer)
{
    (void)data;
    return nummer == 60;
}

static uint8_t kort_in(void *data, int port)
{
    Skrivarkort *s = data;
    if (port != 1)
        return 0xFF;
    if (s->antal_upptagen > 0) {
        s->antal_upptagen--;
        return s->status_upptagen;
    }
    return s->status;
}

static void kort_ut(void *data, int port, uint8_t varde)
{
    Skrivarkort *s = data;
    if (port == 0 && s->antal < SKRIVARE_BUFFERT)
        s->utskrift[s->antal++] = varde;
}

void skrivarkort_starta(Skrivarkort *s, uint8_t status, uint8_t status_upptagen,
                        int antal_upptagen)
{
    memset(s, 0, sizeof *s);
    s->kort = (Kort){ s, kort_svarar, NULL, kort_in, kort_ut, 0 };
    s->status = status;
    s->status_upptagen = status_upptagen;
    s->antal_upptagen = antal_upptagen;
}

/* ---------------------------------------------------------------- */
/* V24-skrivaren på PIO B                                            */

static uint8_t v24_las(void *data, uint8_t senast_skrivet, int rxd)
{
    (void)data;
    return (uint8_t)((senast_skrivet & 0x78) | 0x84 | rxd);
}

static void v24_skriv(void *data, uint8_t varde)
{
    V24Skrivare *s = data;
    if (!(varde & 0x10)) {                   /* en bit på TxD */
        if (s->antal_bitar < 16)
            s->bitar[s->antal_bitar++] = varde >> 3 & 1;
        return;
    }
    /* Tecknet är slut: vila, startbit (0), 8 data, stoppbit. */
    if (s->antal_bitar >= 11 && s->bitar[1] == 0 && s->antal < SKRIVARE_BUFFERT) {
        int tecken = 0;
        for (int k = 0; k < 8; k++)
            tecken |= s->bitar[2 + k] << k;
        s->utskrift[s->antal++] = (uint8_t)tecken;
    }
    s->antal_bitar = 0;
}

void v24_skrivare_starta(V24Skrivare *s)
{
    memset(s, 0, sizeof *s);
    s->enhet = (PioBEnhet){ s, v24_las, v24_skriv };
}
