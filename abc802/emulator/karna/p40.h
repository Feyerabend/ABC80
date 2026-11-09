/*
 * p40.h -- nålskrivaren P40 på kort 60, till boken ABC80 inifrån
 *
 * P40 (Metric P40, Scandia Metric) har ingen egen processor. ABC80 kör
 * vagnens motor och slår an de sju nålarna, en kolumn i taget, med
 * drivrutinen på $7800 (roms/p40.rom, disassemblerad i dis/build/p40.asm
 * och beskriven i band 2, kapitlet Skrivarna). Det här är skrivaren och
 * dess kort: en Z80 PIO på kort 60, en vagn som går mellan två ändlägen
 * och ett papper som punkterna hamnar på.
 *
 * Portarna, med kort 60 valt (OUT 1); kortet avkodar själv bit 5 och 6:
 *     $00  kanal A, data. Ut: bit 0 = 0 motorn åt höger, bit 1 = 0 åt
 *          vänster. In: bit 2 = 0 vagnen i högra änden, bit 3 = 0 i
 *          vänstra, bit 5 = 1 alltid tillbaka efter en rad, bit 6-7
 *          teckenuppsättningen (1 = svensk).
 *     $20  kanal B, data: nålarna, bit 0 översta.
 *     $40  kanal A, styrord.        $60  kanal B, styrord.
 * Läsning ger utgångarna som de skrevs och ingångarna från skrivaren.
 *
 * Mekaniken är inte bevarad, så den är antagen (?):
 *   - Vagnen går med fast fart, utan start- och stoppsträcka, så länge
 *     motorn är på, och stannar mot ändarna. Farten är en kolumn per
 *     P40_KOLUMN_T T-cykler, tiden mellan två kolumner i ett tecken
 *     (COLOUT med anropet, 5 679 T, och slingan runt den, 29 T, lika
 *     åt båda hållen), så att tecknen blir lika breda som de är ritade.
 *   - Banan är P40_BANAN kolumner: 240 för raden och 11 tomma på var
 *     sida, som de 11 tomma kolumner som drivrutinen skriver bakåt innan
 *     raden börjar.
 *   - Ändlägena slår till exakt i ändarna.
 *   - Papperet matas fram en rad varje gång vagnen når en ände.
 * Var nålarna träffar avgörs alltså bara av tiden, som på skrivaren.
 * Om kolumnerna hamnar under varandra åt båda hållen beror på hur väl
 * drivrutinens tidmätning stämmer (TRAVEL; se dis/build/p40.asm).
 */

#ifndef P40_H
#define P40_H

#include <stdint.h>

#include "bussen.h"

#define P40_KORT        60
#define P40_KOLUMN_T    5708                 /* T-cykler per kolumn */
#ifndef P40_BANAN
#define P40_BANAN       262                  /* kolumner mellan ändarna */
#endif
#define P40_DELAR       4                    /* lägen per kolumn i papperet */
#define P40_ANSLAG      65536                /* högst så många anslag */

typedef struct P40Anslag {
    int32_t x;                               /* i kolumner * P40_DELAR */
    int32_t rad;                             /* pappersraden */
    uint8_t nalar;                           /* bit 0 översta */
} P40Anslag;

typedef struct P40 {
    Kort kort;                               /* kopplas till bussen */
    const long long *tid;                    /* maskinens T-cykler */
    uint8_t ut_a, ut_b;                      /* senast skrivna data */
    uint8_t riktning_a, riktning_b;          /* 1 = ingång */
    int vantar_riktning_a, vantar_riktning_b;
    uint8_t teckenuppsattning;               /* bit 6-7 i kanal A, 0-3 */
    long long lage;                          /* vagnen, i T-cykler från vänster */
    long long senast;                        /* när lage räknades */
    int rad;                                 /* pappersraden */
    P40Anslag anslag[P40_ANSLAG];
    int antal;
} P40;

/* tid pekar på maskinens klocka (maskin.tid). Vagnen står till vänster. */
void p40_starta(P40 *p, const long long *tid);

/* Papperet som bild, 1 = punkt: bredd * hojd bytes i bild (eller bara
 * storleken om bild är NULL). Ett anslag blir en punkt om 3 x 3 rutor;
 * en kolumn är 4 rutor bred, en nålrad 4 hög, och raderna 40 isär. */
void p40_papper(const P40 *p, uint8_t *bild, int *bredd, int *hojd);

#endif
