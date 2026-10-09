/*
 * bild.h -- bildminnet som punkter, till boken ABC80 inifrån
 *
 * Skärmen ritas i 320 x 240 punkter, 40 x 24 rutor om 8 x 10, med
 * teckensnittet i teckensnitt.h och mosaiken (2 x 3 block) efter en
 * grafikkod, enligt samma regler som tecken.c. Värden visar punkterna,
 * t.ex. på en canvas i webbläsaren (webb/webb.c).
 */

#ifndef BILD_H
#define BILD_H

#include <stdint.h>

#define BILD_BREDD 320
#define BILD_HOJD  240

/* Rita bildminnet (1 KB, från $7C00) i punkter (BILD_BREDD * BILD_HOJD,
 * en rad i taget uppifrån). Färgerna är värdens, t.ex. 0xAABBGGRR för
 * en canvas. Med markor == 0 visas markören (bit 7) inte; värden låter
 * den blinka genom att växla. */
void bild_rita(const uint8_t *bildminne, uint32_t *punkter,
               uint32_t forgrund, uint32_t bakgrund, int markor);

#endif
