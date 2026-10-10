/*
 * webb.c -- värden i webbläsaren, till boken ABC80 inifrån
 *
 * Kärnan byggs till WebAssembly (se Makefile, make webb) och styrs av
 * abc80.js i sidan. Här finns de funktioner som sidan anropar: den
 * startar maskinen med en ROM, lägger i kretsar och skivor, kör en bild
 * (20 ms) i taget, skriver tangenter, lägger i kassettband som WAV och
 * spelar in. Filerna läser sidan själv; den lägger dem i minnet som
 * minne() ger och lämnar dem med slapp(). Efter varje bild finns
 * skärmen som punkter (punkter(), 320 x 240, RGBA) och ljudet som prov
 * (ljudprov(), 16 bitar, med den provtakt som starta() fick).
 *
 * Det mesta motsvarar flaggorna i vard/abc80.c: krets() är -l,
 * skiva() -D, band() -T, spela_in() -U och p40() -CP.
 */

#include <stdlib.h>
#include <string.h>

#include "../karna/bild.h"
#include "../karna/kassett.h"
#include "../karna/ljud.h"
#include "../karna/maskin.h"
#include "../karna/p40.h"
#include "../karna/skivkort.h"
#include "../vard/wav.h"

#define EXPORT(namn) __attribute__((export_name(#namn)))

#define FORGRUND 0xFFF0F0F0u                 /* RGBA i minnet: R först */
#define BAKGRUND 0xFF000000u
#define INSPELNING (1L << 20)                /* flanker, ungefär 300 block */

static Maskin maskin;
static Ljud ljud;
static Kassett kassett;
static Skivkort skivkort;
static int skivkortet_kopplat;
static P40 skrivaren;
static int skrivaren_kopplad;

static uint32_t bilden[BILD_BREDD * BILD_HOJD];
static int16_t proven[2048];                 /* en bild: 960 vid 48 kHz */
static long long *bandet;                    /* flankerna som spelas upp */
static long bandets_flanker;
static long long *inspelningen;
static uint8_t *inspelad_wav;
static long inspelad_storlek;

/* ---------------------------------------------------------------- */
/* Minnet för filerna                                                */

EXPORT(minne) void *minne(long storlek) { return malloc((size_t)storlek); }
EXPORT(slapp) void slapp(void *p) { free(p); }

/* ---------------------------------------------------------------- */
/* Maskinen                                                          */

/* Töm maskinen och lägg i ROM:en (16 eller 32 KB). Ljudet räknas med
 * provtakt prov per sekund. 0 om ROM:en har fel storlek. Efter kretsar
 * och skivor: aterstall(). */
EXPORT(starta) int starta(int ram_kb, const uint8_t *rom, long storlek, long provtakt)
{
    maskin_starta(&maskin, ram_kb);
    if (!maskin_rom(&maskin, rom, (size_t)storlek))
        return 0;
    ljud_starta(&ljud, provtakt, MASKIN_KLOCKA, 0x00A5C301);
    maskin.ljud = &ljud;
    int stopp = kassett.stopp, utan = kassett.utan_fjarrstyrning;
    kassett_starta(&kassett);                /* bandet sitter kvar, tillbakaspolat */
    kassett.stopp = stopp;
    kassett.utan_fjarrstyrning = utan;
    kassett_spela(&kassett, bandet, bandets_flanker);
    maskin.kassett = &kassett;
    skivkort_starta(&skivkort);
    skivkortet_kopplat = 0;
    skrivaren_kopplad = 0;
    return 1;
}

/* En krets på adressen, skrivskyddad (-l fil@adr). */
EXPORT(krets) void krets(int adress, const uint8_t *data, long storlek)
{
    maskin_krets(&maskin, (uint16_t)adress, data, (size_t)storlek);
}

/* CMOS-minnet (storlek bytes på adressen, som -M): RAM med data i.
 * Ger platsen i minnet, där sidan läser det för att spara det. */
EXPORT(cmos) uint8_t *cmos(int adress, const uint8_t *data, long storlek)
{
    return maskin_cmos(&maskin, (uint16_t)adress, data, (size_t)storlek);
}

/* En skivavbild på kortet (44, 45 eller 36) och enheten (-D). data ska
 * ha 256 bytes över efter avbilden och ligga kvar; kortet skriver i
 * den. 0 om det redan finns åtta. */
EXPORT(skiva) int skiva(int kort, int enhet, uint8_t *data, long storlek, int skrivskydd)
{
    if (!skivkort_enhet(&skivkort, kort, enhet, data, storlek, skrivskydd))
        return 0;
    if (!skivkortet_kopplat) {
        bussen_koppla(&maskin.bussen, &skivkort.kort);
        skivkortet_kopplat = 1;
    }
    return 1;
}

/* Processorn till startläget, som RESET (minnet ligger kvar). */
EXPORT(aterstall) void aterstall(void)
{
    maskin_aterstall(&maskin);
}

/* Kör en bild, 20 ms. Med rita blir skärmen punkter, med markören om
 * markor. Ger antalet ljudprov för bilden. */
EXPORT(bild) int bild(int rita, int markor)
{
    maskin_kor(&maskin, MASKIN_BILD);
    long long antal = ljud_prov_till(&ljud, maskin.tid) - ljud.prov;
    while (antal > 2048) {                   /* det som inte får plats */
        ljud_prov(&ljud, proven, 2048);
        antal -= 2048;
    }
    int n = (int)antal;
    ljud_prov(&ljud, proven, n);
    if (rita)
        bild_rita(maskin.minne + MASKIN_BILDMINNE, bilden, FORGRUND, BAKGRUND, markor);
    return n;
}

EXPORT(punkter) uint32_t *punkter(void) { return bilden; }
EXPORT(ljudprov) int16_t *ljudprov(void) { return proven; }

/* ---------------------------------------------------------------- */
/* Tangenterna                                                       */

EXPORT(tangent) void tangent(int kod) { tangentko_lagg(&maskin, (uint8_t)kod); }
EXPORT(paus) void paus(long ms) { tangentko_paus(&maskin, ms); }
EXPORT(tangenter_klara) int tangenter_klara(void) { return tangentko_tom(&maskin); }

/* ---------------------------------------------------------------- */
/* Kassetten                                                         */

/* Spola tillbaka till bandets början; bandet går om motorreläet är till. */
static void spola_tillbaka(void)
{
    kassett.band = 0;
    kassett.motor = !kassett.stopp && (kassett.motorrelaet || kassett.utan_fjarrstyrning);
    kassett.motor_start = maskin.tid;
    kassett.langst = 0;
    kassett.spelar_in = 0;
}

/* Lägg i ett band (en WAV-fil i minnet; sidan kan släppa den sedan).
 * Ger antalet flanker, eller -1 om filen inte går att läsa. */
EXPORT(band) long band(const uint8_t *wav, long storlek)
{
    const char *fel;
    long n = wav_till_flanker(wav, storlek, NULL, 0, &fel);
    if (n < 0)
        return -1;
    long long *flanker = malloc((size_t)(n ? n : 1) * sizeof *flanker);
    if (!flanker)
        return -1;
    wav_till_flanker(wav, storlek, flanker, n, &fel);
    free(bandet);
    bandet = flanker;
    bandets_flanker = n;
    spola_tillbaka();
    kassett_spela(&kassett, bandet, n);
    return n;
}

/* Tryck ned inspelningsknappen på ett tomt band. 0 om minnet inte räcker. */
EXPORT(spela_in) int spela_in(void)
{
    if (!inspelningen)
        inspelningen = malloc(INSPELNING * sizeof *inspelningen);
    if (!inspelningen)
        return 0;
    free(bandet);
    bandet = NULL;
    bandets_flanker = 0;
    spola_tillbaka();
    kassett_spela(&kassett, NULL, 0);
    kassett_spela_in(&kassett, inspelningen, INSPELNING);
    return 1;
}

/* Det som har spelats in, som en WAV-fil (44 100 prov/s); storleken
 * ger inspelning_storlek(). Filen ligger kvar till nästa anrop. */
EXPORT(inspelning) uint8_t *inspelning(void)
{
    long long slut = kassett_lage(&kassett, maskin.tid);
    if (kassett.langst > slut)
        slut = kassett.langst;
    free(inspelad_wav);
    inspelad_storlek = wav_fran_flanker(kassett.inspelning, kassett.antal_inspelat, slut, NULL);
    inspelad_wav = malloc((size_t)inspelad_storlek);
    if (!inspelad_wav)
        return NULL;
    wav_fran_flanker(kassett.inspelning, kassett.antal_inspelat, slut, inspelad_wav);
    return inspelad_wav;
}

EXPORT(inspelning_storlek) long inspelning_storlek(void) { return inspelad_storlek; }
EXPORT(inspelade_flanker) long inspelade_flanker(void) { return kassett.antal_inspelat; }
EXPORT(motor) int motor(void) { return kassett.motorrelaet; }
EXPORT(bandet_gar) int bandet_gar(void) { return kassett.motor; }

/* Bandspelarens knappar: PLAY nere (1) eller uppe, fjärrstyrd (1,
 * motorreläet styr) eller inte (bandet går så länge PLAY är nere). */
EXPORT(knappar) void knappar(int play_nere, int fjarrstyrd)
{
    kassett_knappar(&kassett, maskin.tid, play_nere, fjarrstyrd);
}

/* Spola till läget i sekunder (bandet som spelas upp, inte under
 * inspelning): 0 är början, och längre än bandet går inte. */
EXPORT(spola) void spola(double sekunder)
{
    if (kassett.spelar_in)
        return;
    double langd = kassett.antal_spelas
                 ? (double)kassett.spelas[kassett.antal_spelas - 1] / MASKIN_KLOCKA : 0;
    if (sekunder > langd)
        sekunder = langd;
    kassett_spola(&kassett, maskin.tid, (long long)(sekunder * MASKIN_KLOCKA));
}

/* Bandets läge och längd i sekunder. */
EXPORT(bandlage) double bandlage(void)
{
    return (double)kassett_lage(&kassett, maskin.tid) / MASKIN_KLOCKA;
}

EXPORT(bandlangd) double bandlangd(void)
{
    if (kassett.spelar_in)
        return bandlage();
    return kassett.antal_spelas
         ? (double)kassett.spelas[kassett.antal_spelas - 1] / MASKIN_KLOCKA : 0;
}

/* Bildminnet (1 KB från $7C00), t.ex. för att kopiera skärmen som text. */
EXPORT(bildminne) const uint8_t *bildminne(void)
{
    return maskin.minne + MASKIN_BILDMINNE;
}

/* ---------------------------------------------------------------- */
/* Skrivaren P40                                                     */

/* Skrivaren på kort 60 (-CP) in eller ur medan maskinen går; drivrutinen
 * ($7800) läggs i med krets() först. BASIC letar upp drivrutinen när
 * PR: öppnas, så maskinen behöver inte startas om. Ur tas också
 * drivrutinen bort: platsen blir tom ($FF), som efter starta().
 * Papperet börjar tomt när skrivaren kopplas in. */
EXPORT(p40) void p40(int in)
{
    if (in && !skrivaren_kopplad) {
        p40_starta(&skrivaren, &maskin.tid);
        bussen_koppla(&maskin.bussen, &skrivaren.kort);
    } else if (!in && skrivaren_kopplad) {
        bussen_koppla_ur(&maskin.bussen, &skrivaren.kort);
        memset(maskin.minne + 0x7800, 0xFF, 0x400);
    }
    skrivaren_kopplad = in;
}

/* Anslagen hittills: p40_anslag() pekar på p40_antal() stycken, 12 bytes
 * vart (x i fjärdedels kolumner, pappersraden, nålarna; se p40.h). */
EXPORT(p40_antal) int p40_antal(void) { return skrivaren.antal; }
EXPORT(p40_anslag) const P40Anslag *p40_anslag(void) { return skrivaren.anslag; }
EXPORT(p40_fullt) int p40_fullt(void) { return skrivaren.antal >= P40_ANSLAG; }

/* Riv av papperet: anslagen glöms, men vagnen står kvar. */
EXPORT(p40_riv) void p40_riv(void) { skrivaren.antal = 0; }
