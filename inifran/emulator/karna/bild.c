/*
 * bild.c -- bildminnet som punkter, till boken ABC80 inifrån
 *
 * Varje ruta är 8 x 10 punkter. Ett tecken är sju rader om fem punkter
 * (bit 6-2 i teckensnittet), med två rader under för g, j, p, q och y.
 * En mosaikruta delas i två kolumner om fyra punkter och tre rader om
 * 3, 3 och 4 punkter; bit 0 är blocket överst till vänster och bit 5
 * nederst till höger, som i tecken.c. Grafiken slås på av koderna
 * $11-$17 och av av $01-$07, från tecknet efter; varje rad börjar med
 * text. I grafikläget är $20-$3F och $60-$7F mosaik, och $40-$5F
 * (versalerna) visas som text. Koder under $20 och $7F är tomma rutor.
 * Markören (bit 7) visar tecknet omväxlande som vanligt och inverterat,
 * men inte när koden är under $20 (Isaksson och Kärrsgård 1980,
 * s. 72-73); program som Genesis-demot har grafikkoden med bit 7 satt.
 */

#include "bild.h"
#include "teckensnitt.h"

/* Mosaikens rader: var de börjar och slutar i rutan. */
static const int mosaik_rad[4] = { 0, 3, 6, 10 };

void bild_rita(const uint8_t *bildminne, uint32_t *punkter,
               uint32_t forgrund, uint32_t bakgrund, int markor)
{
    for (int rad = 0; rad < 24; rad++) {
        const uint8_t *p = bildminne + (rad & 7) * 0x80 + (rad >> 3) * 40;
        int grafik = 0;
        for (int kolumn = 0; kolumn < 40; kolumn++) {
            uint8_t kod = p[kolumn] & 0x7F;
            int omvand = markor && (p[kolumn] & 0x80) && kod >= 0x20;
            uint32_t fg = omvand ? bakgrund : forgrund;
            uint32_t bg = omvand ? forgrund : bakgrund;

            /* Rutans tio rader som bitar, bit 7 till vänster. */
            uint8_t ruta[10] = { 0 };
            if (kod < 0x20 || kod == 0x7F)
                ;
            else if (grafik && (kod < 0x40 || kod >= 0x60)) {
                int punkt = (kod & 0x1F) | (kod & 0x40) >> 1;
                for (int r = 0; r < 3; r++)
                    for (int y = mosaik_rad[r]; y < mosaik_rad[r + 1]; y++)
                        ruta[y] = (uint8_t)((punkt >> 2 * r & 1 ? 0xF0 : 0) |
                                            (punkt >> 2 * r & 2 ? 0x0F : 0));
            } else {
                for (int y = 0; y < 7; y++)
                    ruta[y] = teckensnitt[kod - 0x20][y];
                ruta[7] = teckensnitt_under[kod][0];
                ruta[8] = teckensnitt_under[kod][1];
            }
            if (kod >= 0x11 && kod <= 0x17)
                grafik = 1;
            if (kod >= 0x01 && kod <= 0x07)
                grafik = 0;

            uint32_t *ut = punkter + rad * 10 * BILD_BREDD + kolumn * 8;
            for (int y = 0; y < 10; y++, ut += BILD_BREDD)
                for (int x = 0; x < 8; x++)
                    ut[x] = ruta[y] & 0x80 >> x ? fg : bg;
        }
    }
}
