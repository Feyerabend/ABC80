/*
 * fonster.c -- ABC80 i ett eget fönster med SDL3, till boken ABC80 inifrån
 *
 * En värd till kärnan (../karna) som webbsidan, men som ett vanligt
 * program för Linux, Windows och macOS: skärmen i ett fönster (4:3),
 * tangentbordet och ljudet. Skärmen liknar en vit fosforskärm: varje
 * punkt har ett mjukt sken runt sig, det som släcks klingar av under
 * några hundradelar (efterlysningen), och runt bilden finns en svart
 * kant. Med -skarp blir punkterna skarpa rutor, utan sken.
 * Maskinen körs en bild (20 ms) i taget. Med ljudet följer tiden
 * ljudkortet, som på webbsidan; utan ljud följer den klockan. Medan
 * det finns kvar i tangentkön (-k, inklistrad text) går den så fort
 * datorn kan, utan ljud.
 *
 * Användning:
 *     abc80f [flaggor]
 *     -r fil         ROM:en, 16 KB eller 32 KB (BASIC II); utan -r
 *                    ../roms/abc80new.rom räknat från programmet
 *     -l fil@adr     en krets på adressen (hex); flera går bra
 *     -m kb          RAM: 16 (standard), 32 eller 48
 *     -tkn80 fil     TKN80 med EPROM:en TKN80-III (80 tecken per rad)
 *     -cpm fil.dsk   UNI-80 med start-PROM:en (../cpm/start.bin) och
 *                    systemskivan i FD2 enhet 0 (A:), som i abc80 -cpm;
 *                    som på webbsidan sitter ../cpm/b.dsk i B: (FD2
 *                    enhet 1) och ../cpm/d.dsk i D: (kort 44), om -D
 *                    inte redan har satt något där
 *     -D fil[@kort[.enhet]]  en skivavbild (kort 44, 45 eller 36)
 *     -DS            skriv tillbaka skivorna när fönstret stängs
 *     -T band.wav    ett band i bandspelaren
 *     -U fil.wav     spela in det som sparas på band, skrivs vid slutet
 *     -k text        tangenter efter starten (\r RETURN, \wN paus N ms)
 *     -z n           förstoring: fönstret är n x 352 x n x 264 (standard 3)
 *     -tyst          utan ljud
 *     -fast          fast markör (TKN80 med byglingen)
 *     -skarp         skarpa punkter, utan sken och efterlysning
 *
 * Tangenterna: som på webbsidan. RETURN, ← och → (backsteg är ←), ESC,
 * TAB, DEL och Ctrl-A till Ctrl-Z; å ä ö é ü och Å Ä Ö É Ü blir ABC80:s
 * koder. F12 är RESET. Cmd-V (macOS) eller Shift-Insert klistrar in
 * text, med en paus efter varje rad. Fönstret stängs som vanligt.
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "../karna/bild.h"
#include "../karna/kassett.h"
#include "../karna/ljud.h"
#include "../karna/maskin.h"
#include "../karna/skivkort.h"
#include "../karna/tecken.h"
#include "../vard/vard.h"

#define PROVTAKT   48000
#define LJUD_FORE  0.06                      /* så långt före ljudet körs vi, s */
#define FARG       240                       /* vit fosfor, #F0F0F0 */
#define KANT_X     32                        /* svart kant runt bilden */
#define KANT_Y     24
#define SB         (2 * BILD_BREDD + 2 * KANT_X)  /* fönstret: 704 x 528 */
#define SH         (2 * BILD_HOJD + 2 * KANT_Y)
#define EFTERLYS   0.020                     /* tidskonstanten, s */
#define SKEN       4                         /* skenets radie, punkter */
#define SKEN_STYRKA 0.8f
#define INSPELNING (1L << 21)                /* flanker, ungefär 600 block */

static Maskin maskin;
static Ljud ljud;
static Kassett kassett;
static Skivkort skivkort;
static Uni80 uni80;
static int cpm_lage;

static const char *skivfil[SKIVKORT_ENHETER];
static uint8_t *skivdata[SKIVKORT_ENHETER];
static long skivstorlek[SKIVKORT_ENHETER];
static int skivplats[SKIVKORT_ENHETER];    /* kort * 10 + enhet */
static int antal_skivor;

void vard_fel(const char *text, const char *tillagg)
{
    fprintf(stderr, "abc80f: %s%s\n", text, tillagg ? tillagg : "");
    exit(1);
}

/* Hela filen (malloc, med extra bytes över efter); storleken i *n. */
static uint8_t *las_fil(const char *namn, long *n, long extra)
{
    FILE *f = fopen(namn, "rb");
    if (!f)
        vard_fel("kan inte öppna ", namn);
    fseek(f, 0, SEEK_END);
    long storlek = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *data = malloc((size_t)(storlek + extra + 1));
    if (!data || fread(data, 1, (size_t)storlek, f) != (size_t)storlek)
        vard_fel("kan inte läsa ", namn);
    fclose(f);
    *n = storlek;
    return data;
}

/* ---------------------------------------------------------------- */
/* Tangenterna                                                       */

/* Ett tecken i UTF-8 till ABC80:s kod, eller 0. */
static int kod_for(const char **s)
{
    const uint8_t *p = (const uint8_t *)*s;
    if (p[0] < 0x80) {
        *s += 1;
        return p[0];
    }
    if (p[0] == 0xC3 && p[1]) {
        *s += 2;
        return tecken_svensk(p[1]);
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
 * flaggor gäller -k: \r, \wN, \xHH, \\. */
static void skriv(const char *s, int flaggor)
{
    while (*s) {
        if (flaggor && *s == '\\' && s[1]) {
            s++;
            if (*s == 'r') {
                tangentko_lagg(&maskin, 0x0D);
                s++;
            } else if (*s == 'w') {
                char *slut;
                tangentko_paus(&maskin, strtol(s + 1, &slut, 10));
                s = slut;
            } else if (*s == 'x') {
                char hex[3] = { s[1], s[1] ? s[2] : 0, 0 };
                tangentko_lagg(&maskin, (uint8_t)strtol(hex, NULL, 16));
                s += 1 + strlen(hex);
            } else
                tangentko_lagg(&maskin, (uint8_t)*s++);
            continue;
        }
        if (*s == '\r') {
            s++;
            continue;
        }
        if (*s == '\n') {
            tangentko_lagg(&maskin, 0x0D);
            tangentko_paus(&maskin, 200);
            s++;
            continue;
        }
        int kod = kod_for(&s);
        tangentko_lagg(&maskin, (uint8_t)(kod ? kod : '?'));
    }
}

/* RESET: processorn till startläget; CP/M startar om från skivan. */
static void aterstall(void)
{
    maskin_aterstall(&maskin);
    if (cpm_lage)
        uni80_karta(&uni80, UNI80_START);
}

/* En tangent som inte ger text. 0 om fönstret ska stängas. */
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
        tangentko_lagg(&maskin, (uint8_t)(k->key - SDLK_A + 1));
        return;
    }
    int kod = 0;
    switch (k->key) {
    case SDLK_RETURN: case SDLK_KP_ENTER: kod = 0x0D; break;
    case SDLK_BACKSPACE: case SDLK_LEFT:  kod = 0x08; break;
    case SDLK_RIGHT: case SDLK_TAB:       kod = 0x09; break;
    case SDLK_ESCAPE:                     kod = 0x1B; break;
    case SDLK_DELETE:                     kod = 0x7F; break;
    case SDLK_F12:                        aterstall(); return;
    default:                              return;
    }
    tangentko_lagg(&maskin, (uint8_t)kod);
}

/* Text från tangentbordet (det som har ett tecken). */
static void text_in(const char *s)
{
    if (SDL_GetModState() & (SDL_KMOD_CTRL | SDL_KMOD_GUI))
        return;                              /* tangent_ned tar dem */
    while (*s) {
        int kod = kod_for(&s);
        if (kod)
            tangentko_lagg(&maskin, (uint8_t)kod);
    }
}

/* ---------------------------------------------------------------- */
/* Fosforn                                                           */

static float lys[SB * SH], halv[SB * SH], sken[SB * SH];

/* Punkterna från bild_rita (1 = tänd; bredd 320 eller 640) till
 * fönstrets bild, 640 x 480 inom kanten. Det som tänds lyser direkt,
 * det som släcks avtar med EFTERLYS över tiden dt (s). Skenet är en
 * gaussisk oskärpa av det som lyser, lagd ovanpå. */
static void fosfor(const uint32_t *punkter, int bredd, double dt, int skarp, uint32_t *ut)
{
    float kvar = skarp ? 0.0f : (float)exp(-dt / EFTERLYS);
    int steg = 2 * BILD_BREDD / bredd;
    for (int y = 0; y < 2 * BILD_HOJD; y++) {
        const uint32_t *rad = punkter + y / 2 * bredd;
        float *l = lys + (y + KANT_Y) * SB + KANT_X;
        for (int x = 0; x < 2 * BILD_BREDD; x++) {
            float avtar = l[x] * kvar;
            l[x] = rad[x / steg] ? 1.0f : avtar;
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
        uint32_t c = (uint32_t)(FARG * (v > 1 ? 1 : v));
        ut[k] = 0xFF000000u | c << 16 | c << 8 | c;
    }
}

/* ---------------------------------------------------------------- */
/* Flaggorna                                                         */

static void skiva(const char *arg, int kort_standard, int enhet_standard)
{
    char *namn = SDL_strdup(arg), *at = strrchr(namn, '@');
    int kort = kort_standard, enhet = enhet_standard;
    if (at) {
        *at = 0;
        kort = (int)strtol(at + 1, &at, 10);
        if (*at == '.')
            enhet = atoi(at + 1);
    }
    long storlek;
    uint8_t *data = las_fil(namn, &storlek, 256);
    if (antal_skivor == SKIVKORT_ENHETER ||
        !skivkort_enhet(&skivkort, kort, enhet, data, storlek, 0))
        vard_fel("högst 8 avbilder: ", namn);
    skivfil[antal_skivor] = namn;
    skivplats[antal_skivor] = kort * 10 + enhet;
    skivdata[antal_skivor] = data;
    skivstorlek[antal_skivor++] = storlek;
}

/* En skiva som standard, om platsen är tom och filen finns. */
static void skiva_om_tom(const char *fil, int kort, int enhet)
{
    for (int k = 0; k < antal_skivor; k++)
        if (skivplats[k] == kort * 10 + enhet)
            return;
    FILE *f = fopen(fil, "rb");
    if (!f)
        return;
    fclose(f);
    char arg[2100];
    snprintf(arg, sizeof arg, "%s@%d.%d", fil, kort, enhet);
    skiva(arg, kort, enhet);
}

static void hjalp(void)
{
    fputs("användning: abc80f [-r rom] [-l fil@adr].. [-m 16|32|48] [-tkn80 fil] [-cpm system.dsk]\n"
          "                   [-D fil[@kort[.enhet]]].. [-DS] [-T band.wav] [-U fil.wav]\n"
          "                   [-k text] [-z n] [-tyst] [-fast] [-skarp]\n"
          "F12 är RESET; se huvudet i sdl/fonster.c\n", stderr);
    exit(1);
}

int main(int argc, char **argv)
{
    const char *bas = SDL_GetBasePath();     /* programmets katalog, med '/' */
    char namn[2048];
    const char *romfil = NULL, *tkn80fil = NULL, *cpmskiva = NULL;
    const char *bandfil = NULL, *inspelningsfil = NULL, *tangenter = NULL;
    const char *kretsar[16];
    int antal_kretsar = 0, ram_kb = 16, skala = 3, tyst = 0, fast = 0, spara = 0, skarp = 0;
    const char *skivor[SKIVKORT_ENHETER];
    int antal_skivargument = 0;

    for (int k = 1; k < argc; k++) {
        const char *f = argv[k];
        if (!strcmp(f, "-DS")) { spara = 1; continue; }
        if (!strcmp(f, "-tyst")) { tyst = 1; continue; }
        if (!strcmp(f, "-fast")) { fast = 1; continue; }
        if (!strcmp(f, "-skarp")) { skarp = 1; continue; }
        if (k + 1 >= argc)
            hjalp();
        const char *a = argv[++k];
        if (!strcmp(f, "-r")) romfil = a;
        else if (!strcmp(f, "-l")) { if (antal_kretsar < 16) kretsar[antal_kretsar++] = a; }
        else if (!strcmp(f, "-m")) ram_kb = atoi(a);
        else if (!strcmp(f, "-tkn80")) tkn80fil = a;
        else if (!strcmp(f, "-cpm")) cpmskiva = a;
        else if (!strcmp(f, "-D")) { if (antal_skivargument < SKIVKORT_ENHETER) skivor[antal_skivargument++] = a; }
        else if (!strcmp(f, "-T")) bandfil = a;
        else if (!strcmp(f, "-U")) inspelningsfil = a;
        else if (!strcmp(f, "-k")) tangenter = a;
        else if (!strcmp(f, "-z")) skala = atoi(a) > 0 ? atoi(a) : 1;
        else hjalp();
    }

    /* Maskinen, som i vard/abc80.c. */
    maskin_starta(&maskin, ram_kb);
    tangentko_paus(&maskin, 1000);           /* starten */
    if (!romfil) {
        snprintf(namn, sizeof namn, "%s../roms/abc80new.rom", bas ? bas : "");
        romfil = namn;
    }
    long storlek;
    uint8_t *data = las_fil(romfil, &storlek, 0);
    if (!maskin_rom(&maskin, data, (size_t)storlek))
        vard_fel("ROM:en ska vara 16 eller 32 KB: ", romfil);
    if (tkn80fil) {
        data = las_fil(tkn80fil, &storlek, 0);
        if (!maskin_tkn80(&maskin, data, (size_t)storlek))
            vard_fel("TKN80-III ska vara 4 KB och passa ROM:en (11273 eller 9913): ", tkn80fil);
    }
    skivkort_starta(&skivkort);
    if (cpmskiva) {
        snprintf(namn, sizeof namn, "%s../cpm/start.bin", bas ? bas : "");
        data = las_fil(namn, &storlek, 0);
        if (storlek > UNI80_STARTKRETS)
            vard_fel("start-PROM:en är större än 2 KB: ", namn);
        uni80_starta(&uni80);
        uni80_startkrets(&uni80, data, (size_t)storlek);
        maskin.uni80 = &uni80;
        skiva(cpmskiva, 45, 0);
        cpm_lage = 1;
    }
    for (int k = 0; k < antal_skivargument; k++)
        skiva(skivor[k], 44, 0);
    if (cpm_lage) {                          /* B: och D: som på webbsidan */
        snprintf(namn, sizeof namn, "%s../cpm/b.dsk", bas ? bas : "");
        skiva_om_tom(namn, 45, 1);
        snprintf(namn, sizeof namn, "%s../cpm/d.dsk", bas ? bas : "");
        skiva_om_tom(namn, 44, 0);
    }
    aterstall();
    for (int k = 0; k < antal_kretsar; k++) {
        char kopia[2048];
        snprintf(kopia, sizeof kopia, "%s", kretsar[k]);
        char *at = strrchr(kopia, '@');
        if (!at)
            vard_fel("-l fil@adr: ", kretsar[k]);
        *at = 0;
        data = las_fil(kopia, &storlek, 0);
        maskin_krets(&maskin, (uint16_t)strtol(at + 1, NULL, 16), data, (size_t)storlek);
    }
    if (antal_skivor)
        bussen_koppla(&maskin.bussen, &skivkort.kort);
    if (bandfil || inspelningsfil) {
        kassett_starta(&kassett);
        if (bandfil) {
            long antal;
            long long *flanker = kassett_las_wav(bandfil, &antal);
            kassett_spela(&kassett, flanker, antal);
        }
        if (inspelningsfil) {
            static long long inspelning[INSPELNING];
            kassett_spela_in(&kassett, inspelning, INSPELNING);
        }
        maskin.kassett = &kassett;
    }
    if (tangenter)
        skriv(tangenter, 1);

    /* Fönstret och ljudet. */
    if (!SDL_Init(SDL_INIT_VIDEO | (tyst ? 0 : SDL_INIT_AUDIO)))
        vard_fel("SDL: ", SDL_GetError());
    SDL_Window *fonster;
    SDL_Renderer *ritare;
    if (!SDL_CreateWindowAndRenderer("ABC80", skala * SB / 2, skala * SH / 2,
                                     SDL_WINDOW_RESIZABLE, &fonster, &ritare))
        vard_fel("SDL: ", SDL_GetError());
    SDL_SetRenderLogicalPresentation(ritare, SB, SH,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderVSync(ritare, 1);
    SDL_StartTextInput(fonster);

    SDL_AudioStream *strom = NULL;
    if (!tyst) {
        SDL_AudioSpec spec = { SDL_AUDIO_S16, 1, PROVTAKT };
        strom = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
        if (strom)
            SDL_ResumeAudioStreamDevice(strom);
        else
            fprintf(stderr, "abc80f: inget ljud (%s)\n", SDL_GetError());
    }
    ljud_starta(&ljud, PROVTAKT, MASKIN_KLOCKA, 0x00A5C301);
    maskin.ljud = &ljud;

    static uint32_t punkter[2 * BILD_BREDD * BILD_HOJD], farger[SB * SH];
    static int16_t prov[2048];
    SDL_Texture *bild = SDL_CreateTexture(ritare, SDL_PIXELFORMAT_ARGB8888,
                                          SDL_TEXTUREACCESS_STREAMING, SB, SH);
    if (!bild)
        vard_fel("SDL: ", SDL_GetError());
    SDL_SetTextureScaleMode(bild, skarp ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR);
    const double bildtid = 0.02;
    double nasta = 0, ritad = 0;
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

        /* En bild med ljudet: proven till strömmen, eller bort. */
        #define EN_BILD(med_ljud) do {                                    \
            maskin_kor(&maskin, MASKIN_BILD);                             \
            long long antal = ljud_prov_till(&ljud, maskin.tid) - ljud.prov; \
            while (antal > 0) {                                           \
                int n = antal > 2048 ? 2048 : (int)antal;                 \
                ljud_prov(&ljud, prov, n);                                \
                if ((med_ljud) && strom)                                  \
                    SDL_PutAudioStreamData(strom, prov, n * 2);           \
                antal -= n;                                               \
            }                                                             \
        } while (0)

        if (!tangentko_tom(&maskin)) {
            /* Tangentkön: så fort det går, 20 ms i taget, utan ljud. */
            while (!tangentko_tom(&maskin) && (double)SDL_GetTicksNS() * 1e-9 - nu < bildtid)
                EN_BILD(0);
            if (strom)
                SDL_ClearAudioStream(strom);
            nasta = 0;
        } else if (strom) {
            /* Ljudet bestämmer: håll LJUD_FORE sekunder i kö. */
            int fore = (int)(LJUD_FORE * PROVTAKT) * 2;
            int varv = 0;
            while (SDL_GetAudioStreamQueued(strom) < fore && varv++ < 25)
                EN_BILD(1);
        } else {
            /* Klockan bestämmer. */
            if (nasta == 0 || nu - nasta > 0.5)
                nasta = nu;
            while (nasta <= nu) {
                EN_BILD(0);
                nasta += bildtid;
            }
        }

        /* Skärmen. */
        int kolumner;
        const uint8_t *bildminne = maskin_skarm(&maskin, &kolumner);
        int markor = fast || (SDL_GetTicks() / 330) % 2 == 0;
        bild_rita(bildminne, kolumner, punkter, 1, 0, markor);
        double dt = ritad ? nu - ritad : 1;
        ritad = nu;
        fosfor(punkter, kolumner * 8, dt, skarp, farger);
        SDL_UpdateTexture(bild, NULL, farger, SB * 4);
        SDL_SetRenderDrawColor(ritare, 0, 0, 0, 255);
        SDL_RenderClear(ritare);
        SDL_RenderTexture(ritare, bild, NULL, NULL);
        SDL_RenderPresent(ritare);           /* väntar på skärmen (vsync) */
    }

    SDL_Quit();
    if (inspelningsfil) {
        long long slut_tid = kassett_lage(&kassett, maskin.tid);
        if (kassett.langst > slut_tid)
            slut_tid = kassett.langst;
        kassett_skriv_wav(inspelningsfil, kassett.inspelning, kassett.antal_inspelat, slut_tid);
        if (kassett.full)
            fprintf(stderr, "abc80f: inspelningen fick inte plats; bara början är med\n");
    }
    if (spara)
        for (int k = 0; k < antal_skivor; k++) {
            FILE *f = fopen(skivfil[k], "wb");
            if (!f || fwrite(skivdata[k], 1, (size_t)skivstorlek[k], f) != (size_t)skivstorlek[k])
                vard_fel("kan inte skriva ", skivfil[k]);
            fclose(f);
        }
    return 0;
}
