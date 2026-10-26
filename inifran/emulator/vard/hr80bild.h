/*
 * hr80bild.h -- grafikkortets bild till en PNG-fil, till ABC80 inifrån
 *
 * Utanför boken; se karna/hr80.h.
 */

#ifndef HR80BILD_H
#define HR80BILD_H

#include "../karna/hr80.h"

/* 512 x 512 punkter (varje rad två gånger, så att bilden får ungefär
 * rätt form), i ABC 810:s åtta färger. Ger 0 om filen inte gick att
 * skriva. */
int hr80_bild_skriv(const Hr80 *h, const char *fil, int markor_synlig);

#endif
