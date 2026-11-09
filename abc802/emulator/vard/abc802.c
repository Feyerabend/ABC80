/*
 * abc802.c -- emulatorns huvudprogram för ABC 802, till ABC 802 inifrån
 *
 * Användning:
 *     abc802 [flaggor]     kör maskinen, skriv tangenterna och skriv
 *                          sedan skärmen på standard ut
 *
 *     -r fil         ROM:en, 32 KB (standard ../roms/abc802rom.rom
 *                    räknat från emulator/)
 *     -c fil         teckengeneratorn (standard
 *                    ../roms/orig/abc802/ABC802-char.6490191-01.bin)
 *     -40            bygeln S3 på 40 tecken per rad
 *     -60            bygeln 50/60 Hz på 60 Hz
 *     -k text        tangenter att skriva efter starten; \n och \r blir RETURN,
 *                    \wms en paus på ms millisekunder,
 *                    \xHH tangentkoden HH (PF1-PF8 är \xC0-\xC7)
 *     -f fil         en textfil att skriva som tangenter (ett BASIC-program),
 *                    med en paus på 200 ms efter varje rad, som på webbsidan
 *     -L ms          pausen efter varje rad med -f (200); ett stort program
 *                    behöver mer, eftersom BASIC II tar längre tid på sig att
 *                    lägga in en rad ju större programmet är
 *     -K ms,ms       hur länge en tangent hålls nere och släppt (40,60)
 *     -t ms          kör så länge efter tangenterna (500)
 *     -p fil.png     bilden till en PNG-fil, en punkt per punkt
 *     -i             skriv antalet instruktioner, tiden, pipen och summerns
 *                    omslag (IN 5) efteråt
 *
 * Skivminnet (DOS v.20 i ROM:en, skivkortet i ../karna/skivkort.h):
 *     -D fil[@kort[.enhet]]  en skivavbild, standard kort 44 enhet 0
 *                    (MF0, DR0); kort 36 är hårddisken (HD0). Flera går bra
 *     -DP fil[@..]   samma, skrivskyddad
 *     -DS            skriv tillbaka avbilderna efter körningen
 *
 * Kassetten (SIO B, ../karna/kassett802.h; bandet är tecknen som de går
 * förbi huvudet):
 *     -T fil         ett band att spela upp (LOAD CAS:namn)
 *     -U fil         spela in på ett tomt band (SAVE CAS:namn); filen
 *                    skrivs efter körningen
 *
 * Kontakterna CH A (DART A, PR:) och CH B (SIO A, V24:):
 *     -A fil         det som går ut på CH A till filen (- = standard fel)
 *     -B fil         det som går ut på CH B till filen
 *     -a fil         filen in på CH A, i mottagningens takt
 *     -b fil         filen in på CH B; körningen varar tills den är inne
 *
 * Skrivarkortet (kort 60 på ABC-bussen, ../karna/skrivarkort.h):
 *     -P fil         sätt i kortet; det som skrivs ut går till filen
 *                    (- = standard fel). PR: väljer kortet själv
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../karna/abc802.h"
#include "../karna/skivkort.h"
#include "../karna/skrivarkort.h"
#include "png.h"

static Abc802 maskin;
static Skivkort skivkort;
static Skrivarkort skrivarkort;
static FILE *skrivarfil;
static const char *skivfil[SKIVKORT_ENHETER];

#define INSPELNING (1L << 22)                /* 4 MB, sex timmar band */
static uint8_t *inspelning;
static FILE *kontakt_ut[2];
static unsigned char *kontakt_in[2];
static size_t kontakt_in_storlek[2], kontakt_in_skickat[2];

static void seriellt_ut(void *data, int kontakt, uint8_t tecken)
{
    (void)data;
    if (kontakt_ut[kontakt])
        fputc(tecken, kontakt_ut[kontakt]);
}

/* Fyll köerna till CH A och CH B ur filerna. 1 så länge något är kvar. */
static int fyll_kontakter(void)
{
    int kvar = 0;
    for (int k = 0; k < 2; k++) {
        while (kontakt_in_skickat[k] < kontakt_in_storlek[k]
               && abc802_seriellt_in(&maskin, k, kontakt_in[k][kontakt_in_skickat[k]]))
            kontakt_in_skickat[k]++;
        if (kontakt_in_skickat[k] < kontakt_in_storlek[k] || abc802_seriellt_vantar(&maskin, k))
            kvar = 1;
    }
    return kvar;
}

static unsigned char *las_fil(const char *namn, size_t *storlek)
{
    FILE *f = fopen(namn, "rb");
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *data = malloc(n > 0 ? (size_t)n : 1);
    if (!data || fread(data, 1, (size_t)n, f) != (size_t)n) {
        fclose(f);
        free(data);
        return NULL;
    }
    fclose(f);
    *storlek = (size_t)n;
    return data;
}

/* Ett tecken i UTF-8 (Å Ä Ö É Ü å ä ö é ü) till ABC 802:s kod. */
static int svensk(unsigned char andra)
{
    switch (andra) {
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

/* Text till tangentkön: radslut blir RETURN (13), med en paus på
 * rad_paus ms efter. */
static void skriv_text(const char *text, size_t n, long rad_paus)
{
    for (size_t k = 0; k < n; k++) {
        unsigned char t = (unsigned char)text[k];
        if (t == '\r')
            continue;
        if (t == '\n') {
            abc802_tangent(&maskin, 13);
            if (rad_paus)
                abc802_paus(&maskin, rad_paus);
        } else if (t == 0xC3 && k + 1 < n && svensk((unsigned char)text[k + 1]))
            abc802_tangent(&maskin, (uint8_t)svensk((unsigned char)text[++k]));
        else if (t == '\\' && k + 1 < n && (text[k + 1] == 'n' || text[k + 1] == 'r')) {
            abc802_tangent(&maskin, 13);
            k++;
        } else if (t == '\\' && k + 1 < n && text[k + 1] == 'w') {
            long paus = 0;
            for (k += 2; k < n && text[k] >= '0' && text[k] <= '9'; k++)
                paus = paus * 10 + (text[k] - '0');
            abc802_paus(&maskin, paus);
            k--;
        } else if (t == '\\' && k + 3 < n && text[k + 1] == 'x'
                   && isxdigit((unsigned char)text[k + 2]) && isxdigit((unsigned char)text[k + 3])) {
            char hex[3] = { text[k + 2], text[k + 3], 0 };   /* \xC0 = PF1 */
            abc802_tangent(&maskin, (uint8_t)strtol(hex, NULL, 16));
            k += 3;
        } else if (t < 0x80)
            abc802_tangent(&maskin, t);
    }
}

/* En fil bredvid emulator/: katalogen där programmet ligger + namn. */
static char *bredvid(const char *program, const char *namn)
{
    static char sokvag[4096];
    const char *snedstreck = strrchr(program, '/');
    int n = snedstreck ? (int)(snedstreck - program + 1) : 0;
    snprintf(sokvag, sizeof sokvag, "%.*s%s", n, program, namn);
    return sokvag;
}

/* -D fil[@kort[.enhet]]: avbilden läses in i en buffert som kortet får. */
static void skiva(const char *arg, int skrivskydd)
{
    char *namn = strdup(arg), *at = strrchr(namn, '@');
    int kort = 44, enhet = 0;
    if (at) {
        *at = 0;
        kort = (int)strtol(at + 1, &at, 10);
        if (*at == '.')
            enhet = atoi(at + 1);
    }
    size_t storlek;
    unsigned char *data = las_fil(namn, &storlek);
    if (!data) {
        fprintf(stderr, "abc802: kan inte läsa %s\n", namn);
        exit(1);
    }
    skivfil[skivkort.antal_enheter] = namn;
    if (!skivkort_enhet(&skivkort, kort, enhet, data, (long)storlek, skrivskydd)) {
        fprintf(stderr, "abc802: högst %d avbilder: %s\n", SKIVKORT_ENHETER, namn);
        exit(1);
    }
}

static int spara_skivor(void)
{
    for (int k = 0; k < skivkort.antal_enheter; k++) {
        FILE *f = fopen(skivfil[k], "wb");
        if (!f) {
            fprintf(stderr, "abc802: kan inte skriva %s\n", skivfil[k]);
            return 0;
        }
        fwrite(skivkort.enhet[k].data, 256, (size_t)skivkort.enhet[k].sektorer, f);
        fclose(f);
    }
    return 1;
}

static void skrivaren(void *data, uint8_t tecken)
{
    (void)data;
    fputc(tecken, skrivarfil);
    fflush(skrivarfil);
}

static void anvandning(void)
{
    fprintf(stderr, "användning: abc802 [-r rom] [-c teckenrom] [-40] [-60] "
                    "[-k text] [-f fil] [-L ms] [-K ms,ms] [-t ms] [-p fil.png] [-i]\n"
                    "              [-D fil[@kort[.enhet]]] [-DP fil[@..]] [-DS]\n"
                    "              [-T fil] [-U fil] [-A fil] [-B fil] [-a fil] [-b fil]\n"
                    "              [-P fil]\n");
    exit(2);
}

int main(int argc, char **argv)
{
    const char *romnamn = NULL, *teckennamn = NULL, *pngnamn = NULL;
    const char *bandnamn = NULL, *inspelningsnamn = NULL;
    long ms = 500, rad_paus = 200;
    int info = 0, spara = 0;

    abc802_starta(&maskin);
    skivkort_starta(&skivkort);
    char rom_standard[4096], tecken_standard[4096];
    snprintf(rom_standard, sizeof rom_standard, "%s",
             bredvid(argv[0], "../roms/abc802rom.rom"));
    snprintf(tecken_standard, sizeof tecken_standard, "%s",
             bredvid(argv[0], "../roms/orig/abc802/ABC802-char.6490191-01.bin"));

    /* Tangenterna läggs i kön i den ordning de står, efter flaggorna
       för maskinen; de sparas först. */
    const char *tangenter[64];
    int ar_fil[64], antal = 0;
    for (int k = 1; k < argc; k++) {
        const char *a = argv[k];
        int har_varde = k + 1 < argc;
        if (!strcmp(a, "-r") && har_varde)
            romnamn = argv[++k];
        else if (!strcmp(a, "-c") && har_varde)
            teckennamn = argv[++k];
        else if (!strcmp(a, "-40"))
            maskin.bygel_80 = 0;
        else if (!strcmp(a, "-60"))
            maskin.bygel_60hz = 1;
        else if ((!strcmp(a, "-k") || !strcmp(a, "-f")) && har_varde && antal < 64) {
            ar_fil[antal] = a[1] == 'f';
            tangenter[antal++] = argv[++k];
        } else if (!strcmp(a, "-K") && har_varde) {
            if (sscanf(argv[++k], "%ld,%ld", &maskin.tryck_ms, &maskin.slapp_ms) != 2)
                anvandning();
        } else if (!strcmp(a, "-L") && har_varde)
            rad_paus = atol(argv[++k]);
        else if (!strcmp(a, "-t") && har_varde)
            ms = atol(argv[++k]);
        else if (!strcmp(a, "-p") && har_varde)
            pngnamn = argv[++k];
        else if (!strcmp(a, "-i"))
            info = 1;
        else if ((!strcmp(a, "-D") || !strcmp(a, "-DP")) && har_varde) {
            int skydd = a[2] == 'P';
            skiva(argv[++k], skydd);
        } else if (!strcmp(a, "-DS"))
            spara = 1;
        else if (!strcmp(a, "-T") && har_varde)
            bandnamn = argv[++k];
        else if (!strcmp(a, "-U") && har_varde)
            inspelningsnamn = argv[++k];
        else if ((!strcmp(a, "-A") || !strcmp(a, "-B")) && har_varde) {
            int kontakt = a[1] == 'B';
            const char *namn = argv[++k];
            kontakt_ut[kontakt] = !strcmp(namn, "-") ? stderr : fopen(namn, "wb");
            if (!kontakt_ut[kontakt]) {
                fprintf(stderr, "abc802: kan inte skriva %s\n", namn);
                return 1;
            }
        } else if ((!strcmp(a, "-a") || !strcmp(a, "-b")) && har_varde) {
            int kontakt = a[1] == 'b';
            const char *namn = argv[++k];
            kontakt_in[kontakt] = las_fil(namn, &kontakt_in_storlek[kontakt]);
            if (!kontakt_in[kontakt]) {
                fprintf(stderr, "abc802: kan inte läsa %s\n", namn);
                return 1;
            }
        } else if (!strcmp(a, "-P") && har_varde) {
            const char *namn = argv[++k];
            skrivarfil = !strcmp(namn, "-") ? stderr : fopen(namn, "wb");
            if (!skrivarfil) {
                fprintf(stderr, "abc802: kan inte skriva %s\n", namn);
                return 1;
            }
        } else
            anvandning();
    }

    size_t storlek;
    unsigned char *data = las_fil(romnamn ? romnamn : rom_standard, &storlek);
    if (!data || !abc802_rom(&maskin, data, storlek)) {
        fprintf(stderr, "abc802: ROM:en saknas eller har fel storlek (32 KB)\n");
        return 1;
    }
    free(data);
    data = las_fil(teckennamn ? teckennamn : tecken_standard, &storlek);
    if (!data || !abc802_teckenrom(&maskin, data, storlek)) {
        fprintf(stderr, "abc802: teckengeneratorn saknas eller har fel storlek\n");
        return 1;
    }
    free(data);

    if (skivkort.antal_enheter)
        bussen_koppla(&maskin.bussen, &skivkort.kort);
    if (skrivarfil) {
        skrivarkort_starta(&skrivarkort);
        skrivarkort.tecken = skrivaren;
        bussen_koppla(&maskin.bussen, &skrivarkort.kort);
    }
    if (bandnamn) {
        unsigned char *band = las_fil(bandnamn, &storlek);
        if (!band) {
            fprintf(stderr, "abc802: kan inte läsa %s\n", bandnamn);
            return 1;
        }
        kassett802_spela(&maskin.kassett, band, (long)storlek);
    }
    if (inspelningsnamn) {
        inspelning = malloc(INSPELNING);
        kassett802_spela_in(&maskin.kassett, inspelning, INSPELNING);
    }
    maskin.seriellt_ut = seriellt_ut;
    abc802_aterstall(&maskin);
    abc802_paus(&maskin, 1000);              /* starten */
    for (int k = 0; k < antal; k++) {
        if (!ar_fil[k]) {
            skriv_text(tangenter[k], strlen(tangenter[k]), 0);
            continue;
        }
        unsigned char *text = las_fil(tangenter[k], &storlek);
        if (!text) {
            fprintf(stderr, "abc802: kan inte läsa %s\n", tangenter[k]);
            return 1;
        }
        skriv_text((const char *)text, storlek, rad_paus);
        free(text);
    }

    while (fyll_kontakter() | !abc802_ko_tom(&maskin))
        abc802_kor(&maskin, ABC802_MS(20));
    abc802_kor(&maskin, ABC802_MS(ms));

    if (inspelningsnamn) {
        FILE *f = fopen(inspelningsnamn, "wb");
        if (!f || fwrite(inspelning, 1, (size_t)maskin.kassett.antal_inspelat, f)
                      != (size_t)maskin.kassett.antal_inspelat) {
            fprintf(stderr, "abc802: kan inte skriva %s\n", inspelningsnamn);
            return 1;
        }
        fclose(f);
        if (maskin.kassett.full)
            fprintf(stderr, "abc802: bandet tog slut\n");
    }
    for (int k = 0; k < 2; k++)
        if (kontakt_ut[k] && kontakt_ut[k] != stderr)
            fclose(kontakt_ut[k]);

    char rad[ABC802_RADLANGD];
    int sista = -1;
    for (int r = 0; r < abc802_rader(&maskin); r++) {
        abc802_textrad(&maskin, r, rad);
        if (rad[0])
            sista = r;
    }
    for (int r = 0; r <= sista; r++) {
        abc802_textrad(&maskin, r, rad);
        puts(rad);
    }
    if (pngnamn) {
        static uint8_t bild[ABC802_BREDD * ABC802_HOJD];
        abc802_bild(&maskin, bild);
        for (int k = 0; k < ABC802_BREDD * ABC802_HOJD; k++)
            bild[k] = !bild[k];              /* png_skriv: 1 = svart */
        if (!png_skriv(pngnamn, bild, ABC802_BREDD, ABC802_HOJD)) {
            fprintf(stderr, "abc802: kan inte skriva %s\n", pngnamn);
            return 1;
        }
    }
    if (spara && !spara_skivor())
        return 1;
    if (info)
        fprintf(stderr, "%lld instruktioner, %.3f s, %ld pip, %ld omslag i summern\n",
                maskin.instruktioner, (double)maskin.tid / ABC802_KLOCKA, maskin.pip,
                maskin.summer_omslag);
    return 0;
}
