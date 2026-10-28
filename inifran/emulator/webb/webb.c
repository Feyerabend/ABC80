/*
 * webb.c -- värden i webbläsaren, till boken ABC80 inifrån
 *
 * Kärnan byggs till WebAssembly (se Makefile, make webb) och styrs av
 * abc80.js i sidan. Här finns de funktioner som sidan anropar: den
 * startar maskinen med en ROM, lägger i kretsar och skivor, kör en bild
 * (20 ms) i taget, skriver tangenter, lägger i kassettband som WAV och
 * spelar in. Filerna läser sidan själv; den lägger dem i minnet som
 * minne() ger och lämnar dem med slapp(). Efter varje bild finns
 * skärmen som punkter (punkter(), 320 x 240, RGBA; 640 x 240 med TKN80
 * i 80-teckensläget, se kolumner()) och ljudet som prov
 * (ljudprov(), 16 bitar, med den provtakt som starta() fick).
 *
 * Det mesta motsvarar flaggorna i vard/abc80.c: krets() är -l, bank()
 * -b, skiva() -D, band() -T, spela_in() -U, p40() -CP, centronics() -C 00,
 * tkn80() -tkn80, cpm() -cpm, v24() -s och hr80() -H.
 */

#include <stdlib.h>
#include <string.h>

#include "../karna/bild.h"
#include "../karna/hr80v24.h"
#include "../karna/kassett.h"
#include "../karna/ljud.h"
#include "../karna/maskin.h"
#include "../karna/p40.h"
#include "../karna/skivkort.h"
#include "../karna/skrivarkort.h"
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
static Skrivarkort snittkortet;              /* Centronics på kort 60 */
static int snittkortet_kopplat;
static Uni80 uni80;
static Hr80V24 hr80_kortet;                  /* grafikkortet på V24 */
static int hr80_startat;
static int cpm_lage;                         /* UNI-80 med start-PROM:en */

static uint32_t bilden[2 * BILD_BREDD * BILD_HOJD];   /* 80 tecken: dubbelt */
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
    snittkortet_kopplat = 0;
    cpm_lage = 0;                            /* maskin.uni80 är NULL igen */
    return 1;
}

/* En krets på adressen, skrivskyddad (-l fil@adr). */
EXPORT(krets) void krets(int adress, const uint8_t *data, long storlek)
{
    maskin_krets(&maskin, (uint16_t)adress, data, (size_t)storlek);
}

/* TKN80 med EPROM:en TKN80-III (-tkn80), efter starta(). 0 om den inte
 * passar ROM:en. */
EXPORT(tkn80) int tkn80(const uint8_t *data, long storlek)
{
    return maskin_tkn80(&maskin, data, (size_t)storlek);
}

/* UNI-80 med start-PROM:en (-cpm), efter starta(). Systemskivan ska
 * sitta i FD2 enhet 0 (skiva(45, 0, ...)). aterstall() startar sedan i
 * startkartan, så att PROM:en läser in CP/M från skivan, som när RESET
 * hålls nere på den riktiga maskinen. */
EXPORT(cpm) void cpm(const uint8_t *krets, long storlek)
{
    uni80_starta(&uni80);
    uni80_startkrets(&uni80, krets, (size_t)storlek);
    maskin.uni80 = &uni80;
    cpm_lage = 1;
}

/* En bank i en krets med banker (-b fil,fil@adr), en gång per bank i
 * ordning; en skrivning till adressen + $40 + n väljer bank n. */
EXPORT(bank) void bank(int adress, const uint8_t *data, long storlek)
{
    maskin_bank(&maskin, (uint16_t)adress, data, (size_t)storlek);
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

/* Processorn till startläget, som RESET (minnet ligger kvar). Med
 * cpm() startar CP/M om från skivan. */
EXPORT(aterstall) void aterstall(void)
{
    maskin_aterstall(&maskin);
    if (cpm_lage)
        uni80_karta(&uni80, UNI80_START);
}

/* Kör en bild, 20 ms. Med rita blir skärmen punkter, med markören om
 * markor. Ger antalet ljudprov för bilden. */
EXPORT(bild) int bild(int rita, int markor)
{
    /* Centronicsskrivaren är redo (status 0) tills sidan har en bit kvar
     * att hämta; då är den upptagen (BUSY, bit 4, och bit 1 för 9704:s
     * typ U), och drivrutinen väntar. */
    snittkortet.status = snittkortet.antal > SKRIVARE_BUFFERT - 1024 ? 0x12 : 0x00;
    maskin_kor(&maskin, MASKIN_BILD);
    long long antal = ljud_prov_till(&ljud, maskin.tid) - ljud.prov;
    while (antal > 2048) {                   /* det som inte får plats */
        ljud_prov(&ljud, proven, 2048);
        antal -= 2048;
    }
    int n = (int)antal;
    ljud_prov(&ljud, proven, n);
    if (rita) {
        int kolumner;
        const uint8_t *bildminne = maskin_skarm(&maskin, &kolumner);
        bild_rita(bildminne, kolumner, bilden, FORGRUND, BAKGRUND, markor);
    }
    return n;
}

/* Tecken per rad på skärmen: 40, eller 80 med TKN80 i 80-teckensläget;
 * punkter() är då 640 punkter bred. */
EXPORT(kolumner) int kolumner(void)
{
    int kolumner;
    maskin_skarm(&maskin, &kolumner);
    return kolumner;
}

EXPORT(punkter) uint32_t *punkter(void) { return bilden; }
EXPORT(ljudprov) int16_t *ljudprov(void) { return proven; }

/* ---------------------------------------------------------------- */
/* Tangenterna                                                       */

EXPORT(tangent) void tangent(int kod) { tangentko_lagg(&maskin, (uint8_t)kod); }
EXPORT(tangent_ned) void tangent_ned_w(int kod) { maskin_tangent_ned(&maskin, (uint8_t)kod); }
EXPORT(tangent_upp) void tangent_upp_w(void) { maskin_tangent_upp(&maskin); }
EXPORT(paus) void paus(long ms) { tangentko_paus(&maskin, ms); }
EXPORT(tangenter_klara) int tangenter_klara(void) { return tangentko_tom(&maskin); }

/* ---------------------------------------------------------------- */
/* V24                                                               */

/* Ett tecken till V24 (-s), i 1 200 baud. v24_om() börjar sända om ms
 * millisekunder; batchläget börjar 100 ms efter tangenterna. */
EXPORT(v24) void v24(int kod) { v24_lagg(&maskin, (uint8_t)kod); }
EXPORT(v24_om) void v24_om(long ms) { v24_starta(&maskin, maskin.tid + MASKIN_MS(ms)); }

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

/* Bildminnet som visas (1 KB från $7C00, eller med kolumner() == 80
 * TKN80:s 2 KB från $5800), t.ex. för att kopiera skärmen som text. */
EXPORT(bildminne) const uint8_t *bildminne(void)
{
    int kolumner;
    return maskin_skarm(&maskin, &kolumner);
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

/* ---------------------------------------------------------------- */
/* Centronicsskrivaren                                               */

/* Centronicskortet på kort 60 (-C 00) in eller ur medan maskinen går,
 * med 9704 på $7800 (krets() först), som p40(). Skrivaren tar emot
 * tecknen som de är; sidan läser dem med centronics_tecken() och
 * centronics_antal() och tömmer med centronics_hamtat(). */
EXPORT(centronics) void centronics(int in)
{
    if (in && !snittkortet_kopplat) {
        skrivarkort_starta(&snittkortet, 0x00, 0x00, 0);
        bussen_koppla(&maskin.bussen, &snittkortet.kort);
    } else if (!in && snittkortet_kopplat) {
        bussen_koppla_ur(&maskin.bussen, &snittkortet.kort);
        memset(maskin.minne + 0x7800, 0xFF, 0x400);
    }
    snittkortet_kopplat = in;
}

EXPORT(centronics_antal) int centronics_antal(void) { return snittkortet.antal; }
EXPORT(centronics_tecken) const uint8_t *centronics_tecken(void) { return snittkortet.utskrift; }
EXPORT(centronics_hamtat) void centronics_hamtat(void) { snittkortet.antal = 0; }

/* ---------------------------------------------------------------- */
/* Grafikkortet i HR-80:s anda                                       */

/* Kortet på V24 (-H) in eller ur, efter starta() och när som helst
 * medan maskinen går. Bilden ligger kvar när kortet kopplas ur och
 * när maskinen startar om, som på ett kort med egen ström; hr80_rensa()
 * börjar om. Utanför boken (karna/hr80.h). */
EXPORT(hr80) void hr80(int in)
{
    if (in && !hr80_startat) {
        hr80_v24_starta(&hr80_kortet, &maskin);
        hr80_startat = 1;
    }
    maskin.pio_b_enhet = in ? &hr80_kortet.enhet : NULL;
}

EXPORT(hr80_rensa) void hr80_rensa(void)
{
    hr80_startat = 0;
    if (maskin.pio_b_enhet == &hr80_kortet.enhet)
        hr80(1);
}

/* Tecknen som kortet har fått, så att sidan ser när något har hänt. */
EXPORT(hr80_antal) long hr80_antal(void) { return hr80_startat ? hr80_kortet.antal : 0; }

/* Webbläsarens mus på bilden blir kortets mus (påhittad): läget i
 * bildens punkter, 512 x 256 uppifrån, och knappen (1 nere). */
EXPORT(hr80_mus) void hr80_mus_till(int x, int y, int knapp)
{
    if (!hr80_startat)
        return;
    Hr80 *k = &hr80_kortet.kort;
    hr80_mus(k, x - k->mx, y - k->my, knapp);
}

/* Bilden, 512 x 256 punkter i RGBA (varje rad visas två gånger), med
 * kortets markör om markor. ABC 810:s åtta färger, R först. */
EXPORT(hr80_bild) const uint32_t *hr80_bild(int markor)
{
    static uint32_t punkter[HR80_BREDD * HR80_HOJD];
    static const uint32_t farger[8] = {
        0xFF000000u, 0xFF0000FFu, 0xFF00FF00u, 0xFF00FFFFu,
        0xFFFF0000u, 0xFFFF00FFu, 0xFFFFFF00u, 0xFFFFFFFFu,
    };
    if (!hr80_startat) {
        hr80_v24_starta(&hr80_kortet, &maskin);
        hr80_startat = 1;
    }
    hr80_rita(&hr80_kortet.kort, punkter, farger, markor);
    return punkter;
}
