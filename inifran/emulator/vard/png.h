/*
 * png.h -- en bild till en PNG-fil, till boken ABC80 inifrån
 *
 * Se png.c.
 */

#ifndef PNG_H
#define PNG_H

#include <stdint.h>

/* bild har bredd * hojd bytes, 1 = svart. Ger 0 om filen inte gick att skriva. */
int png_skriv(const char *fil, const uint8_t *bild, int bredd, int hojd);

#endif
