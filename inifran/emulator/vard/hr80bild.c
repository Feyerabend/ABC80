/*
 * hr80bild.c -- grafikkortets bild till en PNG-fil, till ABC80 inifrån
 *
 * Utanför boken; se hr80bild.h.
 */

#include <string.h>

#include "hr80bild.h"
#include "png.h"

/* Svart, röd, grön, gul, blå, magenta, cyan, vit: R, G och B av och på
   som på ABC 810. */
static const uint32_t farger[8] = {
    0x000000, 0xFF0000, 0x00FF00, 0xFFFF00, 0x0000FF, 0xFF00FF, 0x00FFFF, 0xFFFFFF
};

int hr80_bild_skriv(const Hr80 *h, const char *fil, int markor_synlig)
{
    static uint32_t punkter[HR80_BREDD * HR80_HOJD], bild[HR80_BREDD * 2 * HR80_HOJD];
    hr80_rita(h, punkter, farger, markor_synlig);
    for (int y = 0; y < 2 * HR80_HOJD; y++)
        memcpy(bild + y * HR80_BREDD, punkter + y / 2 * HR80_BREDD, sizeof punkter[0] * HR80_BREDD);
    return png_skriv_farg(fil, bild, HR80_BREDD, 2 * HR80_HOJD);
}
