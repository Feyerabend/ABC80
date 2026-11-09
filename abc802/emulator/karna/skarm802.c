/*
 * skarm802.c -- bilden och texten på ABC 802:s skärm, till ABC 802 inifrån
 *
 * 6845 räknar fram adresserna i bildminnet: rad r, kolumn k ligger på
 * (R12 R13) + r x R1 + k, i de 2 KB som finns. Teckengeneratorn
 * (6490191-01, 4 KB) har 16 linjer per tecken och två uppsättningar om
 * 128 tecken: text på $000-$7FF och grafik på $800-$FFF, där $20-$3F och
 * $60-$7F är mosaik med 2 x 3 punkter och $40-$5F är bokstäver.
 * Bildminnets bit 7 inverterar tecknet; bit 0-6 väljer det.
 *
 * Varje linje i teckengeneratorn har punkterna i bit 0-5 (bit 5 till
 * vänster). Är bit 7 satt är tecknet en kod som inte syns (ATE): bit 6
 * är värdet och bit 0-1 vad det gäller, till slutet av raden:
 *     0 grafik ($11-$17 på, $01-$07 av)
 *     1 blink  ($08 på, $09 av)
 *     2 dold   ($18 på)
 * Varje rad börjar med text, utan blink och synlig. Linje 15 är
 * markören, och linje 14 (tom) visas för blinkande tecken i
 * blinkfasen och för dolda tecken. Detta står i PAL-kretsen ABCP2-1
 * (PAL16R4, 64 90192-01); här är det läst ur teckengeneratorn och MAME:s
 * beskrivning av PAL:en.
 *
 * Med RTS på DART B (MUX 80/40) visas bara de jämna adresserna, var
 * och en dubbelt så bred: 40 tecken per rad.
 *
 * Markören: 6845:s R10 bit 5-6 (00 fast, 01 ingen, 10 och 11 blinkar
 * med 1/16 och 1/32 av bildtakten) och linjerna R10-R11 (bit 0-4); på de
 * linjerna visas linje 15 i stället.
 */

#include <string.h>

#include "abc802.h"

/* Blinkfasen för tecken: 32 bilder (?). */
#define BLINK_BILDER 32

/* T-cykler per bild: (R0 + 1) teckentider à 1,5 T per linje,
 * (R4 + 1) x (R9 + 1) + R5 linjer. */
static long long bildtid(const Crtc *c)
{
    long linjer = (c->r[4] + 1L) * (c->r[9] + 1L) + c->r[5];
    long long t = (c->r[0] + 1LL) * linjer * 3 / 2;
    return t > 0 ? t : ABC802_KLOCKA / 50;
}

static void bilden(const Abc802 *m, uint8_t *punkter, int farg)
{
    const Crtc *c = &m->crtc;
    memset(punkter, 0, ABC802_BREDD * ABC802_HOJD);
    int kolumner = c->r[1] > 80 ? 80 : c->r[1];
    int linjer = c->r[9] + 1;
    int rader = c->r[6];
    if (linjer > 16)
        linjer = 16;
    if (rader * linjer > ABC802_HOJD)
        rader = ABC802_HOJD / linjer;
    int start = c->r[12] << 8 | c->r[13];
    int markor = (c->r[14] << 8 | c->r[15]) & 0x3FFF;
    long long bild = m->tid / bildtid(c);
    int markorlage = c->r[10] >> 5 & 3;
    int markor_syns = markorlage == 0
                   || (markorlage == 2 && (bild & 8))
                   || (markorlage == 3 && (bild & 16));
    int blinkfas = (int)(bild / (BLINK_BILDER / 2) & 1);
    int fyrtio = abc802_fyrtio(m);

    for (int rad = 0; rad < rader; rad++)
        for (int linje = 0; linje < linjer; linje++) {
            uint8_t *ut = punkter + (rad * linjer + linje) * ABC802_BREDD;
            int grafik = 0, blink = 0, dold = 0;
            int forg = 7, bakgrund = 0;      /* bara med farg: vit på svart */
            for (int k = 0; k < kolumner; k += fyrtio ? 2 : 1) {
                int adress = (start + rad * c->r[1] + k) & 0x3FFF;
                uint8_t kod = m->bildminne[adress & 0x7FF];
                int l = linje;
                if (markor_syns && adress == markor
                    && linje >= (c->r[10] & 0x1F) && linje <= (c->r[11] & 0x1F))
                    l = 15;
                if ((blink && blinkfas) || dold)
                    l = 14;
                uint8_t data = m->teckenrom[(grafik << 11) | (kod & 0x7F) << 4 | l];
                if (farg) {
                    /* 28 och 29 är tomma tecken i teckengeneratorn, inga
                     * koder; som i text-tv gäller de från sin egen plats. */
                    if ((kod & 0x7F) == 28)
                        bakgrund = 0;
                    else if ((kod & 0x7F) == 29)
                        bakgrund = forg;
                }
                if (data & 0x80) {           /* en kod som inte syns */
                    int varde = (data & 0x40) != 0;
                    switch (data & 3) {
                    case 0: grafik = varde; break;
                    case 1: blink = varde; break;
                    case 2: dold = varde; break;
                    }
                    if (farg) {
                        /* Som i ABC 800 C (text-tv): 1-7 och 17-23 är
                         * förgrunden från nästa plats. */
                        int f = kod & 0x7F;
                        for (int p = 0; p < (fyrtio ? 12 : 6); p++)
                            ut[k * 6 + p] = (uint8_t)bakgrund;
                        if ((f >= 1 && f <= 7) || (f >= 17 && f <= 23))
                            forg = f & 7;
                    }
                    continue;
                }
                int inv = kod >> 7;
                for (int p = 0; p < 6; p++) {
                    uint8_t tand = (uint8_t)((data >> (5 - p) & 1) ^ inv);
                    if (farg)
                        tand = (uint8_t)(tand ? forg : bakgrund);
                    if (fyrtio) {
                        ut[k * 6 + 2 * p] = tand;
                        ut[k * 6 + 2 * p + 1] = tand;
                    } else
                        ut[k * 6 + p] = tand;
                }
            }
        }
}

void abc802_bild(const Abc802 *m, uint8_t *punkter)
{
    bilden(m, punkter, 0);
}

void abc802_fargbild(const Abc802 *m, uint8_t *punkter)
{
    bilden(m, punkter, 1);
}

/* ---------------------------------------------------------------- */
/* Texten                                                            */

static const char *const svenska_tecken[128] = {
    [0x24] = "¤", [0x40] = "É", [0x5B] = "Ä", [0x5C] = "Ö", [0x5D] = "Å",
    [0x5E] = "Ü", [0x60] = "é", [0x7B] = "ä", [0x7C] = "ö", [0x7D] = "å",
    [0x7E] = "ü",
};

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

/* Mosaiken i grafiktecknet, läst ur teckengeneratorn: sex punkter, bit
 * 0 överst till vänster och bit 5 nederst till höger, som Unicode. */
static unsigned mosaik(const uint8_t *tecken)
{
    static const int linje[3] = { 1, 5, 8 };
    int punkter = 0;
    for (int r = 0; r < 3; r++) {
        if (tecken[linje[r]] & 0x38)
            punkter |= 1 << (2 * r);
        if (tecken[linje[r]] & 0x07)
            punkter |= 2 << (2 * r);
    }
    switch (punkter) {
    case 0:  return ' ';
    case 21: return 0x258C;                  /* vänster halva */
    case 42: return 0x2590;                  /* höger halva */
    case 63: return 0x2588;                  /* hel ruta */
    }
    return 0x1FB00 + punkter - 1 - (punkter > 21) - (punkter > 42);
}

int abc802_rader(const Abc802 *m)
{
    int r = m->crtc.r[6];
    return r < 1 ? 24 : r > ABC802_HOJD / 10 ? ABC802_HOJD / 10 : r;
}

void abc802_textrad(const Abc802 *m, int rad, char *ut)
{
    const Crtc *c = &m->crtc;
    int fyrtio = abc802_fyrtio(m);
    int start = c->r[12] << 8 | c->r[13];
    int kolumner = c->r[1] > 80 || !c->r[1] ? 80 : c->r[1];
    int grafik = 0;
    char *p = ut, *slut = ut;
    for (int k = 0; k < kolumner; k += fyrtio ? 2 : 1) {
        uint8_t kod = m->bildminne[(start + rad * kolumner + k) & 0x7FF] & 0x7F;
        const uint8_t *tecken = m->teckenrom + ((grafik << 11) | kod << 4);
        if (tecken[0] & 0x80) {
            if ((tecken[0] & 3) == 0)
                grafik = (tecken[0] & 0x40) != 0;
            *p++ = ' ';
            continue;
        }
        if (grafik && (kod & 0x20))
            p += utf8(mosaik(tecken), p);
        else if (svenska_tecken[kod])
            p += (int)strlen(strcpy(p, svenska_tecken[kod]));
        else
            *p++ = kod >= 0x20 && kod < 0x7F ? (char)kod : ' ';
        if (p[-1] != ' ')
            slut = p;
    }
    *slut = 0;
}
