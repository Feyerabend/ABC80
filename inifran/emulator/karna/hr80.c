/*
 * hr80.c -- ett grafikkort på V24 i HR-80:s anda, till ABC80 inifrån
 *
 * Utanför boken; se hr80.h. Koderna och deras namn är annonsens (Liber
 * 1984, emulator/färg-abc80.pdf):
 *
 *     0 null                  16 semigrafik, sammanhängande (teledata)
 *     1 välj färg             17 semigrafik, åtskild
 *     2 sätt punkt (x, y)     18 markören (y+32, x+32)
 *     4 linje (x, y)          19 absolut adressering (y, x)
 *     5 fyll (x, y)           20 förgrundsfärg   21 bakgrundsfärg
 *     8 vänster  9 höger      22 standard        24 rensa raden
 *    10 ned     11 upp        25 markören relativt
 *    12 rensa skärmen         27 ESC
 *    13 vagnretur             28 fast punkt (x, y)
 *    14 markören på           29 punkt relativt (x, y)
 *    15 markören av           30 linje relativt  31 fyll relativt
 *    32-127 tecken            128-253 kör ett makro
 *
 *     ESC 61 markören (rad, kolumn)    ESC 48-51 färgekvivalens
 *     ESC 73 initiera om               ESC 52 40 tecken (färg)
 *     ESC 82 reset                     ESC 56 80 tecken (svartvitt)
 *     ESC 68 kod 66 (bitmapp) ... 254, ESC 68 kod 82 (instruktioner) ... 254
 *
 * Koderna 3, 6, 7, 23, 26 och 255 gör ingenting. Annonsen nämner också
 * understrykning, dubbel höjd och omvänd video, men inte med vilka
 * koder; de finns inte här.
 *
 * Gissat, eftersom manualen inte är funnen:
 * - Argumenten är hela byte: x och y 0-255, de relativa -128...127
 *   (tvåkomplement); bara kod 18 har +32 i annonsen, och ESC 61 får det
 *   också. y = 0 är nederst, som i ett diagram. I det svartvita läget är
 *   grafikens x dubblat (0-255 över 512 punkter); texten använder alla.
 * - Färgerna är ABC 800 C:s. Bildskärmen ABC 810 får signalerna R, G och
 *   B av och på (ABC 800, bruksanvisningen 2.3), så de åtta är 0 svart,
 *   1 röd, 2 grön, 3 gul, 4 blå, 5 magenta, 6 cyan, 7 vit, med R som bit
 *   0, G bit 1 och B bit 2; det är också text-tv:ns och ABC 800:s
 *   färgkoder för text (BASIC II-manualen 11.1). Grafiken där visar fyra
 *   av dem, valda med FGCTL ur en färgvalstabell (manualen 12.4,
 *   s. 86-88). ESC 48-51 gissas sätta färg 0-3 var för sig, så att varje
 *   rad i tabellen går att få, också de med två lika färger som
 *   animationen använder (12.3). Från början 0 svart, 1 röd, 2 grön,
 *   3 vit, som FGCTL 6 (134). Ritfärg och förgrund 3, bakgrund 0.
 * - Texten: rutor om 6 x 10 punkter med ABC80:s teckensnitt, 40 eller 80
 *   per rad och 24 rader, mitt på bilden. 19 sätter rutans övre vänstra
 *   hörn i punkten (x, y), så att text kan stå var som helst. 25 flyttar
 *   (rader, kolumner). Längst ned rullar hela bilden upp en rad.
 * - 16 och 17 ritar 32-63 och 96-127 som text-tv:ns mosaik (2 x 3 block)
 *   i förgrundsfärgen, tills 22 ("standard"), som också återställer
 *   färgerna.
 * - Fyllningen byter den sammanhängande ytan med startpunktens färg mot
 *   ritfärgen.
 * - ESC 73 går till 40 tecken, standardfärgerna och en ren skärm; makrona
 *   är kvar. ESC R tar bort dem också.
 * - Makron: 254 avslutar alltid en definition. En bitmapp börjar med
 *   bredden, sedan raderna uppifrån med 7 punkter per byte (bit 6 till
 *   vänster, så att 254 aldrig förekommer); den ritas i ritfärgen med
 *   övre vänstra hörnet i den fasta punkten. Ett makro med instruktioner
 *   tolkas som om tecknen kom på V24. Makrominnet frigörs vid ESC R.
 */

#include <string.h>

#include "hr80.h"
#include "teckensnitt.h"

static const uint8_t standardpalett[4] = { 0, 1, 2, 7 };   /* FGCTL 6 */

static int bredd(const Hr80 *h) { return h->svartvit ? 512 : 256; }
static int kolumner(const Hr80 *h) { return h->svartvit ? 80 : 40; }
static int text_x0(const Hr80 *h) { return (bredd(h) - kolumner(h) * 6) / 2; }
#define TEXT_Y0 8

/* Grafikens koordinater (y uppåt) till bildminnets (y nedåt). */
static int gx(const Hr80 *h, int x) { return h->svartvit ? 2 * x : x; }
static int gy(int y) { return 255 - y; }

static int las_punkt(const Hr80 *h, int x, int y)
{
    if (h->svartvit)
        return h->minne[y * 64 + x / 8] >> (7 - x % 8) & 1;
    return h->minne[y * 64 + x / 4] >> (6 - 2 * (x % 4)) & 3;
}

static void satt_punkt(Hr80 *h, int x, int y, int farg)
{
    if (x < 0 || y < 0 || x >= bredd(h) || y >= 256)
        return;
    if (h->svartvit) {
        uint8_t *p = &h->minne[y * 64 + x / 8], bit = (uint8_t)(0x80 >> x % 8);
        *p = farg & 1 ? *p | bit : *p & ~bit;
    } else {
        uint8_t *p = &h->minne[y * 64 + x / 4];
        int skift = 6 - 2 * (x % 4);
        *p = (uint8_t)((*p & ~(3 << skift)) | (farg & 3) << skift);
    }
}

/* En byte där alla punkterna har färgen. */
static uint8_t fylld_byte(const Hr80 *h, int farg)
{
    if (h->svartvit)
        return farg & 1 ? 0xFF : 0x00;
    return (uint8_t)((farg & 3) * 0x55);
}

static void rensa(Hr80 *h)
{
    memset(h->minne, fylld_byte(h, h->bakgrund), HR80_MINNE);
    h->tx = text_x0(h);
    h->ty = TEXT_Y0;
}

static void linje(Hr80 *h, int x0, int y0, int x1, int y1)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int fel = dx + dy;
    for (;;) {
        satt_punkt(h, x0, y0, h->farg);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * fel;
        if (e2 >= dy) { fel += dy; x0 += sx; }
        if (e2 <= dx) { fel += dx; y0 += sy; }
    }
}

/* Fyllningen rad för rad med en egen stack. Blir den full tappas
 * resten av ytan; 4 096 sträckor räcker för vanliga figurer. */
#define FYLLSTACK 4096
static struct { int16_t x, y; } fyllstack[FYLLSTACK];

static void fyll(Hr80 *h, int x, int y)
{
    int b = bredd(h);
    if (x < 0 || y < 0 || x >= b || y >= 256)
        return;
    int gammal = las_punkt(h, x, y), ny = h->svartvit ? h->farg & 1 : h->farg & 3;
    if (gammal == ny)
        return;
    int n = 0;
    fyllstack[n].x = (int16_t)x; fyllstack[n].y = (int16_t)y; n++;
    while (n > 0) {
        n--;
        x = fyllstack[n].x; y = fyllstack[n].y;
        if (las_punkt(h, x, y) != gammal)
            continue;
        int v = x, hg = x;
        while (v > 0 && las_punkt(h, v - 1, y) == gammal)
            v--;
        while (hg < b - 1 && las_punkt(h, hg + 1, y) == gammal)
            hg++;
        for (int i = v; i <= hg; i++)
            satt_punkt(h, i, y, ny);
        for (int r = y - 1; r <= y + 1; r += 2) {
            if (r < 0 || r >= 256)
                continue;
            int i = v;
            while (i <= hg) {
                if (las_punkt(h, i, r) == gammal && n < FYLLSTACK) {
                    fyllstack[n].x = (int16_t)i; fyllstack[n].y = (int16_t)r; n++;
                    while (i <= hg && las_punkt(h, i, r) == gammal)
                        i++;
                } else
                    i++;
            }
        }
    }
}

/* Hela bilden upp en textrad; den nya raden längst ned i bakgrunden. */
static void rulla(Hr80 *h)
{
    memmove(h->minne, h->minne + 10 * 64, (256 - 10) * 64);
    memset(h->minne + (256 - 10) * 64, fylld_byte(h, h->bakgrund), 10 * 64);
    h->ty -= 10;
}

static void ny_rad(Hr80 *h)
{
    h->tx = text_x0(h);
    h->ty += 10;
    while (h->ty > 256 - 10)
        rulla(h);
}

/* Ett tecken (32-127) i rutan vid markören, sedan ett steg åt höger. */
static void rita_tecken(Hr80 *h, uint8_t kod)
{
    uint8_t ruta[10] = { 0 };
    if (h->semigrafik && (kod < 0x40 || kod >= 0x60)) {
        static const int rad_start[4] = { 0, 3, 7, 10 };
        int block = (kod & 0x1F) | (kod & 0x40) >> 1;
        for (int r = 0; r < 3; r++)
            for (int y = rad_start[r]; y < rad_start[r + 1]; y++) {
                uint8_t v = block >> 2 * r & 1 ? 0xE0 : 0, hg = block >> 2 * r & 2 ? 0x1C : 0;
                if (h->semigrafik == 2) {          /* åtskild: luft till höger och under */
                    v &= 0xC0; hg &= 0x18;
                    if (y == rad_start[r + 1] - 1)
                        v = hg = 0;
                }
                ruta[y] = v | hg;
            }
    } else if (kod >= 0x20 && kod < 0x7F) {
        for (int y = 0; y < 7; y++)
            ruta[y] = teckensnitt[kod - 0x20][y];
        ruta[7] = teckensnitt_under[kod][0];
        ruta[8] = teckensnitt_under[kod][1];
    }
    for (int y = 0; y < 10; y++)
        for (int x = 0; x < 6; x++)
            satt_punkt(h, h->tx + x, h->ty + y,
                       ruta[y] & 0x80 >> x ? h->forgrund : h->bakgrund);
    h->tx += 6;
    if (h->tx > text_x0(h) + (kolumner(h) - 1) * 6)
        ny_rad(h);
}

/* En bitmapp ur ett makro: bredden, sedan 7 punkter per byte. */
static void rita_bitmapp(Hr80 *h, const uint8_t *data, int langd)
{
    if (langd < 1 || data[0] == 0)
        return;
    int b = data[0], per_rad = (b + 6) / 7;
    int x0 = gx(h, h->px), y0 = gy(h->py);
    for (int i = 1, rad = 0; i + per_rad <= langd; i += per_rad, rad++)
        for (int x = 0; x < b; x++)
            if (data[i + x / 7] & 0x40 >> x % 7) {
                satt_punkt(h, x0 + (h->svartvit ? 2 * x : x), y0 + rad, h->farg);
                if (h->svartvit)
                    satt_punkt(h, x0 + 2 * x + 1, y0 + rad, h->farg);
            }
}

static void standard(Hr80 *h)
{
    h->semigrafik = 0;
    h->farg = h->forgrund = 3;
    h->bakgrund = 0;
}

static void initiera(Hr80 *h)
{
    h->svartvit = 0;
    memcpy(h->palett, standardpalett, 4);
    standard(h);
    h->markor = 1;
    h->px = h->py = 0;
    h->kommando = 0;
    h->esc = 0;
    h->behover = h->antal_argument = 0;
    rensa(h);
}

void hr80_starta(Hr80 *h)
{
    memset(h, 0, sizeof *h);
    h->definierar = -1;
    initiera(h);
}

static int s8(uint8_t v) { return v < 128 ? v : v - 256; }

/* Kommandot med alla sina argument. */
static void utfor(Hr80 *h)
{
    const uint8_t *a = h->argument;
    switch (h->kommando) {
    case 1:  h->farg = a[0]; break;
    case 2:  h->px = a[0]; h->py = a[1];
             satt_punkt(h, gx(h, h->px), gy(h->py), h->farg); break;
    case 4:  linje(h, gx(h, h->px), gy(h->py), gx(h, a[0]), gy(a[1]));
             h->px = a[0]; h->py = a[1]; break;
    case 5:  fyll(h, gx(h, a[0]), gy(a[1])); break;
    case 18:
    case 256 + 61:
             h->ty = TEXT_Y0 + (a[0] - 32) * 10;
             h->tx = text_x0(h) + (a[1] - 32) * 6; break;
    case 19: h->ty = gy(a[0]); h->tx = gx(h, a[1]); break;
    case 20: h->forgrund = a[0]; break;
    case 21: h->bakgrund = a[0]; break;
    case 25: h->ty += s8(a[0]) * 10; h->tx += s8(a[1]) * 6; break;
    case 28: h->px = a[0]; h->py = a[1]; break;
    case 29: h->px += s8(a[0]); h->py += s8(a[1]);
             satt_punkt(h, gx(h, h->px), gy(h->py), h->farg); break;
    case 30: {
             int x = h->px + s8(a[0]), y = h->py + s8(a[1]);
             linje(h, gx(h, h->px), gy(h->py), gx(h, x), gy(y));
             h->px = x; h->py = y; break;
             }
    case 31: fyll(h, gx(h, h->px + s8(a[0])), gy(h->py + s8(a[1]))); break;
    case 256 + 48: case 256 + 49: case 256 + 50: case 256 + 51:
             h->palett[h->kommando - 256 - 48] = a[0] & 7; break;
    case 256 + 68: {
             /* Början på en definition; resten tas emot till 254. */
             int nr = a[0] - 128;
             h->definierar = nr >= 0 && nr < 126 ? nr : 126;   /* 126: kastas */
             h->makro_typ = a[1];
             if (h->definierar < 126) {
                 h->makro_start[nr] = h->makro_slut;
                 h->makro_langd[nr] = 0;
                 h->makro_slag[nr] = a[1];
             }
             break;
             }
    }
    h->kommando = 0;
}

/* Hur många argument kommandot tar; 0 för dem som utförs direkt. */
static int argument_till(int kommando)
{
    switch (kommando) {
    case 1: case 20: case 21:
    case 256 + 48: case 256 + 49: case 256 + 50: case 256 + 51:
        return 1;
    case 2: case 4: case 5: case 18: case 19: case 25:
    case 28: case 29: case 30: case 31:
    case 256 + 61: case 256 + 68:
        return 2;
    }
    return 0;
}

static void kor_makro(Hr80 *h, int nr)
{
    if (h->makro_langd[nr] == 0 || h->djup >= 8)
        return;
    const uint8_t *data = h->makrominne + h->makro_start[nr];
    int langd = h->makro_langd[nr];
    if (h->makro_slag[nr] == 66) {
        rita_bitmapp(h, data, langd);
        return;
    }
    h->djup++;
    for (int i = 0; i < langd; i++)
        hr80_tecken(h, data[i]);
    h->djup--;
}

/* ESC och koden efter. */
static void esc(Hr80 *h, uint8_t kod)
{
    switch (kod) {
    case 73: initiera(h); return;
    case 82: hr80_starta(h); return;
    case 52: h->svartvit = 0; rensa(h); return;
    case 56: h->svartvit = 1; rensa(h); return;
    }
    if (argument_till(256 + kod)) {
        h->kommando = 256 + kod;
        h->behover = argument_till(h->kommando);
        h->antal_argument = 0;
    }
}

void hr80_tecken(Hr80 *h, uint8_t tecken)
{
    /* En definition tar emot allt till 254. */
    if (h->definierar >= 0) {
        if (tecken == 254)
            h->definierar = -1;
        else if (h->definierar < 126) {
            if (h->makro_slut < HR80_MAKROMINNE) {
                h->makrominne[h->makro_slut++] = tecken;
                h->makro_langd[h->definierar]++;
            } else {
                h->makro_langd[h->definierar] = 0;     /* fullt: kastas */
                h->definierar = 126;
            }
        }
        return;
    }
    if (h->behover > 0) {
        h->argument[h->antal_argument++] = tecken;
        if (h->antal_argument == h->behover) {
            h->behover = 0;
            utfor(h);
        }
        return;
    }
    if (h->esc) {
        h->esc = 0;
        esc(h, tecken);
        return;
    }
    if (tecken >= 128) {
        if (tecken <= 253)
            kor_makro(h, tecken - 128);
        return;
    }
    if (tecken >= 32) {
        if (tecken < 127)
            rita_tecken(h, tecken);
        return;
    }
    if (argument_till(tecken)) {
        h->kommando = tecken;
        h->behover = argument_till(tecken);
        h->antal_argument = 0;
        return;
    }
    switch (tecken) {
    case 8:
        if (h->tx - 6 >= 0)
            h->tx -= 6;
        break;
    case 9:
        h->tx += 6;
        if (h->tx > text_x0(h) + (kolumner(h) - 1) * 6)
            ny_rad(h);
        break;
    case 10:
        h->ty += 10;
        while (h->ty > 256 - 10)
            rulla(h);
        break;
    case 11:
        if (h->ty - 10 >= 0)
            h->ty -= 10;
        break;
    case 12: rensa(h); break;
    case 13: h->tx = text_x0(h); break;
    case 14: h->markor = 1; break;
    case 15: h->markor = 0; break;
    case 16: h->semigrafik = 1; break;
    case 17: h->semigrafik = 2; break;
    case 22: standard(h); break;
    case 24:
        for (int y = h->ty; y < h->ty + 10; y++)
            for (int x = 0; x < bredd(h); x++)
                satt_punkt(h, x, y, h->bakgrund);
        break;
    case 27: h->esc = 1; break;
    }
}

void hr80_rita(const Hr80 *h, uint32_t *punkter, const uint32_t farger[8],
               int markor_synlig)
{
    for (int y = 0; y < HR80_HOJD; y++)
        for (int x = 0; x < HR80_BREDD; x++)
            punkter[y * HR80_BREDD + x] = h->svartvit
                ? farger[las_punkt(h, x, y) ? 7 : 0]
                : farger[h->palett[las_punkt(h, x / 2, y)]];
    if (!h->markor || !markor_synlig)
        return;
    int y = h->ty + 9, skala = h->svartvit ? 1 : 2;
    if (y < 0 || y >= HR80_HOJD)
        return;
    uint32_t farg = h->svartvit ? farger[h->forgrund & 1 ? 7 : 0]
                                : farger[h->palett[h->forgrund & 3]];
    for (int x = h->tx * skala; x < (h->tx + 6) * skala; x++)
        if (x >= 0 && x < HR80_BREDD)
            punkter[y * HR80_BREDD + x] = farg;
}
