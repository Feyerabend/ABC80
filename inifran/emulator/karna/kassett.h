/*
 * kassett.h -- bandspelaren och kassettgränssnittet, till boken ABC80 inifrån
 *
 * Kassetten sitter på PIO B (port $3A):
 *     bit 5 ut   motorreläet: 1 = bandet går
 *     bit 6 ut   skrivsignalen; vid läsning nollställer 0 flanklatchen
 *                (och håller den nollställd så länge biten är 0)
 *     bit 7 in   flanklatchen, inverterad: 0 = en flank har kommit
 * Vid skrivning vänder ROM:en bit 6 själv (CASHLF), en gång per bitcell
 * för en nolla och två gånger för en etta (FM). Vid läsning går
 * inspelningen genom en deriverande länk och två komparatorer, som
 * sätter latchen vid varje flank, åt båda hållen; PIO B ger avbrott när
 * bit 7 går låg (RCVSTA, CASINT). Uppgifterna kommer ur ROM:en,
 * Markesjö (s. 166-167, figur 6.31) och bokens kapitel om hårdvaran.
 *
 * Bandet är en följd av flanker, var och en med sin plats på bandet
 * räknad i T-cykler (2 995 200 per sekund) från bandets början. Bandet
 * går bara när motorn är på, och det står still annars. Värden gör om
 * en WAV-fil till flanker (det som spelas upp) och flankerna som spelas
 * in till en WAV-fil; kärnan ser bara talen.
 *
 * Inspelningsknappen: när den är nere (kassett_spela_in) skrivs bit 6:s
 * flanker in på bandet och inget läses; annars spelas bandet upp och
 * skrivsignalen går ingenstans.
 *
 * Bandspelarens knappar (kassett_knappar): med PLAY uppe (stopp) står
 * bandet still. Med PLAY nere går det när motorreläet drar, om
 * bandspelaren är fjärrstyrd; annars går det hela tiden, och stannar
 * inte när ROM:en släpper reläet. Så hördes musiken i Genesis Projects
 * demo (2015, exempel/genesis/): den ligger på bandet efter programmet
 * och spelas av bandspelaren själv, inte av ABC80. Från början är PLAY
 * nere och bandspelaren fjärrstyrd: motorn följer reläet.
 *
 * Spolning (kassett_spola) flyttar bandet till ett annat läge, framåt
 * eller bakåt; de flanker som ligger före läget räknas som passerade.
 */

#ifndef KASSETT_H
#define KASSETT_H

#include <stdint.h>

typedef struct Kassett {
    /* Bandet som spelas upp: flankernas platser, växande. */
    const long long *spelas;
    long antal_spelas;
    long nasta;                              /* den första som inte har passerat */

    /* Inspelningen: flankerna skrivs i värdens buffert. */
    long long *inspelning;
    long antal_inspelat, storlek_inspelning;
    int spelar_in;                           /* inspelningsknappen är nere */
    int full;                                /* bufferten räckte inte */

    /* Motorn och bandets läge. */
    int motorrelaet;                         /* bit 5, det senast skrivna */
    int stopp;                               /* PLAY är uppe: bandet står */
    int utan_fjarrstyrning;                  /* bandet går utan reläet */
    int motor;                               /* bandet går */
    long long band;                          /* läget när motorn startade
                                                (eller står, om den är av) */
    long long motor_start;                   /* maskinens tid när den startade */
    long long langst;                        /* längsta läget bandet har nått */

    /* Gränssnittet i ABC80. */
    int skrivsignal;                         /* bit 6, det senast skrivna */
    int latch;                               /* 1: en flank har kommit */
} Kassett;

/* Ingen kassett i, motorn av, bandet i början. */
void kassett_starta(Kassett *k);

/* Lägg i ett band att spela upp (flankerna ska växa). */
void kassett_spela(Kassett *k, const long long *flanker, long antal);

/* Tryck ned inspelningsknappen: flankerna skrivs i bufferten. */
void kassett_spela_in(Kassett *k, long long *buffert, long storlek);

/* Bandets läge vid maskinens tid. */
long long kassett_lage(const Kassett *k, long long tid);

/* Bandspelarens knappar vid maskinens tid: PLAY nere eller uppe, och
 * fjärrstyrd (motorreläet styr) eller inte. */
void kassett_knappar(Kassett *k, long long tid, int play_nere, int fjarrstyrd);

/* Spola bandet till läget (T-cykler från början, inte under 0) vid
 * maskinens tid; motorn går eller står som förut. */
void kassett_spola(Kassett *k, long long tid, long long lage);

/* OUT till port $3A vid tiden. */
void kassett_skriv(Kassett *k, long long tid, uint8_t varde);

/* Låt bandet gå fram till tiden. 1 om latchen sattes av en flank (PIO B
 * kan då ge avbrott). */
int kassett_fram(Kassett *k, long long tid);

#endif
