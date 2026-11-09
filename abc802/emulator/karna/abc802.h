/*
 * abc802.h -- ABC 802 runt processorn, till ABC 802 inifrån
 *
 * Kärnan i emulatorn för ABC 802: minnet med ROM:en, RAM:et och
 * bildminnet, kretsarna DART, SIO/2, CTC och 6845, tangentbordet och
 * avbrottskedjan, med tiden räknad i processorns T-cykler. Som för
 * ABC80 (maskin.h) gör kärnan ingen in- eller utmatning själv.
 *
 * Uppgifterna kommer ur servicemanualen (230 8088-15) och kretsschemat
 * där, sammanfattade i ../../maskin/MASKINEN.md, och ur ROM:en
 * (../../dis/tools/abc802_notes.py):
 *     klockan      3 MHz (12 MHz / 4); 6845 får 2 MHz, CTC kanal 0-2
 *                  1,5 MHz på CLK/TRG
 *     minnet       ROM $0000-$7FFF, 64 KB RAM, bildminnet 2 KB på
 *                  $7800-$7FFF. Låskretsen 7D minns om den senaste
 *                  instruktionshämtningen låg i $7800-$7FFF; då syns
 *                  RAM:et på $0000-$77FF och ROM:en på $7800-$7FFF (LDIR7F
 *                  på $7FFD). Annars syns ROM:en på $0000-$77FF och
 *                  bildminnet på $7800-$7FFF (som data). DTR på DART B
 *                  (LRS*) ger RAM överallt. Skrivningar dit ROM:en syns
 *                  försvinner (ROME* stänger av RAM:et).
 *     portar       $20-$23 DART, $38-$3F 6845 (jämn = register, udda =
 *                  data; A3 = 0 läser, $31 = data), $40-$43 SIO/2, $60-$63 CTC; OUT $00-$05 och
 *                  IN $00-$02 går till korten på ABC-bussen (bussen.h),
 *                  resten läses $FF
 *     avbrotten    kedjan CTC, SIO/2, DART, i läge 2
 *     DART A RI    bygeln S3: 1 = 80 tecken
 *     DART B       tangentbordet; DCD = en tangent är nere (Key Down*),
 *                  CTS = bygeln 50/60 Hz (1 = 60 Hz), RTS = 40 tecken
 *                  (MUX 80/40), DTR = 64 KB RAM (LRS*)
 *     SIO B DCD    bygeln S1, skärmsläckaren (1 = på)
 *     SIO B        kassetten (kassett802.h); DTR drar motorns relä
 *     CH A, CH B   kontakterna för V24 och skrivaren: DART A (PR:) med
 *                  klockan från CTC kanal 2, och SIO A (V24:) som sänder
 *                  med CTC kanal 0 och tar emot med kanal 1 (ROM:en på
 *                  $7209, abc802_notes.py). Kanal 0 ger också kassettens
 *                  klocka (?). Ett tecken tar så lång tid som WR3-WR5 och
 *                  CTC:n säger. DCD och CTS är på, som med en ansluten
 *                  skrivare eller terminal.
 *     tangentbordet  sänder koden på DART B, 1 startbit, 8 databitar och
 *                  2 stoppbitar; 19 200 baud (?) antas. Det som sänds
 *                  till det, kod 7, är ett pip.
 */

#ifndef ABC802_H
#define ABC802_H

#include <stddef.h>
#include <stdint.h>

#include "bussen.h"
#include "ctc.h"
#include "kassett802.h"
#include "sio.h"
#include "z80.h"

#define ABC802_KLOCKA      3000000L              /* T-cykler per sekund */
#define ABC802_SUMMER_MAX 8192              /* omslag mellan två hämtningar */
#define ABC802_MS(ms)      ((long long)(ms) * ABC802_KLOCKA / 1000)
#define ABC802_BILDMINNE   0x800
#define ABC802_TECKENROM   0x1000               /* de första 4 KB av 8 */
#define ABC802_TANGENTKO   65536
#define ABC802_SERIELL_KO  16384

/* Kontakterna på baksidan. */
enum { ABC802_CH_A, ABC802_CH_B };

typedef struct Abc802 Abc802;

/* 6845: adressregistret och R0-R17. */
typedef struct Crtc {
    int adress;
    uint8_t r[18];
} Crtc;

struct Abc802 {
    Z80 z80;
    uint8_t rom[0x8000];
    uint8_t ram[0x10000];
    uint8_t bildminne[ABC802_BILDMINNE];
    uint8_t teckenrom[ABC802_TECKENROM];
    int hamtning_7800;                       /* låskretsen 7D */
    long long tid;                           /* T-cykler sedan start */
    long long instruktioner;

    Sio dart, sio;
    Ctc ctc;
    Crtc crtc;
    Bussen bussen;                           /* värden kopplar in korten */

    /* Byglarna (läses vid start av CRTINI och KEYWT). */
    int bygel_80, bygel_60hz, bygel_slackare;

    /* Tangentkön: en kod 0-255, eller 256 + ms för en paus. */
    int tangentko[ABC802_TANGENTKO];
    int ko_forst, ko_sist;
    long long ko_nasta;                      /* när kön går vidare */
    int ko_steg;                             /* 0 vilar, 1 nere, 2 släppt */
    long long tecken_framme;                 /* -1, eller när koden är framme */
    uint8_t tecken;
    long tryck_ms, slapp_ms;

    long pip;                                /* antal pip sedan start */

    /* Summern: vippan (74LS390, 4E) slås om varje gång port 5 läses.
     * Tiderna (T-cykler) sedan abc802_summer senast; värden gör ljud av
     * dem. */
    int summer;                              /* vippans läge */
    long summer_omslag;                      /* antal omslag sedan start */
    int summer_fore;                         /* läget vid summer_fran */
    long long summer_fran;
    int summer_antal;
    long long summer_tid[ABC802_SUMMER_MAX];

    Kassett802 kassett;

    /* CH A och CH B: det som kommer in (värden lägger det i kön) och
     * det som går ut (till värden). Har maskinen skickat XOFF väntar
     * kön tills XON kommer, som hos en avsändare med XON/XOFF. */
    uint8_t seriell_ko[2][ABC802_SERIELL_KO];
    int seriell_forst[2], seriell_sist[2];
    long long seriell_nasta[2];
    int seriell_xoff[2];
    int seriell_pa[2];                       /* mottagaren var på */
    long mottagning_tid[2];                  /* T-cykler per tecken in */
    void *seriell_data;
    void (*seriellt_ut)(void *data, int kontakt, uint8_t tecken);

    /* En krok i ROM:en: när processorn ska hämta en instruktion på
     * krok_pc och ROM:en syns där anropas krok i stället. Den får ändra
     * registren och minnet (pc också) och lämnar antalet T-cykler, eller
     * 0 om instruktionen ska köras som vanligt. Nodkortet (natkort.h)
     * använder den. */
    int (*krok)(void *data, Abc802 *m);
    void *krok_data;
    uint16_t krok_pc;
};

/* Minnet som processorn ser det just nu. */
uint8_t abc802_las(Abc802 *m, uint16_t adress);
void    abc802_skriv(Abc802 *m, uint16_t adress, uint8_t varde);

/* Töm maskinen. Byglarna: 80 tecken, 50 Hz, ingen skärmsläckare.
 * Tangenterna hålls nere 40 ms och släppta 60 ms. */
void abc802_starta(Abc802 *m);

/* ROM:en (32 KB) och teckengeneratorn (4 KB, eller de första 4 KB av
 * 8). 0 om storleken är fel. */
int abc802_rom(Abc802 *m, const uint8_t *data, size_t storlek);
int abc802_teckenrom(Abc802 *m, const uint8_t *data, size_t storlek);

/* Processorn och kretsarna till startläget (RESET). */
void abc802_aterstall(Abc802 *m);

/* Kör minst tcykler T-cykler. */
void abc802_kor(Abc802 *m, long long tcykler);

/* Tangentkön. tangentko_tom är sann när kön och tangenten är klara. */
void abc802_tangent(Abc802 *m, uint8_t kod);
void abc802_paus(Abc802 *m, long ms);
int  abc802_ko_tom(const Abc802 *m);

/* Ett tecken in på CH A eller CH B. 0 om kön är full. */
int abc802_seriellt_in(Abc802 *m, int kontakt, uint8_t tecken);
/* Tecken som väntar i kön. */
int abc802_seriellt_vantar(const Abc802 *m, int kontakt);

/* Kassettens motor (DTR på SIO B). */
int abc802_motor(const Abc802 *m);

/* 40 tecken per rad (RTS på DART B). */
int abc802_fyrtio(const Abc802 *m);

/* Summern sedan förra anropet som prov (takt per sekund), -1 eller 1,
 * högst max. 0 om vippan inte har slagits om (då är det tyst). */
int abc802_summer(Abc802 *m, int takt, float *ut, int max);

/* Bilden: bredd x höjd punkter, en byte per punkt (1 = tänd). */
#define ABC802_BREDD  480                    /* 80 tecken à 6 punkter */
#define ABC802_HOJD   250                    /* 25 rader à 10 linjer: R6 är
                                              * 24, men kan vara 25 */
void abc802_bild(const Abc802 *m, uint8_t *punkter);
/* Samma bild, men som om ABC 802 hade haft färg (utanför boken, se
 * ../utanfor/farg): koderna 1-7 och 17-23 ger förgrunden och 28-29
 * bakgrunden som i ABC 800 C. En byte per punkt är färgen 0-7 (svart,
 * röd, grön, gul, blå, magenta, cyan, vit). Maskinen själv kan inte
 * visa färg; teckengeneratorn tar bort den. */
void abc802_fargbild(const Abc802 *m, uint8_t *punkter);

/* Skärmen som text: rad 0-23 i UTF-8 utan blanksteg i slutet. ut ska
 * rymma ABC802_RADLANGD bytes. Grafiken blir Unicodes mosaiktecken. */
#define ABC802_RADLANGD (80 * 4 + 1)
void abc802_textrad(const Abc802 *m, int rad, char *ut);
/* Antalet synliga rader (R6), högst 25. */
int abc802_rader(const Abc802 *m);

#endif
