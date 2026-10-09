/*
 * ljud.h -- ljudkretsen SN76477, till boken ABC80 inifrån
 *
 * ABC80 har en SN76477 som styrs med OUT 6. Kretsens ingångar sitter
 * på bitarna i porten:
 *     bit 0      kretsen på (1) eller av (0); när den slås på startar
 *                engångspulsen
 *     bit 1      VCO:ns spänning: 0 = 0 V, hög ton (ungefär 6 400 Hz),
 *                1 = 2 V, låg ton (ungefär 640 Hz)
 *     bit 2      0 = VCO:n styrs av spänningen, 1 = av SLF:en (svaj)
 *     bit 3-5    blandaren: läget (bit 5, 3, 4) = C B A
 *                0 VCO, 1 SLF, 2 brus, 3 VCO+brus, 4 SLF+brus,
 *                5 SLF+VCO+brus, 6 SLF+VCO, 7 tyst
 *     bit 6-7    höljet: läget (bit 6, 7)
 *                0 följer VCO:n, 1 engångspulsen, 2 alltid på, 3 växlande
 * Ett pip (CHR$(7)) är OUT 6,0 och sedan OUT 6,131.
 *
 * Komponentvärdena är ABC80:s (MAME:s abc80-drivrutin) och kretsens
 * modell kommer ur sn76477.c i Pico-varianten (dis/04/src), som bygger
 * på MAME:s modell: kondensatorspänningar som laddas och laddas ur ett
 * steg per prov, och mätta förstärkningstabeller för höljet.
 *
 * Skrivningarna läggs i en kö med sin tid i T-cykler och verkställs när
 * proven för den tiden räknas fram, så att ljudet hamnar rätt i tiden
 * oavsett hur värden hämtar proven.
 */

#ifndef LJUD_H
#define LJUD_H

#include <stdint.h>

#define LJUD_HANDELSER 8192

typedef struct Ljud {
    long provtakt;                           /* prov per sekund */
    long klocka;                             /* T-cykler per sekund */
    long long prov;                          /* prov som har räknats fram */

    /* Skrivningarna till port 6 som inte har verkställts än. */
    long long handelse_tid[LJUD_HANDELSER];
    uint8_t handelse_varde[LJUD_HANDELSER];
    int forst, sist;

    /* Ingångarna (det senast skrivna till port 6). */
    int pa;                                  /* kretsen är på */
    float vco_spanning;                      /* 0 eller 2 V */
    int vco_extern;                          /* 1: spänningen styr, 0: SLF:en */
    int blandare;                            /* 0-7 */
    int holje;                               /* 0-3 */

    /* Kondensatorernas spänningar och vipporna. */
    float engangs_v, slf_v, vco_v, brusfilter_v, holje_v;
    int engangs_vippa, slf_vippa, vco_vippa, vco_varannan;
    int brusfilter_vippa, brus_vippa;

    /* Bruset: ett skiftregister som stegas med brusklockan. */
    uint32_t brusregister, brusfas, brustakt;

    /* Ändringarna per prov (fasta för ABC80:s komponenter). */
    float engangs_laddning, engangs_urladdning;
    float slf_laddning, slf_urladdning, vco_steg;
    float brusfilter_laddning, brusfilter_urladdning;
    float stigning, avklingning, topp_v;

    float styrka;                            /* 0-1, tonas vid av och på */
    float forra, utgang;                     /* högpassfiltret, i volt */
} Ljud;

/* Kretsen avslagen, med provtakten och klockan (T-cykler per sekund).
 * fro bestämmer brusets och SLF:ens startläge, som på en riktig krets
 * beror på när den slogs på; samma fro ger samma ljud. */
void ljud_starta(Ljud *l, long provtakt, long klocka, uint32_t fro);

/* En skrivning till port 6 vid tiden (T-cykler). Tiderna ska växa. */
void ljud_skriv(Ljud *l, long long tid, uint8_t varde);

/* Antalet prov från starten till tiden. */
long long ljud_prov_till(const Ljud *l, long long tid);

/* De nästa antal proven, 16 bitar med tecken; tystnad är 0. */
void ljud_prov(Ljud *l, int16_t *buffert, int antal);

#endif
