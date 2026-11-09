/*
 * fonster.c -- ABC 802 i ett eget fönster med SDL3, till ABC 802 inifrån
 *
 * En värd till kärnan (../karna) som batchläget (../vard/abc802.c), men
 * med skärmen i ett fönster (4:3), tangentbordet och pipet. Maskinen körs en bild (20 ms) i taget och
 * följer klockan. Medan det finns kvar i tangentkön (-k, -f, inklistrad
 * text) går den så fort datorn kan.
 *
 * Skärmen är bärnstensfärgad och liknar en fosforskärm: varje punkt
 * har ett mjukt sken runt sig, och det som släcks klingar av under
 * några hundradelar (efterlysningen), så att markören och det som
 * rullar drar en svans efter sig. Med -skarp blir punkterna skarpa
 * rutor utan sken och utan efterlysning.
 *
 * Pipet kommer från tangentbordets högtalare när datorn sänder kod 7 på
 * DART B. Tonen och längden är inte kända (tangentbordets 2716 finns
 * inte här); 1 kHz i 100 ms antas (?).
 *
 * Användning:
 *     abc802f [flaggor]
 *     -r fil         ROM:en, 32 KB; utan -r ../../roms/abc802rom.rom
 *                    räknat från programmet
 *     -c fil         teckengeneratorn; utan -c
 *                    ../../roms/orig/abc802/ABC802-char.6490191-01.bin
 *     -40            bygeln S3 på 40 tecken per rad
 *     -60            bygeln 50/60 Hz på 60 Hz
 *     -k text        tangenter efter starten (\r eller \n RETURN, \wN
 *                    paus N ms, \xHH en kod)
 *     -f fil         en textfil att skriva som tangenter (ett BASIC-program)
 *     -z n           förstoring: fönstret är n x 528 x n x 396 (standard 2)
 *     -tyst          utan pip
 *     -skarp         skarpa punkter, utan sken och efterlysning
 *     -farg          som om ABC 802 hade haft färg, som ABC 800 C (utanför
 *                    boken, se ../../utanfor/farg); skarpa punkter
 *     -D fil[@kort[.enhet]]  en skivavbild för DOS:en, standard kort 44
 *                    enhet 0 (MF0, DR0); kort 36 är hårddisken. Flera går bra
 *     -DP fil[@..]   samma, skrivskyddad
 *     -DS            skriv tillbaka skivorna när fönstret stängs
 *     -T fil         ett band i kassetten att spela upp (LOAD CAS:namn)
 *     -U fil         spela in på ett tomt band (SAVE CAS:namn); filen
 *                    skrivs när fönstret stängs
 *     -A fil, -B fil det som går ut på CH A (PR:) och CH B (V24:) till
 *                    filen, - = standard ut
 *     -a fil, -b fil filen in på CH A och CH B
 *     -P fil         skrivarkortet (kort 60) i ABC-bussen; det som skrivs
 *                    ut går till filen, - = standard ut (PR: väljer kortet
 *                    själv, ../karna/skrivarkort.h)
 *
 * Tangenterna: RETURN, ← och → (backsteg är ←), ESC, TAB, DEL och
 * Ctrl-A till Ctrl-Z; å ä ö é ü och Å Ä Ö É Ü blir ABC 802:s koder.
 * F12 är RESET. Cmd-V (macOS) eller Shift-Insert klistrar in text, med
 * en paus efter varje rad. Fönstret stängs som vanligt.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "../karna/abc802.h"
#include "../karna/skivkort.h"
#include "../karna/skrivarkort.h"

#define INSPELNING (1L << 22)                /* 4 MB band */
static FILE *kontakt_ut[2];
static uint8_t *kontakt_in[2];
static size_t kontakt_in_storlek[2], kontakt_in_skickat[2];

static void seriellt_ut(void *data, int kontakt, uint8_t tecken)
{
    (void)data;
    if (kontakt_ut[kontakt]) {
        fputc(tecken, kontakt_ut[kontakt]);
        fflush(kontakt_ut[kontakt]);
    }
}

static void fyll_kontakter(void);

#define PROVTAKT   48000
#define PIP_HZ     1000                      /* (?) */
#define PIP_MS     100                       /* (?) */
#define ROD        255                       /* bärnsten, #FFB000 */
#define GRON       176
#define BLA        0
#define KANT_X     48                        /* svart kant runt bilden */
#define KANT_Y     21                        /* 25 rader: 3 x 250 + 42 */
#define SB         (2 * ABC802_BREDD + 2 * KANT_X)  /* fönstret: 1056 x 792 */
#define SH         (3 * ABC802_HOJD + 2 * KANT_Y)
#define EFTERLYS   0.020                     /* tidskonstanten, s */
#define SKEN       5                         /* skenets radie, punkter */
#define SKEN_STYRKA 0.8f

static Abc802 maskin;

static void fel(const char *text, const char *tillagg)
{
    fprintf(stderr, "abc802f: %s%s\n", text, tillagg ? tillagg : "");
    exit(1);
}

/* Hela filen (malloc, med en nolla efter); storleken i *n. */
static uint8_t *las_fil(const char *namn, size_t *n)
{
    FILE *f = fopen(namn, "rb");
    if (!f)
        fel("kan inte öppna ", namn);
    fseek(f, 0, SEEK_END);
    long storlek = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc((size_t)storlek + 1);
    if (!data || fread(data, 1, (size_t)storlek, f) != (size_t)storlek)
        fel("kan inte läsa ", namn);
    fclose(f);
    data[storlek] = 0;
    *n = (size_t)storlek;
    return data;
}

/* ---------------------------------------------------------------- */
/* Skivorna                                                          */

static Skivkort skivkort;
static Skrivarkort skrivarkort;
static FILE *skrivarfil;

static void skrivaren(void *data, uint8_t tecken)
{
    (void)data;
    fputc(tecken, skrivarfil);
    fflush(skrivarfil);
}
static const char *skivfil[SKIVKORT_ENHETER];

/* -D fil[@kort[.enhet]], som i vard/abc802.c. */
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
    uint8_t *data = las_fil(namn, &storlek);
    skivfil[skivkort.antal_enheter] = namn;
    if (!skivkort_enhet(&skivkort, kort, enhet, data, (long)storlek, skrivskydd))
        fel("för många skivor: ", namn);
}

static void spara_skivor(void)
{
    for (int k = 0; k < skivkort.antal_enheter; k++) {
        FILE *f = fopen(skivfil[k], "wb");
        if (!f)
            fel("kan inte skriva ", skivfil[k]);
        fwrite(skivkort.enhet[k].data, 256, (size_t)skivkort.enhet[k].sektorer, f);
        fclose(f);
    }
}

/* ---------------------------------------------------------------- */
/* Tangenterna                                                       */

/* Den andra byten i Å Ä Ö É Ü å ä ö é ü (UTF-8, $C3 först) till
 * ABC 802:s kod, eller 0. */
static int svensk(uint8_t andra)
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

/* Ett tecken i UTF-8 till ABC 802:s kod, eller 0. */
static int kod_for(const char **s)
{
    const uint8_t *p = (const uint8_t *)*s;
    if (p[0] < 0x80) {
        *s += 1;
        return p[0];
    }
    if (p[0] == 0xC3 && p[1]) {
        *s += 2;
        return svensk(p[1]);
    }
    if (p[0] == 0xC2 && p[1] == 0xA4) {      /* ¤ */
        *s += 2;
        return 0x24;
    }
    do                                       /* hoppa över tecknet */
        (*s)++;
    while ((**s & 0xC0) == 0x80);
    return 0;
}

/* Text som tangenter. Radslut blir RETURN med en paus på 200 ms; med
 * flaggor gäller -k: \r, \n, \wN, \xHH, \\. */
static void skriv(const char *s, int flaggor)
{
    while (*s) {
        if (flaggor && *s == '\\' && s[1]) {
            s++;
            if (*s == 'r' || *s == 'n') {
                abc802_tangent(&maskin, 0x0D);
                s++;
            } else if (*s == 'w') {
                char *slut;
                abc802_paus(&maskin, strtol(s + 1, &slut, 10));
                s = slut;
            } else if (*s == 'x') {
                char hex[3] = { s[1], s[1] ? s[2] : 0, 0 };
                abc802_tangent(&maskin, (uint8_t)strtol(hex, NULL, 16));
                s += 1 + strlen(hex);
            } else
                abc802_tangent(&maskin, (uint8_t)*s++);
            continue;
        }
        if (*s == '\r') {
            s++;
            continue;
        }
        if (*s == '\n') {
            abc802_tangent(&maskin, 0x0D);
            abc802_paus(&maskin, 200);
            s++;
            continue;
        }
        int kod = kod_for(&s);
        abc802_tangent(&maskin, (uint8_t)(kod ? kod : '?'));
    }
}

/* En tangent som inte ger text. */
static void tangent_ned(const SDL_KeyboardEvent *k)
{
    SDL_Keymod mod = k->mod;
    if (((mod & SDL_KMOD_GUI) && k->key == SDLK_V) ||
        ((mod & SDL_KMOD_SHIFT) && k->key == SDLK_INSERT)) {
        char *text = SDL_GetClipboardText();
        if (text)
            skriv(text, 0);
        SDL_free(text);
        return;
    }
    if (mod & SDL_KMOD_GUI)
        return;                              /* Cmd-Q och de andra */
    if ((mod & SDL_KMOD_CTRL) && k->key >= SDLK_A && k->key <= SDLK_Z) {
        abc802_tangent(&maskin, (uint8_t)(k->key - SDLK_A + 1));
        return;
    }
    int kod = 0;
    switch (k->key) {
    case SDLK_RETURN: case SDLK_KP_ENTER: kod = 0x0D; break;
    case SDLK_BACKSPACE: case SDLK_LEFT:  kod = 0x08; break;
    case SDLK_RIGHT: case SDLK_TAB:       kod = 0x09; break;
    case SDLK_ESCAPE:                     kod = 0x1B; break;
    case SDLK_DELETE:                     kod = 0x7F; break;
    case SDLK_F12:                        abc802_aterstall(&maskin); return;
    default:                              return;
    }
    abc802_tangent(&maskin, (uint8_t)kod);
}

/* Text från tangentbordet (det som har ett tecken). */
static void text_in(const char *s)
{
    if (SDL_GetModState() & (SDL_KMOD_CTRL | SDL_KMOD_GUI))
        return;                              /* tangent_ned tar dem */
    while (*s) {
        int kod = kod_for(&s);
        if (kod)
            abc802_tangent(&maskin, (uint8_t)kod);
    }
}

/* ---------------------------------------------------------------- */

/* ---------------------------------------------------------------- */
/* Fosforn                                                           */

static float lys[SB * SH], halv[SB * SH], sken[SB * SH];

/* Punkterna (480 x 250) till fönstrets bild (960 x 750): varje punkt
 * blir 2 x 3, och runt bilden finns en svart kant (KANT_X, KANT_Y) som
 * skenet kan nå ut i. Det som tänds lyser direkt, det som släcks avtar med
 * EFTERLYS över tiden dt (s). Skenet är en gaussisk oskärpa av det
 * som lyser, lagd ovanpå. */
static void fosfor(const uint8_t *punkter, double dt, int skarp, uint32_t *ut)
{
    float kvar = skarp ? 0.0f : (float)exp(-dt / EFTERLYS);
    for (int y = 0; y < 3 * ABC802_HOJD; y++) {
        const uint8_t *rad = punkter + y / 3 * ABC802_BREDD;
        float *l = lys + (y + KANT_Y) * SB + KANT_X;
        for (int x = 0; x < 2 * ABC802_BREDD; x++) {
            float avtar = l[x] * kvar;
            l[x] = rad[x / 2] ? 1.0f : avtar;
        }
    }

    if (!skarp) {
        float vikt[SKEN + 1], summa = 0;
        for (int k = 0; k <= SKEN; k++) {
            float d = (float)k / (SKEN / 2.0f);
            vikt[k] = expf(-d * d / 2);
            summa += k ? 2 * vikt[k] : vikt[k];
        }
        for (int k = 0; k <= SKEN; k++)
            vikt[k] /= summa;
        for (int y = 0; y < SH; y++) {       /* vågrätt */
            const float *l = lys + y * SB;
            float *h = halv + y * SB;
            for (int x = 0; x < SB; x++) {
                float v = vikt[0] * l[x];
                for (int k = 1; k <= SKEN; k++)
                    v += vikt[k] * ((x >= k ? l[x - k] : 0) + (x + k < SB ? l[x + k] : 0));
                h[x] = v;
            }
        }
        for (int y = 0; y < SH; y++)         /* lodrätt */
            for (int x = 0; x < SB; x++) {
                float v = vikt[0] * halv[y * SB + x];
                for (int k = 1; k <= SKEN; k++)
                    v += vikt[k] * ((y >= k ? halv[(y - k) * SB + x] : 0)
                                    + (y + k < SH ? halv[(y + k) * SB + x] : 0));
                sken[y * SB + x] = v;
            }
    }

    for (int k = 0; k < SB * SH; k++) {
        float v = lys[k] + (skarp ? 0 : SKEN_STYRKA * sken[k]);
        if (v > 1)
            v = 1;
        ut[k] = 0xFF000000u | (uint32_t)(ROD * v) << 16 | (uint32_t)(GRON * v) << 8
              | (uint32_t)(BLA * v);
    }
}

/* ---------------------------------------------------------------- */
/* Färgbilden (-farg): färgerna 0-7 rakt ut, skarpt, utan fosfor. */
static void fargbild(const uint8_t *punkter, uint32_t *ut)
{
    static const uint32_t farger[8] = {
        0x000000, 0xFF2828, 0x28E628, 0xFFF028, 0x465AFF, 0xFF28FF, 0x28F0F0, 0xFFFFFF
    };
    for (int k = 0; k < SB * SH; k++)
        ut[k] = 0xFF000000u;
    for (int y = 0; y < 3 * ABC802_HOJD; y++) {
        const uint8_t *rad = punkter + y / 3 * ABC802_BREDD;
        uint32_t *l = ut + (y + KANT_Y) * SB + KANT_X;
        for (int x = 0; x < 2 * ABC802_BREDD; x++)
            l[x] = 0xFF000000u | farger[rad[x / 2] & 7];
    }
}

static void hjalp(void)
{
    fputs("användning: abc802f [-r rom] [-c teckenrom] [-40] [-60] [-k text] [-f fil]\n"
          "                    [-z n] [-tyst] [-skarp] [-farg] [-D fil[@kort[.enhet]]].. [-DP fil] [-DS]\n"
          "                    [-T fil] [-U fil] [-A fil] [-B fil] [-a fil] [-b fil]\n"
          "                    [-P fil]\n"
          "F12 är RESET; se huvudet i sdl/fonster.c\n", stderr);
    exit(1);
}

int main(int argc, char **argv)
{
    const char *bas = SDL_GetBasePath();     /* programmets katalog, med '/' */
    char romnamn[2048], teckennamn[2048];
    const char *romfil = NULL, *teckenfil = NULL;
    const char *tangenter[64];
    int ar_fil[64], antal = 0, skala = 2, tyst = 0, skarp = 0, farg = 0, spara = 0;
    const char *bandfil = NULL, *inspelningsfil = NULL;
    uint8_t *inspelning = NULL;

    abc802_starta(&maskin);
    skivkort_starta(&skivkort);
    for (int k = 1; k < argc; k++) {
        const char *f = argv[k];
        if (!strcmp(f, "-40")) { maskin.bygel_80 = 0; continue; }
        if (!strcmp(f, "-60")) { maskin.bygel_60hz = 1; continue; }
        if (!strcmp(f, "-tyst")) { tyst = 1; continue; }
        if (!strcmp(f, "-skarp")) { skarp = 1; continue; }
        if (!strcmp(f, "-farg")) { farg = 1; continue; }
        if (!strcmp(f, "-DS")) { spara = 1; continue; }
        if (k + 1 >= argc)
            hjalp();
        const char *a = argv[++k];
        if (!strcmp(f, "-r")) romfil = a;
        else if (!strcmp(f, "-c")) teckenfil = a;
        else if ((!strcmp(f, "-k") || !strcmp(f, "-f")) && antal < 64) {
            ar_fil[antal] = f[1] == 'f';
            tangenter[antal++] = a;
        } else if (!strcmp(f, "-z")) skala = atoi(a) > 0 ? atoi(a) : 1;
        else if (!strcmp(f, "-D")) skiva(a, 0);
        else if (!strcmp(f, "-DP")) skiva(a, 1);
        else if (!strcmp(f, "-T")) bandfil = a;
        else if (!strcmp(f, "-U")) inspelningsfil = a;
        else if (!strcmp(f, "-A") || !strcmp(f, "-B")) {
            int kontakt = f[1] == 'B';
            kontakt_ut[kontakt] = !strcmp(a, "-") ? stdout : fopen(a, "wb");
            if (!kontakt_ut[kontakt])
                fel("kan inte skriva ", a);
        } else if (!strcmp(f, "-a") || !strcmp(f, "-b")) {
            int kontakt = f[1] == 'b';
            kontakt_in[kontakt] = las_fil(a, &kontakt_in_storlek[kontakt]);
        } else if (!strcmp(f, "-P")) {
            skrivarfil = !strcmp(a, "-") ? stdout : fopen(a, "wb");
            if (!skrivarfil)
                fel("kan inte skriva ", a);
        } else hjalp();
    }

    /* Maskinen, som i vard/abc802.c. */
    if (!romfil) {
        snprintf(romnamn, sizeof romnamn, "%s../../roms/abc802rom.rom", bas ? bas : "");
        romfil = romnamn;
    }
    if (!teckenfil) {
        snprintf(teckennamn, sizeof teckennamn,
                 "%s../../roms/orig/abc802/ABC802-char.6490191-01.bin", bas ? bas : "");
        teckenfil = teckennamn;
    }
    size_t storlek;
    uint8_t *data = las_fil(romfil, &storlek);
    if (!abc802_rom(&maskin, data, storlek))
        fel("ROM:en ska vara 32 KB: ", romfil);
    free(data);
    data = las_fil(teckenfil, &storlek);
    if (!abc802_teckenrom(&maskin, data, storlek))
        fel("teckengeneratorn ska vara 4 eller 8 KB: ", teckenfil);
    free(data);
    if (skivkort.antal_enheter)
        bussen_koppla(&maskin.bussen, &skivkort.kort);
    if (skrivarfil) {
        skrivarkort_starta(&skrivarkort);
        skrivarkort.tecken = skrivaren;
        bussen_koppla(&maskin.bussen, &skrivarkort.kort);
    }
    if (bandfil) {
        data = las_fil(bandfil, &storlek);
        kassett802_spela(&maskin.kassett, data, (long)storlek);
    }
    if (inspelningsfil) {
        inspelning = malloc(INSPELNING);
        kassett802_spela_in(&maskin.kassett, inspelning, INSPELNING);
    }
    maskin.seriellt_ut = seriellt_ut;
    abc802_aterstall(&maskin);
    abc802_paus(&maskin, 1000);              /* starten */
    for (int k = 0; k < antal; k++) {
        if (!ar_fil[k]) {
            skriv(tangenter[k], 1);
            continue;
        }
        data = las_fil(tangenter[k], &storlek);
        skriv((const char *)data, 0);
        free(data);
    }

    /* Fönstret och ljudet. Punkterna är dubbelt så höga som breda
     * (480 x 250 på en 4:3-skärm); bilden ritas 960 x 750. */
    if (!SDL_Init(SDL_INIT_VIDEO | (tyst ? 0 : SDL_INIT_AUDIO)))
        fel("SDL: ", SDL_GetError());
    SDL_Window *fonster;
    SDL_Renderer *ritare;
    if (!SDL_CreateWindowAndRenderer("ABC 802", skala * SB / 2, skala * SH / 2,
                                     SDL_WINDOW_RESIZABLE, &fonster, &ritare))
        fel("SDL: ", SDL_GetError());
    SDL_SetRenderLogicalPresentation(ritare, SB, SH,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(ritare, 1);
    SDL_StartTextInput(fonster);
    SDL_Texture *bild = SDL_CreateTexture(ritare, SDL_PIXELFORMAT_ARGB8888,
                                          SDL_TEXTUREACCESS_STREAMING, SB, SH);
    if (!bild)
        fel("SDL: ", SDL_GetError());
    SDL_SetTextureScaleMode(bild, skarp || farg ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);

    SDL_AudioStream *strom = NULL;
    static int16_t pip[PROVTAKT * PIP_MS / 1000];
    if (!tyst) {
        SDL_AudioSpec spec = { SDL_AUDIO_S16, 1, PROVTAKT };
        strom = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
        if (strom)
            SDL_ResumeAudioStreamDevice(strom);
        else
            fprintf(stderr, "abc802f: inget ljud (%s)\n", SDL_GetError());
        for (int k = 0; k < (int)(sizeof pip / sizeof pip[0]); k++)
            pip[k] = (k * 2 * PIP_HZ / PROVTAKT) % 2 ? -6000 : 6000;
    }

    static uint8_t punkter[ABC802_BREDD * ABC802_HOJD];
    static uint32_t farger[SB * SH];
    const double bildtid = 0.02;
    double nasta = 0, ritad = 0;
    long pipen = 0;
    int slut = 0;

    while (!slut) {
        SDL_Event h;
        while (SDL_PollEvent(&h)) {
            if (h.type == SDL_EVENT_QUIT)
                slut = 1;
            else if (h.type == SDL_EVENT_KEY_DOWN)
                tangent_ned(&h.key);
            else if (h.type == SDL_EVENT_TEXT_INPUT)
                text_in(h.text.text);
        }
        double nu = (double)SDL_GetTicksNS() * 1e-9;
        fyll_kontakter();

        if (!abc802_ko_tom(&maskin)) {
            /* Tangentkön: så fort det går, 20 ms i taget. */
            while (!abc802_ko_tom(&maskin) && (double)SDL_GetTicksNS() * 1e-9 - nu < bildtid)
                abc802_kor(&maskin, ABC802_MS(20));
            nasta = 0;
        } else {
            /* Klockan bestämmer. */
            if (nasta == 0 || nu - nasta > 0.5)
                nasta = nu;
            while (nasta <= nu) {
                abc802_kor(&maskin, ABC802_MS(20));
                nasta += bildtid;
            }
        }

        /* Summern (IN 5): det som har låtit sedan förra varvet, ett varv
         * fördröjt. ROM:ens BEEP (CHR$(7)) slår om summern 200 gånger och
         * skickar sedan kod 7 till tangentbordet; kommer ett pip kastas
         * summern från det och förra varvet, så att BEEP bara blir pipet. */
        {
            static float prov[PROVTAKT];
            static int16_t vantar[PROVTAKT];
            static int vantar_n = 0;
            int n = abc802_summer(&maskin, PROVTAKT, prov, PROVTAKT);
            if (maskin.pip != pipen) {
                /* Pipet: ett i taget, och inget som hopats medan kön gick. */
                vantar_n = 0;
                if (strom && SDL_GetAudioStreamQueued(strom) == 0)
                    SDL_PutAudioStreamData(strom, pip, sizeof pip);
                pipen = maskin.pip;
            } else {
                if (strom && vantar_n > 0)
                    SDL_PutAudioStreamData(strom, vantar, vantar_n * (int)sizeof vantar[0]);
                for (int k = 0; k < n; k++)
                    vantar[k] = (int16_t)(prov[k] * 6000);
                vantar_n = n;
            }
        }

        /* Skärmen. */
        double dt = ritad ? nu - ritad : 1;
        ritad = nu;
        if (farg) {
            abc802_fargbild(&maskin, punkter);
            fargbild(punkter, farger);
        } else {
            abc802_bild(&maskin, punkter);
            fosfor(punkter, dt, skarp, farger);
        }
        SDL_UpdateTexture(bild, NULL, farger, SB * 4);
        SDL_SetRenderDrawColor(ritare, 0, 0, 0, 255);
        SDL_RenderClear(ritare);
        SDL_RenderTexture(ritare, bild, NULL, NULL);
        SDL_RenderPresent(ritare);           /* väntar på skärmen (vsync) */
    }

    SDL_Quit();
    if (spara)
        spara_skivor();
    if (inspelningsfil) {
        FILE *f = fopen(inspelningsfil, "wb");
        if (!f)
            fel("kan inte skriva ", inspelningsfil);
        fwrite(inspelning, 1, (size_t)maskin.kassett.antal_inspelat, f);
        fclose(f);
    }
    for (int k = 0; k < 2; k++)
        if (kontakt_ut[k] && kontakt_ut[k] != stdout)
            fclose(kontakt_ut[k]);
    return 0;
}

/* Köerna till CH A och CH B fylls ur filerna (-a, -b). */
static void fyll_kontakter(void)
{
    for (int k = 0; k < 2; k++)
        while (kontakt_in_skickat[k] < kontakt_in_storlek[k]
               && abc802_seriellt_in(&maskin, k, kontakt_in[k][kontakt_in_skickat[k]]))
            kontakt_in_skickat[k]++;
}
