/*
 * bild.h -- bildminnet som punkter, till boken ABC80 inifrån
 *
 * Skärmen ritas i 320 x 240 punkter, 40 x 24 rutor om 8 x 10, med
 * teckensnittet i teckensnitt.h och mosaiken (2 x 3 block) efter en
 * grafikkod, enligt samma regler som tecken.c. Värden visar punkterna,
 * t.ex. på en canvas i webbläsaren (webb/webb.c). Med TKN80 i
 * 80-teckensläget blir det 640 x 240 punkter, 80 x 24 rutor; punkterna
 * är då hälften så breda på skärmen.
 */

#ifndef BILD_H
#define BILD_H

#include <stdint.h>

#define BILD_BREDD 320                       /* 40 tecken; 80 ger dubbelt */
#define BILD_HOJD  240

/* Rita bildminnet (1 KB från $7C00 med kolumner = 40, 2 KB från $5800
 * med 80) i punkter (kolumner * 8 * BILD_HOJD, en rad i taget
 * uppifrån). Färgerna är värdens, t.ex. 0xAABBGGRR för en canvas. Med
 * markor == 0 visas markören (bit 7) inte; värden låter den blinka
 * genom att växla. */
void bild_rita(const uint8_t *bildminne, int kolumner, uint32_t *punkter,
               uint32_t forgrund, uint32_t bakgrund, int markor);

#endif
