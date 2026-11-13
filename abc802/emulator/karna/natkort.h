/*
 * natkort.h -- nodkortet och centralen i LUXNET, till ABC 802 inifrån
 *
 * Med LUXNET-PROM:en (../roms/abc802net.rom) frågar DOS:en nätet före
 * varje anrop: NETHK ($6DC1) väljer nodkortet (kort 3) och kör OUT (C),A
 * på $6009 (NETOUT) med ett funktionsnummer 1-$16 i L. Där tog det
 * riktiga kortet över med sitt eget minne (?); hur det gick till och vad
 * som gick på tråden vet vi inte (?).
 *
 * Här spelar emulatorn både nodkortet och centralen. Kroken på $6009
 * (abc802.h, krok) gör jobbet direkt och hoppar till NETBAK ($6DDE) med
 * L = $80 (klart). Tar kortet inte funktionen körs OUT som vanligt, L
 * förblir utan bit 7 och den lokala DOS:en gör jobbet. Svaret står i
 * registren som NETHK har lagt på stacken: AF (A = felet, CY = 1 vid
 * fel), DE och BC. Det är emulatorns gränssnitt mot ROM:en, inte
 * LUXNET:s eget protokoll.
 *
 * Centralen har filerna i minnet. Värden fyller den från en mapp, som
 * blir kontots UFD:, med en undermapp per katalog (CD), och skriver
 * tillbaka det som har ändrats. En fil är posternas data, 253 bytes per
 * post (platserna 3-255 i kanalens buffert), efter varandra.
 *
 * Funktionerna som centralen tar:
 *     9   starten (DOSSTA): inloggningen, UFD: sätts (DSYSUN, $FFF9)
 *     2   PREPARE, 3 OPEN, 4 CLOSE, 5 läs post, 6 skriv post,
 *     7   SETEND ($FFFF = KILL), 8 NAME (DE = det nya namnet), $12 CD
 *     1   läs sektor: katalogen som sektorer, så att Luxors LIB.BAC
 *         (UFD-DOS) kan lista UFD: (natkort.c, KATALOG)
 * OPEN "UFD:" utan namn öppnar biblioteket som en fil, med katalogen
 * och en RIB per fil (storleken, ägaren, tiden och skydden), så som
 * Dataindustriers L.BAC läser det (natkort.c, BIBLIOTEKET).
 * allt för enheten UFD: ($1E). De andra går till den lokala DOS:en (som
 * ger fel 200 där det bara finns en väg via nätet). FILESTAT ($13-$15)
 * gör något som ingen källa beskriver och ger fel 200. PFD:, LFD: och
 * RP0:-RP2: finns inte här (fel 46).
 *
 * Kärnan använder inget ur C-biblioteket utom malloc, free och mem*,
 * så att den går att bygga till webbsidan.
 */

#ifndef NATKORT_H
#define NATKORT_H

#include <stdint.h>

#include "abc802.h"

#define NATKORT_FILER   256
#define NATKORT_KATALOG 64                   /* "KALLE/PROV" */
#define NATKORT_POST    253                  /* data per post */
#define NATKORT_RAD     96

typedef struct NatFil {
    char katalog[NATKORT_KATALOG];           /* "" = kontots UFD */
    char namn[12];                           /* "NAMN    EXT" (11 tecken) */
    uint8_t *data;
    long langd, plats;
    uint8_t tid[6];                          /* år - 1900, månad, dag, tim, min, sek */
    int ar_katalog;                          /* en katalog, namn = sista ledet */
    int andrad, borta;                       /* för värden */
} NatFil;

typedef struct Natkort {
    NatFil filer[NATKORT_FILER];
    int antal;
    char ufd[NATKORT_KATALOG];               /* UFD: just nu ("" = hemma) */
    int kanal[8];                            /* fil per kanal, -1 ingen,
                                                -2 biblioteket */
    int ta_bort[8];                          /* KILL: bort vid CLOSE */
    uint8_t nu[6];                           /* klockan, som tid; värden sätter
                                                den (natkort_fil tar den) */

    /* Trafiken: varje anrop som centralen tog, som en rad text. */
    long anrop;
    char rad[NATKORT_RAD];
    void *logg_data;
    void (*loggat)(void *data, const char *rad);
} Natkort;

/* Töm centralen (klockan blir 0, år 1900: ingen tid). */
void natkort_starta(Natkort *n);

/* Fyll centralen: en katalog eller en fil. katalog är "" eller
 * "A/B"; namn är "NAMN.EXT". Filens tid blir n->nu. 0 om det inte går
 * (fullt, fel namn). */
int natkort_katalog(Natkort *n, const char *katalog);
int natkort_fil(Natkort *n, const char *katalog, const char *namn,
                const uint8_t *data, long langd);

/* Koppla in kortet i maskinen (kroken på $6009), efter abc802_starta. */
void natkort_koppla(Natkort *n, Abc802 *m);

/* Namnet som "NAMN.EXT" (ut rymmer 13 bytes). */
void natkort_filnamn(const NatFil *f, char *ut);

#endif
