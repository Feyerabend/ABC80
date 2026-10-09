/*
 * maskin.h -- ABC80 runt processorn, till boken ABC80 inifrån
 *
 * Kärnan i emulatorn: minnet, portarna, avbrotten, tangentkön, V24 in
 * och labplattan, med tiden räknad i processorns T-cykler. Den gör
 * ingen in- eller utmatning själv: värden (vard/) läser filerna och
 * ger dem som bytebuffertar, kör maskinen med maskin_kor, lägger
 * tangenter i kön och läser bildminnet.
 *
 * Uppgifterna om maskinen kommer ur Markesjö, abc80old_hw.txt
 * (dis/tools) och MAME:s abc80-drivrutin:
 *     klockan        2,9952 MHz (11,9808 MHz / 4), 59 904 T-cykler per 20 ms
 *     NMI            var 20:e ms (bildens vertikalsignal)
 *     minnet         ROM från $0000, bildminnet $7C00-$7FFF, RAM överst;
 *                    allt annat läser $FF och går inte att skriva
 *     port $38       tangentbordet (PIO A): koden, bit 7 = tangenten är nere
 *     port $39       PIO A:s styrord: läge, avbrott och vektor
 *     port $3A       PIO B: bit 0-2 V24:s ingångar (RxD, CTS, DCD; CTS och
 *                    DCD läser 1, eller labplattans strömbrytare), bit 3-6
 *                    det senast skrivna och bit 7 kassettens flanklatch
 *                    (1 utan kassett)
 *     port $3B       PIO B:s styrord; avbrott när bit 7 går låg (kassetten)
 *     avbrott        PIO A i läge 3 med avbrott på: INT när en tangent
 *                    trycks ned; i läge 1: INT från linjesignalen på ASTB,
 *                    7 812,5 gånger i sekunden. Vektorn är PIO A:s.
 *                    PIO B ger INT med sin vektor när kassettens latch
 *                    sätts, och inte igen förrän efter RETI; PIO A går före
 *     port 0-7       ABC-bussen (bussen.h): OUT 1 väljer kort, de andra
 *                    portarna går till det valda kortet
 *     port 6         ljudkretsen SN76477 (ljud.h), om värden har kopplat in den
 * Värden kan koppla in kort på bussen, en enhet på PIO B (V24-skrivaren),
 * kassetten (kassett.h) och krokar som anropas före och efter varje
 * instruktion.
 */

#ifndef MASKIN_H
#define MASKIN_H

#include <stddef.h>
#include <stdint.h>

#include "bussen.h"
#include "kassett.h"
#include "ljud.h"
#include "z80.h"

#define MASKIN_KLOCKA     2995200L               /* T-cykler per sekund */
#define MASKIN_BILD       (MASKIN_KLOCKA / 50)   /* T-cykler per 20 ms */
/* ms till T-cykler, exakt (2 995,2 per ms) */
#define MASKIN_MS(ms)     ((long long)(ms) * MASKIN_KLOCKA / 1000)
#define MASKIN_BILDMINNE  0x7C00

#define TANGENTKO_STORLEK 65536
#define V24_STORLEK       4096
#define LABB_STORLEK      16
#define BANKER            8
#define BANKSTORLEK       4096

typedef struct Maskin Maskin;

/* En port i Z80 PIO: läget, avbrotten och vektorn. Efter läge 3 kommer
 * riktningen och efter ett avbrottsord med bit 4 masken. */
typedef struct Pio {
    int lage;                                /* 0-3 */
    int avbrott;                             /* avbrotten på */
    int riktning_kommer, mask_kommer;
    uint8_t vektor;
    uint8_t avbrottsord;                     /* bit 5: aktivt hög, 6: OCH */
    uint8_t mask;                            /* 1: biten ger inget avbrott */
    int vantar;                              /* INT som inte har tagits */
    int betjanas;                            /* tagits, men inte RETI än */
} Pio;

/* En enhet på PIO B i stället för de vanliga V24-ingångarna. las får det
 * senast skrivna och linjen RxD och ger det som port $3A läses som;
 * skriv får varje OUT till $3A. */
typedef struct PioBEnhet {
    void *data;
    uint8_t (*las)(void *data, uint8_t senast_skrivet, int rxd);
    void (*skriv)(void *data, uint8_t varde);
} PioBEnhet;

/* Krokar för värden; de som är NULL anropas inte. fore_steg anropas
 * före varje instruktion och stannar maskinen om den ger något annat
 * än 0 (maskin_kor återvänder, och maskinen står still tills
 * maskin.stoppad nollställs). efter_steg får operationskoden och PC
 * och SP före instruktionen. port_ut får varje OUT och skyddad_skrivning
 * varje skrivning som minnet inte tog emot. */
typedef struct Krokar {
    void *data;
    int  (*fore_steg)(void *data, Maskin *m);
    void (*efter_steg)(void *data, Maskin *m, uint8_t operation, uint16_t pc, uint16_t sp);
    void (*port_ut)(void *data, Maskin *m, uint8_t port, uint8_t varde);
    void (*skyddad_skrivning)(void *data, Maskin *m, uint16_t adress, uint8_t varde);
} Krokar;

struct Maskin {
    Z80 z80;
    uint8_t minne[65536];
    uint8_t skydd[65536];                    /* 1: går inte att skriva */
    long long tid;                           /* T-cykler sedan start */
    long long nasta_nmi;                     /* när nästa NMI kommer */
    long long linjesignal;                   /* räknas i T-cykler * 15 625 */
    long long instruktioner;                 /* sedan start */
    int utan_nmi;                            /* 1: ingen NMI (klockan står) */
    long instruktioner_per_bild;             /* 0: tiden i T-cykler; annars
                                                räknas den i instruktioner,
                                                så många per 20 ms */
    int stoppad;                             /* en krok har stannat maskinen */
    Krokar krokar;
    Bussen bussen;
    PioBEnhet *pio_b_enhet;                  /* NULL: V24 in och labplattan */
    Ljud *ljud;                              /* NULL: inget ljud */
    Kassett *kassett;                        /* NULL: ingen bandspelare */

    /* En krets med banker: skrivning till adress + $40 + n väljer bank n. */
    uint8_t bank[BANKER][BANKSTORLEK];
    int antal_banker, bankstorlek;
    uint16_t bankadress;

    /* PIO A: tangentbordet på port $38, styrorden på $39. PIO B:
       V24 och kassetten på $3A, styrorden på $3B. */
    uint8_t tangent;                         /* koden; bit 7 = nere */
    Pio pio_a, pio_b;

    /* Tangentkön: en kod 0-255, eller 256 + ms för en paus. */
    int tangentko[TANGENTKO_STORLEK];
    int ko_forst, ko_sist;
    int tangent_nere;                        /* tangenten ur kön är nere */
    long long ko_nasta;                      /* när kön går vidare */
    long tryck_ms, slapp_ms;                 /* hur länge nere och släppt */

    /* V24 in: tecknen som tas emot, 8 bitar, ingen paritet, en stoppbit. */
    uint8_t v24[V24_STORLEK];
    int antal_v24;
    long long v24_start;                     /* -1: inte börjat */
    long baud;

    /* Labplattan: strömbrytarna på V24:s ingångar (bit 0-2) från en tid. */
    int labb[LABB_STORLEK];
    long long labb_tid[LABB_STORLEK];
    int antal_labb;
    uint8_t pio_b_skrivet;                   /* PIO B: det senast skrivna */
};

/* Töm maskinen: minnet $FF och skrivskyddat, bildminnet blanksteg, RAM
 * överst (ram_kb 16, 32 eller 48; under $8000 sitter inget RAM).
 * Tangenterna hålls nere 40 ms och släppta 60 ms, V24 går i 1 200 baud. */
void maskin_starta(Maskin *m, int ram_kb);

/* ROM:en från $0000: 16 KB (ABC80) eller 32 KB (BASIC II, $0000-$7BFF;
 * bildminnet ligger kvar). 0 om storleken är fel. */
int maskin_rom(Maskin *m, const uint8_t *data, size_t storlek);

/* En krets på adressen, skrivskyddad. */
void maskin_krets(Maskin *m, uint16_t adress, const uint8_t *data, size_t storlek);

/* Kretsen med banker: lägg till nästa bank (högst BANKER) och koppla in
 * bank 0 på adressen. */
void maskin_bank(Maskin *m, uint16_t adress, const uint8_t *data, size_t storlek);

/* RAM på adressen, fyllt med fyllnad (t.ex. ett bildminne på ett kort). */
void maskin_ram(Maskin *m, uint16_t adress, size_t storlek, uint8_t fyllnad);

/* Processorn till startläget (efter att minnet är laddat). */
void maskin_aterstall(Maskin *m);

/* Kör minst tcykler T-cykler, eller tills en krok stannar maskinen. */
void maskin_kor(Maskin *m, long long tcykler);

/* Tangentkön. tangentko_tom är sann när kön, tangenten och pausen är slut. */
void tangentko_lagg(Maskin *m, uint8_t kod);
void tangentko_paus(Maskin *m, long ms);
int  tangentko_tom(const Maskin *m);

/* V24 in: lägg till tecken; mottagningen börjar vid tiden start. */
void v24_lagg(Maskin *m, uint8_t tecken);
void v24_starta(Maskin *m, long long start);
long long v24_tid(const Maskin *m);          /* T-cykler för alla tecken */

/* Labplattan: strömbrytarna (bit 0-2, 7 = alla öppna) från tiden ms. */
void labb_lagg(Maskin *m, int brytare, long ms);

#endif
