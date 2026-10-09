/*
 * abc80.c -- emulatorns huvudprogram, till boken ABC80 inifrån
 *
 * Användning:
 *     abc80 [flaggor]                  kör ABC80 i terminalen
 *     abc80 -t ms [flaggor]            kör utan terminal och skriv skärmen
 *     abc80 [flaggor] < /dev/null      samma, med -t 500 (standard in
 *                                      är ingen terminal, eller -x)
 *
 * Maskinen:
 *     -r fil         ROM:en, 16 KB ($0000-$3FFF) eller 32 KB (BASIC II,
 *                    $0000-$7BFF). Utan -r ../disass/abc80/abc80new.rom
 *                    bredvid programmet. Namnen 9913 och 11273 (ROM:arna i
 *                    fyra kretsar, ../dis/roms/orig/ABC80-ZA35xx-namn.bin)
 *                    och basicii80 (../basicii80/roms/basicii80.rom) går
 *                    också, räknat från programmet, i bokens arkiv
 *     -l fil@adr     en krets (skrivskyddad) på adressen, i hex; flera går bra
 *     -b f0,f1,..@adr  en krets med banker (högst 8 à 4 KB); skrivning till
 *                    adr+$40+n väljer bank n
 *     -m kb          RAM överst i minnet: 16 (standard), 32 eller 48
 *     -M fil[@adr]   CMOS-minne med batteri, 2 KB på adr (standard 5000,
 *                    som Super Smartaid): läses ur filen, om den finns,
 *                    och skrivs tillbaka när emulatorn slutar
 *
 * Tangenterna och V24:
 *     -k text        tangenter att skriva efter starten
 *     -f fil         en textfil att skriva som tangenter (ett BASIC-program);
 *                    -k och -f skrivs i den ordning de står
 *     -K ms,ms       hur länge en tangent hålls nere och släppt (40,60)
 *     -s text        tecken som tas emot på V24 efter tangenterna
 *     -S baud        hastigheten på V24 (1200)
 *     -W h@ms,...    labplattan på V24: strömbrytarna på ingångarna, bit
 *                    0-2 (hex, 7 = öppna) från tiden ms (@0 kan utelämnas);
 *                    i batchläget skrivs utgångarna (bit 3, 4) efter skärmen
 *     -L             V24-skrivaren på PIO B; tecknen skrivs som "PR: hh ..."
 *
 * Korten på ABC-bussen (se karna/skivkort.h, skrivarkort.h, ieckort.h):
 *     -D fil[@kort[.enhet]]  en skivavbild, standard kort 44 enhet 0;
 *                    kort 36 är hårddisken, 45 FD2. Flera går bra
 *     -DP fil[@..]   samma, skrivskyddad
 *     -DS            skriv tillbaka avbilderna efter körningen
 *     -DX n[,m]      skriv sektorerna n-m i den första avbilden efteråt
 *     -DB n          sektor n är trasig; -DBW n bara vid skrivning
 *     -DH n[,m]      FD2 svarar inte på kommando n (-m) förrän C3
 *     -DQ s+oo=hh,.. ändra bytes i den senaste avbilden: sektor s
 *                    (decimalt), plats oo (hex)
 *     -C hh[,bb,n]   skrivarkortet 60 med statusen hh (de n första
 *                    läsningarna bb); tecknen skrivs som "PR60: hh ..."
 *     -CR            ett bildminne på $4000-$43FF (skrivarkretsens typ R),
 *                    som skrivs efter skärmen med "K:" först
 *     -E adr[,text]  IEC-kortet 49 med adressen; text är det en talare
 *                    skickar (\r, \xHH, \\, \e = EOI med nästa tecken)
 *     -EN            ingen lyssnare på IEC-bussen
 *
 * Kassetten (se karna/kassett.h och vard/kassettfil.c):
 *     -T fil.wav     bandet som spelas upp (LOAD, OPEN); en WAV-fil med
 *                    en inspelning från ABC80 eller från -U
 *     -U fil.wav     spela in (SAVE, PREPARE): skrivsignalen skrivs till
 *                    filen efteråt, 44 100 prov/s
 *                    I batchläget körs maskinen så länge motorn går
 *                    (högst 30 minuter) innan tiden i -t börjar
 *
 * Batchläget:
 *     -t ms          kör så länge efter tangenterna och skriv sedan
 *                    skärmen på standard ut
 *     -d adr,n       skriv också n bytes av minnet från adr
 *     -F             filenheten TST: (bara BASIC II); -FD skriver filerna
 *     -x adr         anropsläget: anropa adr efter starten (se vard/anrop.c)
 *     -p adr=hh,..   lägg bytes i minnet före anropet
 *     -R HL=hhhh,..  registren före anropet (HL DE BC AF A F B C D E H L
 *                    IX IY SP)
 *     -G n           anropet är klart med SP upp till n bytes lägre
 *     -c             skriv varje nytt mål för CALL och RST under anropet
 *     -V             skriv skärmen också efter anropet
 *     -N             ingen NMI (klockan står)
 *     -P adr         registren på standard fel när PC når adr (högst 8)
 *     -O pp          OUT till porten pp på standard fel
 *     -w             skrivningar till skrivskyddat minne på standard fel
 *     -v             PC och antalet instruktioner på standard fel efteråt
 *     -a fil.wav     ljudet (OUT 6, SN76477) till en WAV-fil, 22 050 prov/s
 *     -i n           tiden i instruktioner som i abc80host: n per 20 ms
 *                    (abc80host har 6000); utan -i räknas T-cykler
 *
 * I texten till -k och -s är \r RETURN, \xHH koden HH, \\ ett \, \.
 * ingenting och \wN en paus på N ms (bara -k). Å, Ä, Ö, É, Ü och små
 * å, ä, ö, é, ü i UTF-8 blir ABC80:s koder för dem.
 *
 * Här läses alla filer; kärnan (karna/) får bara bytebuffertar.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../karna/tecken.h"
#include "vard.h"

void vard_fel(const char *text, const char *tillagg)
{
    fprintf(stderr, "abc80: %s%s\n", text, tillagg ? tillagg : "");
    exit(1);
}

/* Hela filen i en buffert som räcker till 64 KB; antalet bytes. */
static size_t las_fil(const char *namn, uint8_t *buffert, size_t storsta)
{
    FILE *f = fopen(namn, "rb");
    if (!f)
        vard_fel("kan inte öppna ", namn);
    size_t n = fread(buffert, 1, storsta, f);
    fclose(f);
    return n;
}

/* ---------------------------------------------------------------- */
/* Tangenttexten                                                     */

/* Texten till -k (till tangentkön, med pauser) eller -s (till V24). */
static void text(Maskin *m, const char *s, int till_v24)
{
    while (*s) {
        int kod;
        if (*s == '\\' && s[1]) {
            s++;
            if (*s == 'r') {
                kod = 0x0D;
                s++;
            } else if (*s == 'x') {
                char hex[3] = { s[1], s[1] ? s[2] : 0, 0 };
                kod = (int)strtol(hex, NULL, 16);
                s += 1 + strlen(hex);
            } else if (*s == 'w') {
                char *slut;
                long ms = strtol(s + 1, &slut, 10);
                s = slut;
                if (!till_v24)
                    tangentko_paus(m, ms);
                continue;
            } else if (*s == '.') {          /* \. skiljer \w5\.0 */
                s++;
                continue;
            } else
                kod = (uint8_t)*s++;
        } else if ((uint8_t)*s == 0xC3 && s[1]) {
            kod = tecken_svensk((uint8_t)s[1]);
            if (!kod)
                kod = '?';
            s += 2;
        } else
            kod = (uint8_t)*s++;
        if (till_v24)
            v24_lagg(m, (uint8_t)kod);
        else
            tangentko_lagg(m, (uint8_t)kod);
    }
}

/* En textfil (-f): radslut blir RETURN, med en paus på 200 ms efter. */
static void textfil(Maskin *m, const char *namn)
{
    FILE *f = fopen(namn, "rb");
    if (!f)
        vard_fel("kan inte öppna ", namn);
    int c;
    while ((c = getc(f)) != EOF) {
        if (c == '\r')
            continue;
        if (c == '\n') {
            tangentko_lagg(m, 0x0D);
            tangentko_paus(m, 200);
        } else if (c == 0xC3) {
            int andra = getc(f);
            uint8_t kod = andra == EOF ? 0 : tecken_svensk((uint8_t)andra);
            tangentko_lagg(m, kod ? kod : '?');
        } else
            tangentko_lagg(m, (uint8_t)c);
    }
    fclose(f);
}

/* -W h[@ms],...: strömbrytarna h (hex, bit 0-2) från tiden ms. */
static void labbtext(Maskin *m, const char *s)
{
    while (*s) {
        char *slut;
        int brytare = (int)strtol(s, &slut, 16);
        long ms = 0;
        if (*slut == '@')
            ms = strtol(slut + 1, &slut, 10);
        labb_lagg(m, brytare, ms);
        if (*slut != ',')
            break;
        s = slut + 1;
    }
}

/* ---------------------------------------------------------------- */
/* Kretsarna och ROM:en                                              */

static char programmets_katalog[1024];       /* med '/' sist, eller tom */

/* "fil@adr": skiljer namnet från adressen (hex). */
static int adress_efter(char *fil_och_adress, const char *flagga)
{
    char *at = strrchr(fil_och_adress, '@');
    if (!at)
        vard_fel(flagga, fil_och_adress);
    *at = 0;
    return (int)strtol(at + 1, NULL, 16);
}

static void krets(Maskin *m, const char *arg)
{
    static uint8_t buffert[0x10000];
    char namn[1024];
    snprintf(namn, sizeof namn, "%s", arg);
    int adress = adress_efter(namn, "-l fil@adr: ");
    size_t n = las_fil(namn, buffert, (size_t)(0x10000 - adress));
    maskin_krets(m, (uint16_t)adress, buffert, n);
}

static void banker(Maskin *m, const char *arg)
{
    uint8_t buffert[BANKSTORLEK];
    char namn[1024];
    snprintf(namn, sizeof namn, "%s", arg);
    int adress = adress_efter(namn, "-b fil,fil@adr: ");
    for (char *f = strtok(namn, ","); f; f = strtok(NULL, ",")) {
        memset(buffert, 0xFF, sizeof buffert);
        size_t n = las_fil(f, buffert, sizeof buffert);
        maskin_bank(m, (uint16_t)adress, buffert, n);
    }
}

/* CMOS-minnet (-M fil[@adr]): 2 KB, tomt (0) om filen inte finns än. */
#define CMOS_STORLEK 0x800
static char cmos_namn[1024];
static uint8_t *cmos_minne;

static void cmos_las(Maskin *m, const char *arg)
{
    uint8_t buffert[CMOS_STORLEK] = { 0 };
    int adress = 0x5000;
    snprintf(cmos_namn, sizeof cmos_namn, "%s", arg);
    if (strrchr(cmos_namn, '@'))
        adress = adress_efter(cmos_namn, "-M fil@adr: ");
    FILE *f = fopen(cmos_namn, "rb");
    if (f) {
        if (fread(buffert, 1, sizeof buffert, f)) {}
        fclose(f);
    }
    cmos_minne = maskin_cmos(m, (uint16_t)adress, buffert, sizeof buffert);
}

static void cmos_skriv(void)
{
    FILE *f = fopen(cmos_namn, "wb");
    if (!f || fwrite(cmos_minne, 1, CMOS_STORLEK, f) != CMOS_STORLEK)
        vard_fel("kan inte skriva CMOS-minnet till ", cmos_namn);
    fclose(f);
}

/* ROM:en ur en fil eller med ett av namnen 9913, 11273 och basicii80. */
static void rom(Vard *v, const char *arg)
{
    static uint8_t data[0x8001];
    char namn[1100];
    size_t storlek;
    if (!strcmp(arg, "9913") || !strcmp(arg, "11273")) {
        static const char *const krets_nummer[4] = { "06", "07", "08", "09" };
        storlek = 0;
        for (int k = 0; k < 4; k++) {
            snprintf(namn, sizeof namn, "%s../dis/roms/orig/ABC80-ZA35%s-%s.bin",
                     programmets_katalog, krets_nummer[k], arg);
            if (las_fil(namn, data + storlek, 0x1001) != 0x1000)
                vard_fel("kretsen ska vara 4 KB: ", namn);
            storlek += 0x1000;
        }
    } else {
        if (!strcmp(arg, "basicii80"))
            snprintf(namn, sizeof namn, "%s../basicii80/roms/basicii80.rom",
                     programmets_katalog);
        else
            snprintf(namn, sizeof namn, "%s", arg);
        storlek = las_fil(namn, data, sizeof data);
    }
    if (!maskin_rom(v->maskin, data, storlek))
        vard_fel("ROM:en ska vara 16 eller 32 KB: ", arg);
    v->basic_ii = storlek == 0x8000;
}

/* ---------------------------------------------------------------- */
/* Skivorna                                                          */

/* -D fil[@kort[.enhet]]: avbilden läses in i en buffert som kortet får. */
static void skiva(Vard *v, const char *arg, int skrivskydd)
{
    char *namn = strdup(arg), *at = strrchr(namn, '@');
    int kort = 44, enhet = 0;
    if (at) {
        *at = 0;
        kort = (int)strtol(at + 1, &at, 10);
        if (*at == '.')
            enhet = atoi(at + 1);
    }
    FILE *f = fopen(namn, "rb");
    if (!f)
        vard_fel("kan inte öppna ", namn);
    fseek(f, 0, SEEK_END);
    long storlek = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc((size_t)storlek + 256);
    if (!data || fread(data, 1, (size_t)storlek, f) != (size_t)storlek)
        vard_fel("kan inte läsa ", namn);
    fclose(f);
    v->skivfil[v->skivkort->antal_enheter] = namn;
    if (!skivkort_enhet(v->skivkort, kort, enhet, data, storlek, skrivskydd))
        vard_fel("högst 8 avbilder: ", namn);
}

/* -DQ s+oo=hh,...: ändra bytes i den senaste avbilden. */
static void andra_skiva(Vard *v, const char *arg)
{
    Skivkort *s = v->skivkort;
    if (!s->antal_enheter)
        vard_fel("-DQ kommer efter -D: ", arg);
    Skivenhet *e = &s->enhet[s->antal_enheter - 1];
    char *t;
    long sektor = strtol(arg, &t, 10);
    if (*t++ != '+')
        vard_fel("-DQ s+oo=hh,...: ", arg);
    long plats = sektor * 256 + strtol(t, &t, 16);
    if (*t++ != '=')
        vard_fel("-DQ s+oo=hh,...: ", arg);
    while (*t) {
        if (plats < 0 || plats >= e->sektorer * 256)
            vard_fel("-DQ utanför avbilden: ", arg);
        e->data[plats++] = (uint8_t)strtol(t, &t, 16);
        if (*t == ',')
            t++;
    }
}

/* ---------------------------------------------------------------- */

static void hjalp(void)
{
    fputs("användning: abc80 [-r rom] [-l fil@adr]... [-b f0,f1..@adr] [-m 16|32|48] [-M fil[@adr]]\n"
          "                  [-k text] [-f fil] [-K ms,ms] [-s text] [-S baud] [-W h@ms,..] [-L]\n"
          "                  [-D fil[@kort[.enhet]]].. [-C hh[,bb,n]] [-CR] [-E adr[,text]]\n"
          "                  [-T band.wav] [-U inspelning.wav]\n"
          "                  [-t ms [-d adr,n].. [-a fil.wav] [-x adr [-p ..] [-R ..]] ...]\n"
          "utan -t körs ABC80 i terminalen (Ctrl-C två gånger avslutar); se huvudet i vard/abc80.c\n",
          stderr);
    exit(1);
}

/* Flaggorna utan värde. */
static const char *const utan_varde[] = {
    "-L", "-DS", "-CR", "-EN", "-F", "-FD", "-c", "-V", "-N", "-w", "-v", NULL
};

static int har_varde(const char *flagga)
{
    for (int k = 0; utan_varde[k]; k++)
        if (!strcmp(flagga, utan_varde[k]))
            return 0;
    return 1;
}

int main(int argc, char **argv)
{
    static Maskin maskin;
    static Vard vard;
    static Skivkort skivkort;
    static Skrivarkort skrivarkort;
    static V24Skrivare v24_skrivare;
    static Ieckort ieckort;
    static Ljud ljud;
    static Kassett kassett;
    static char standard_rom[1100];
    Vard *v = &vard;
    const char *romfil = NULL, *v24 = NULL, *bankarg = NULL, *cmosfil = NULL;
    const char *kretsar[16];
    int antal_kretsar = 0, ram_kb = 16, utforlig = 0, utan_lyssnare = 0;

    const char *snedstreck = strrchr(argv[0], '/');
    if (snedstreck)
        snprintf(programmets_katalog, sizeof programmets_katalog, "%.*s",
                 (int)(snedstreck - argv[0] + 1), argv[0]);

    /* RAM-storleken behövs innan något läggs i maskinen. */
    for (int k = 1; k + 1 < argc; k++)
        if (!strcmp(argv[k], "-m"))
            ram_kb = atoi(argv[k + 1]);
    maskin_starta(&maskin, ram_kb);
    tangentko_paus(&maskin, 1000);           /* starten */
    skivkort_starta(&skivkort);
    v->maskin = &maskin;
    v->skivkort = &skivkort;
    v->efter_ms = -1;
    v->anrop_adress = -1;
    v->logga_port = -1;

    for (int k = 1; k < argc; k++) {
        const char *f = argv[k], *a = NULL;
        if (f[0] != '-' || !f[1])
            hjalp();
        if (har_varde(f)) {
            if (k + 1 >= argc)
                hjalp();
            a = argv[++k];
        }
        if (!strcmp(f, "-r")) romfil = a;
        else if (!strcmp(f, "-l")) { if (antal_kretsar < 16) kretsar[antal_kretsar++] = a; }
        else if (!strcmp(f, "-b")) bankarg = a;
        else if (!strcmp(f, "-m")) ;                    /* läst ovan */
        else if (!strcmp(f, "-M")) cmosfil = a;
        else if (!strcmp(f, "-k")) text(&maskin, a, 0); /* i ordning */
        else if (!strcmp(f, "-f")) textfil(&maskin, a);
        else if (!strcmp(f, "-K")) {
            if (sscanf(a, "%ld,%ld", &maskin.tryck_ms, &maskin.slapp_ms) != 2)
                hjalp();
        }
        else if (!strcmp(f, "-s")) v24 = a;
        else if (!strcmp(f, "-S")) maskin.baud = atol(a);
        else if (!strcmp(f, "-W")) labbtext(&maskin, a);
        else if (!strcmp(f, "-L")) {
            v24_skrivare_starta(&v24_skrivare);
            maskin.pio_b_enhet = &v24_skrivare.enhet;
            v->v24_skrivare = &v24_skrivare;
        }
        else if (!strcmp(f, "-D")) skiva(v, a, 0);
        else if (!strcmp(f, "-DP")) skiva(v, a, 1);
        else if (!strcmp(f, "-DS")) v->spara_skivor = 1;
        else if (!strcmp(f, "-DX")) { if (v->antal_visa_sektorer < 8) v->visa_sektorer[v->antal_visa_sektorer++] = a; }
        else if (!strcmp(f, "-DB") || !strcmp(f, "-DBW")) {
            long sektor = strtol(a, NULL, 0);
            if (skivkort.antal_trasiga < SKIVKORT_TRASIGA)
                skivkort.trasig[skivkort.antal_trasiga++] = f[3] ? -sektor - 1 : sektor;
        }
        else if (!strcmp(f, "-DH")) {
            char *slut;
            skivkort.hang_forsta = skivkort.hang_sista = (int)strtol(a, &slut, 10);
            if (*slut == ',')
                skivkort.hang_sista = atoi(slut + 1);
        }
        else if (!strcmp(f, "-DQ")) andra_skiva(v, a);
        else if (!strcmp(f, "-C")) {
            char *slut;
            int status = (int)strtol(a, &slut, 16), upptagen = 0, antal = 0;
            if (*slut == ',') {
                upptagen = (int)strtol(slut + 1, &slut, 16);
                if (*slut == ',')
                    antal = atoi(slut + 1);
            }
            skrivarkort_starta(&skrivarkort, (uint8_t)status, (uint8_t)upptagen, antal);
            v->skrivarkort = &skrivarkort;
        }
        else if (!strcmp(f, "-CR")) v->kortets_bildminne = 1;
        else if (!strcmp(f, "-E")) {
            char *slut;
            ieckort_starta(&ieckort, (int)strtol(a, &slut, 10));
            if (*slut == ',')
                ieckort_text(&ieckort, slut + 1);
            v->ieckort = &ieckort;
        }
        else if (!strcmp(f, "-EN")) utan_lyssnare = 1;  /* -E kan stå efter */
        else if (!strcmp(f, "-t")) v->efter_ms = atol(a);
        else if (!strcmp(f, "-d")) { if (v->antal_visa_minne < VARD_LISTA) v->visa_minne[v->antal_visa_minne++] = a; }
        else if (!strcmp(f, "-F")) v->tst = 1;
        else if (!strcmp(f, "-FD")) v->tst = v->tst_visa = 1;
        else if (!strcmp(f, "-x")) v->anrop_adress = strtol(a, NULL, 16);
        else if (!strcmp(f, "-p")) { if (v->antal_andringar < VARD_LISTA) v->andringar[v->antal_andringar++] = a; }
        else if (!strcmp(f, "-R")) v->register_text = a;
        else if (!strcmp(f, "-G")) v->stacken_vaxer = atoi(a);
        else if (!strcmp(f, "-c")) v->visa_anrop = 1;
        else if (!strcmp(f, "-V")) v->skarmen_ocksa = 1;
        else if (!strcmp(f, "-N")) maskin.utan_nmi = 1;
        else if (!strcmp(f, "-P")) { if (v->antal_bevakade < 8) v->bevakade[v->antal_bevakade++] = (uint16_t)strtol(a, NULL, 16); }
        else if (!strcmp(f, "-O")) v->logga_port = (int)strtol(a, NULL, 16);
        else if (!strcmp(f, "-w")) v->logga_skrivningar = 1;
        else if (!strcmp(f, "-v")) utforlig = 1;
        else if (!strcmp(f, "-i")) maskin.instruktioner_per_bild = atol(a);
        else if (!strcmp(f, "-a")) v->ljudfil = a;
        else if (!strcmp(f, "-T")) v->bandfil = a;
        else if (!strcmp(f, "-U")) v->inspelningsfil = a;
        else hjalp();
    }
    if (!romfil) {                           /* bredvid programmet */
        snprintf(standard_rom, sizeof standard_rom, "%s../disass/abc80/abc80new.rom",
                 programmets_katalog);
        romfil = standard_rom;
    }

    if (v->kortets_bildminne)
        maskin_ram(&maskin, 0x4000, 0x400, ' ');
    rom(v, romfil);
    maskin_aterstall(&maskin);
    for (int k = 0; k < antal_kretsar; k++)
        krets(&maskin, kretsar[k]);
    if (bankarg)
        banker(&maskin, bankarg);
    if (cmosfil)
        cmos_las(&maskin, cmosfil);
    if (v24)
        text(&maskin, v24, 1);
    if (v->tst && !v->basic_ii)
        vard_fel("-F går bara med BASIC II", NULL);
    if (skivkort.antal_enheter)
        bussen_koppla(&maskin.bussen, &skivkort.kort);
    if (v->skrivarkort)
        bussen_koppla(&maskin.bussen, &skrivarkort.kort);
    if (v->ieckort) {
        ieckort.utan_lyssnare = utan_lyssnare;
        bussen_koppla(&maskin.bussen, &ieckort.kort);
    }
    if (v->ljudfil) {
        /* Ett fast fro, så att samma körning ger samma ljud. */
        ljud_starta(&ljud, 22050, MASKIN_KLOCKA, 0x00A5C301);
        maskin.ljud = &ljud;
    }
    if (v->bandfil || v->inspelningsfil) {
        kassett_starta(&kassett);
        if (v->bandfil) {
            long antal;
            long long *flanker = kassett_las_wav(v->bandfil, &antal);
            kassett_spela(&kassett, flanker, antal);
        }
        if (v->inspelningsfil) {
            static long long inspelning[1 << 21];   /* 2 M flanker, ungefär 600 block */
            kassett_spela_in(&kassett, inspelning, 1 << 21);
        }
        maskin.kassett = v->kassett = &kassett;
    }
    anrop_krokar(v);

    /* Utan -t: terminalen, om det finns en; annars batchläget med
       500 ms efter tangenterna, som abc80host. Anropsläget är alltid
       ett batchläge. */
    if (v->efter_ms < 0 && (v->anrop_adress >= 0 || !isatty(0)))
        v->efter_ms = 500;
    if (v->efter_ms < 0) {
        if (v24)
            v24_starta(&maskin, 2 * MASKIN_KLOCKA);
        terminal_kor(&maskin);
    } else
        batch_kor(v);
    if (v->inspelningsfil) {
        long long slut = kassett_lage(&kassett, maskin.tid);
        if (kassett.langst > slut)
            slut = kassett.langst;
        kassett_skriv_wav(v->inspelningsfil, kassett.inspelning, kassett.antal_inspelat, slut);
        if (kassett.full)
            fprintf(stderr, "abc80: inspelningen fick inte plats; bara början är med\n");
    }
    if (cmosfil)
        cmos_skriv();
    if (utforlig)
        fprintf(stderr, "PC $%04X, %lld instruktioner, %lld T-cykler\n",
                maskin.z80.pc, maskin.instruktioner, maskin.tid);
    return 0;
}
