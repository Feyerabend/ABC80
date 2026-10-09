/*
 * tecken.c -- bildminnet som text, till boken ABC80 inifrån
 *
 * Se tecken.h. Bildkretsen visar koderna $00-$1F som mellanslag;
 * $11-$17 slår på grafik från nästa tecken och $01-$07 slår på text,
 * och varje rad börjar med text. I grafikläget är $20-$3F och $60-$7F
 * 2 x 3 punkter (bit 0-4 och 6) och $40-$5F vanliga tecken.
 */

#include <string.h>

#include "tecken.h"

/* Tecknen som inte är ASCII. */
static const char *const svenska_tecken[128] = {
    [0x24] = "¤", [0x40] = "É", [0x5B] = "Ä", [0x5C] = "Ö", [0x5D] = "Å",
    [0x5E] = "Ü", [0x60] = "é", [0x7B] = "ä", [0x7C] = "ö", [0x7D] = "å",
    [0x7E] = "ü",
};

/* Ett Unicodetecken som UTF-8; antalet bytes. */
static int utf8(unsigned kod, char *ut)
{
    if (kod < 0x80) {
        ut[0] = (char)kod;
        return 1;
    }
    if (kod < 0x800) {
        ut[0] = (char)(0xC0 | kod >> 6);
        ut[1] = (char)(0x80 | (kod & 0x3F));
        return 2;
    }
    if (kod < 0x10000) {
        ut[0] = (char)(0xE0 | kod >> 12);
        ut[1] = (char)(0x80 | (kod >> 6 & 0x3F));
        ut[2] = (char)(0x80 | (kod & 0x3F));
        return 3;
    }
    ut[0] = (char)(0xF0 | kod >> 18);
    ut[1] = (char)(0x80 | (kod >> 12 & 0x3F));
    ut[2] = (char)(0x80 | (kod >> 6 & 0x3F));
    ut[3] = (char)(0x80 | (kod & 0x3F));
    return 4;
}

/* Sex punkter (bit 0 överst till vänster, bit 5 nederst till höger)
 * som Unicode. Blocket U+1FB00 saknar tom ruta, vänster halva, höger
 * halva och hel ruta, som redan finns i andra block. */
static unsigned mosaik(int punkter)
{
    switch (punkter) {
    case 0:  return ' ';
    case 21: return 0x258C;                  /* vänster halva */
    case 42: return 0x2590;                  /* höger halva */
    case 63: return 0x2588;                  /* hel ruta */
    }
    return 0x1FB00 + punkter - 1 - (punkter > 21) - (punkter > 42);
}

void tecken_rad(const uint8_t *bildminne, int rad, char *ut,
                const char *fore, const char *efter)
{
    const uint8_t *p = bildminne + (rad & 7) * 0x80 + (rad >> 3) * TECKEN_KOLUMNER;
    int grafik = 0;
    for (int k = 0; k < TECKEN_KOLUMNER; k++) {
        uint8_t kod = p[k] & 0x7F;
        int markor = p[k] & 0x80 && kod >= 0x20;  /* under $20 syns den inte */
        char tecknet[8];
        int n;
        if (kod < 0x20 || kod == 0x7F)
            n = utf8(' ', tecknet);
        else if (grafik && (kod < 0x40 || kod >= 0x60))
            n = utf8(mosaik((kod & 0x1F) | (kod & 0x40) >> 1), tecknet);
        else if (svenska_tecken[kod]) {
            n = (int)strlen(svenska_tecken[kod]);
            memcpy(tecknet, svenska_tecken[kod], (size_t)n);
        } else
            n = utf8(kod, tecknet);
        if (kod >= 0x11 && kod <= 0x17)
            grafik = 1;
        if (kod >= 0x01 && kod <= 0x07)
            grafik = 0;

        if (markor && !fore) {
            *ut++ = '_';
            continue;
        }
        if (markor)
            ut += strlen(strcpy(ut, fore));
        memcpy(ut, tecknet, (size_t)n);
        ut += n;
        if (markor)
            ut += strlen(strcpy(ut, efter));
    }
    *ut = 0;
}

void tecken_rad_kortad(const uint8_t *bildminne, int rad, char *ut)
{
    tecken_rad(bildminne, rad, ut, NULL, NULL);
    size_t n = strlen(ut);
    while (n > 0 && ut[n - 1] == ' ')
        ut[--n] = 0;
}

uint8_t tecken_svensk(uint8_t andra_byten)
{
    switch (andra_byten) {
    case 0x85: return 0x5D;                  /* Å */
    case 0x84: return 0x5B;                  /* Ä */
    case 0x96: return 0x5C;                  /* Ö */
    case 0x89: return 0x40;                  /* É */
    case 0x9C: return 0x5E;                  /* Ü */
    case 0xA5: return 0x7D;                  /* å */
    case 0xA4: return 0x7B;                  /* ä */
    case 0xB6: return 0x7C;                  /* ö */
    case 0xA9: return 0x60;                  /* é */
    case 0xBC: return 0x7E;                  /* ü */
    }
    return 0;
}
