/*
 * webb.c -- värden i webbläsaren, till ABC 802 inifrån
 *
 * Kärnan byggs till WebAssembly (se Makefile, make webb) och styrs av
 * abc802.js i sidan, som för ABC80 (abc80/emulator/webb). Här finns de
 * funktioner som sidan anropar: den startar maskinen med ROM:en och
 * teckengeneratorn, kör en bild (20 ms) i taget och skriver tangenter.
 * Filerna läser sidan själv; den lägger dem i minnet som minne() ger
 * och lämnar dem med slapp(). Efter varje bild finns skärmen som
 * punkter (punkter(), 480 x 250, en byte per punkt, 1 = tänd); fosforn
 * (skenet och efterlysningen) ritar sidan. pip() räknar pipen.
 *
 * Det mesta motsvarar flaggorna i vard/abc802.c och sdl/fonster.c:
 * starta() har byglarna -40 och -60, och skiva() är -D: en avbild i
 * MF0 (DR0), kort 44 enhet 0. Skivan sitter kvar när maskinen startas
 * om; sidan hämtar den med skivans_data() för att spara den.
 *
 * Kassetten är -T och -U: band() lägger i ett band att spela upp och
 * tomt_band() ett tomt att spela in på (4 MB). spola() spolar tillbaka;
 * ett inspelat band spelas sedan upp. Bandet sitter också kvar när
 * maskinen startas om. Det som går ut på CH A (PR:) och CH B (V24:)
 * hamnar i en buffert per kontakt (64 KB) som sidan tömmer efter varje
 * bild (ut_data(), ut_antal(), ut_tomt()), och seriellt_in() är -a och
 * -b: ett tecken in, 0 om kön är full; seriellt_vantar() säger hur många
 * som ännu inte har gått in.
 *
 * Skrivarkortet (kort 60, ../karna/skrivarkort.h) är -P: skrivarkortet(1)
 * sätter i det och skrivarkortet(0) tar ur det. Det som skrivs ut hamnar
 * i en tredje buffert, kontakt 2 för ut_data() och de andra.
 */

#include <stdlib.h>
#include <string.h>

#include "../karna/abc802.h"
#include "../karna/skivkort.h"
#include "../karna/skrivarkort.h"

#define EXPORT(namn) __attribute__((export_name(#namn)))

static Abc802 maskin;
static Skivkort skivkort;
static Skrivarkort skrivarkort;
static int skrivarkort_i;
static uint8_t bilden[ABC802_BREDD * ABC802_HOJD];
static char raden[ABC802_RADLANGD];

#define INSPELNING (1L << 22)
static uint8_t *bandet;                      /* bandet i kassetten */
static long bandets_storlek;
static int spelar_in;

#define UT_STORLEK 65536
#define SKRIVAREN 2                          /* kontakt 2: skrivarkortet */
static uint8_t ut_buffert[3][UT_STORLEK];
static long ut_i[3];

static void seriellt_ut(void *data, int kontakt, uint8_t tecken)
{
    (void)data;
    if (ut_i[kontakt] < UT_STORLEK)
        ut_buffert[kontakt][ut_i[kontakt]++] = tecken;
}

static void skrivaren(void *data, uint8_t tecken)
{
    seriellt_ut(data, SKRIVAREN, tecken);
}

static void bandet_i(void)
{
    if (spelar_in)
        kassett802_spela_in(&maskin.kassett, bandet, INSPELNING);
    else if (bandet)
        kassett802_spela(&maskin.kassett, bandet, bandets_storlek);
}

/* ---------------------------------------------------------------- */
/* Minnet för filerna                                                */

EXPORT(minne) void *minne(long storlek) { return malloc((size_t)storlek); }
EXPORT(slapp) void slapp(void *p) { free(p); }

/* ---------------------------------------------------------------- */
/* Maskinen                                                          */

/* Töm maskinen, lägg i ROM:en (32 KB) och teckengeneratorn (4 eller
 * 8 KB), sätt byglarna (80 tecken, 60 Hz) och starta (RESET). 0 om
 * någon av filerna har fel storlek. */
EXPORT(starta) int starta(const uint8_t *rom, long rom_storlek,
                          const uint8_t *tecken, long tecken_storlek,
                          int bygel_80, int bygel_60hz)
{
    abc802_starta(&maskin);
    if (!skivkort.kort.data)
        skivkort_starta(&skivkort);
    skivkort.lage = 0;
    bussen_koppla(&maskin.bussen, &skivkort.kort);
    skrivarkort_starta(&skrivarkort);
    skrivarkort.tecken = skrivaren;
    if (skrivarkort_i)
        bussen_koppla(&maskin.bussen, &skrivarkort.kort);
    if (!abc802_rom(&maskin, rom, (size_t)rom_storlek) ||
        !abc802_teckenrom(&maskin, tecken, (size_t)tecken_storlek))
        return 0;
    maskin.bygel_80 = bygel_80;
    maskin.bygel_60hz = bygel_60hz;
    maskin.seriellt_ut = seriellt_ut;
    ut_i[0] = ut_i[1] = ut_i[SKRIVAREN] = 0;
    bandet_i();
    abc802_aterstall(&maskin);
    return 1;
}

/* Processorn och kretsarna till startläget, som RESET (minnet ligger
 * kvar). */
EXPORT(aterstall) void aterstall(void) { abc802_aterstall(&maskin); }

/* Kör en bild, 20 ms. Med rita blir skärmen punkter. */
EXPORT(bild) void bild(int rita)
{
    abc802_kor(&maskin, ABC802_MS(20));
    if (rita)
        abc802_bild(&maskin, bilden);
}

EXPORT(punkter) uint8_t *punkter(void) { return bilden; }

/* Antalet pip sedan start (kod 7 till tangentbordet). */
EXPORT(pip) long pip(void) { return maskin.pip; }

/* 40 tecken per rad just nu (RTS på DART B). */
EXPORT(fyrtio) int fyrtio(void) { return abc802_fyrtio(&maskin); }

/* Rad 0-23 på skärmen som text i UTF-8 (en nolla efter), t.ex. för att
 * kopiera skärmen. */
EXPORT(rader) int rader(void)
{
    return abc802_rader(&maskin);
}

EXPORT(textrad) const char *textrad(int rad)
{
    abc802_textrad(&maskin, rad, raden);
    return raden;
}

/* ---------------------------------------------------------------- */
/* Skivminnet                                                        */

/* En avbild i MF0 (kort 44 enhet 0) i stället för den som satt där;
 * data kommer från minne() och hör sedan till kortet. 0 om storleken
 * inte är hela sektorer. */
EXPORT(skiva) int skiva(uint8_t *data, long storlek)
{
    if (storlek <= 0 || storlek % 256) {
        free(data);
        return 0;
    }
    for (int k = 0; k < skivkort.antal_enheter; k++) {
        Skivenhet *e = &skivkort.enhet[k];
        if (e->kort == 44 && e->enhet == 0) {
            free(e->data);
            e->data = data;
            e->sektorer = storlek / 256;
            return 1;
        }
    }
    return skivkort_enhet(&skivkort, 44, 0, data, storlek, 0);
}

/* Skivan i MF0 som den är nu, och dess storlek i bytes. */
EXPORT(skivans_data) uint8_t *skivans_data(void)
{
    return skivkort.antal_enheter ? skivkort.enhet[0].data : NULL;
}

EXPORT(skivans_storlek) long skivans_storlek(void)
{
    return skivkort.antal_enheter ? skivkort.enhet[0].sektorer * 256 : 0;
}

/* ---------------------------------------------------------------- */
/* Tangenterna                                                       */

EXPORT(tangent) void tangent(int kod) { abc802_tangent(&maskin, (uint8_t)kod); }
EXPORT(paus) void paus(long ms) { abc802_paus(&maskin, ms); }
EXPORT(tangenter_klara) int tangenter_klara(void) { return abc802_ko_tom(&maskin); }

/* ---------------------------------------------------------------- */
/* Kassetten                                                         */

/* Ett band att spela upp, från början; data kommer från minne() och
 * hör sedan till kassetten. */
EXPORT(band) void band(uint8_t *data, long storlek)
{
    free(bandet);
    bandet = data;
    bandets_storlek = storlek;
    spelar_in = 0;
    bandet_i();
}

/* Ett tomt band, med inspelningsknappen nedtryckt. 0 om minnet inte
 * räcker. */
EXPORT(tomt_band) int tomt_band(void)
{
    uint8_t *ny = malloc(INSPELNING);
    if (!ny)
        return 0;
    free(bandet);
    bandet = ny;
    bandets_storlek = 0;
    spelar_in = 1;
    bandet_i();
    return 1;
}

/* Tillbaka till början; det som är inspelat spelas sedan upp. */
EXPORT(spola) void spola(void)
{
    if (spelar_in) {
        bandets_storlek = maskin.kassett.antal_inspelat;
        spelar_in = 0;
    }
    bandet_i();
}

/* Bandet som det är nu (för att spara det), dess längd i tecken, var
 * huvudet står, och om det spelas in, om motorn går och om ett
 * inspelat band tog slut. */
EXPORT(bandets_data) uint8_t *bandets_data(void) { return bandet; }
EXPORT(bandets_langd) long bandets_langd(void)
{
    return spelar_in ? maskin.kassett.antal_inspelat : bandets_storlek;
}
EXPORT(bandets_lage) long bandets_lage(void) { return maskin.kassett.lage; }
EXPORT(spelar_inne) int spelar_inne(void) { return spelar_in; }
EXPORT(motor) int motor(void) { return abc802_motor(&maskin); }
EXPORT(bandet_fullt) int bandet_fullt(void) { return maskin.kassett.full; }

/* ---------------------------------------------------------------- */
/* CH A och CH B                                                     */

static int kontakten(int kontakt) { return kontakt == SKRIVAREN ? SKRIVAREN : kontakt & 1; }

EXPORT(ut_data) uint8_t *ut_data(int kontakt) { return ut_buffert[kontakten(kontakt)]; }
EXPORT(ut_antal) long ut_antal(int kontakt) { return ut_i[kontakten(kontakt)]; }
EXPORT(ut_tomt) void ut_tomt(int kontakt) { ut_i[kontakten(kontakt)] = 0; }
EXPORT(seriellt_in) int seriellt_in(int kontakt, int tecken)
{
    if (kontakt == SKRIVAREN)                /* skrivaren skickar inget */
        return 1;
    return abc802_seriellt_in(&maskin, kontakt & 1, (uint8_t)tecken);
}

/* Tecken som ännu inte har gått in på kontakten (0 eller 1). */
EXPORT(seriellt_vantar) int seriellt_vantar(int kontakt)
{
    return abc802_seriellt_vantar(&maskin, kontakt & 1);
}

/* Skrivarkortet i (1) eller ur (0) bussen. PR: väljer det själv först
 * vid nästa RESET; dessförinnan med "PR:C". */
EXPORT(skrivarkortet) void skrivarkortet(int i)
{
    if (i && !skrivarkort_i)
        bussen_koppla(&maskin.bussen, &skrivarkort.kort);
    else if (!i && skrivarkort_i)
        bussen_koppla_ur(&maskin.bussen, &skrivarkort.kort);
    skrivarkort_i = i != 0;
}
