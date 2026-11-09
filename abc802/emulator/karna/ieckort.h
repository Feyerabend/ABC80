/*
 * ieckort.h -- IEC-kortet på ABC-bussen, till boken ABC80 inifrån
 *
 * Kort 49 (DataBoard 4027) med kretsen TMS 9914A, som IEC-kretsen 4027
 * talar med. OUT 1 = 49 väljer kortet, IN 1 ger STAT = $E0 + adressen
 * (kodpluggen), OUT 2 väljer kretsens register (bit 0-2) och port 0
 * läser och skriver det.
 *
 * En enkel modell av 9914A (TI:s datablad). INT STATUS 0 (läs 0) har BI
 * $20, BO $10 och END $08 och nollställs när den läses. BO sätts när
 * kretsen tar kontrollen (sic, tca, tcs: ATN sann), när ton sätts eller
 * gts görs medan ton gäller (talare), och efter varje tecken till DATA
 * OUT: lyssnarna tar emot direkt, om det finns någon. En talare på
 * bussen skickar en given text: nästa tecken kommer (BI, med END vid
 * EOI) när lon gäller, ATN är falsk och kretsen inte håller
 * handskakningen; efter läsning av DATA IN håller den om hdfa är satt,
 * tills rhdf.
 *
 * Det som händer på bussen skrivs i en logg, tolv poster på varje rad
 * efter "IEC:": kommandonamnen till AUX CMD (sist 0 = släpp), Chh =
 * kommando (ATN), Dhh = data från ABC80 (E = med EOI), Rhh = läst av
 * ABC80, Xhh = tecken utan roll, Ahh = ADDRESS.
 */

#ifndef IECKORT_H
#define IECKORT_H

#include <stdint.h>

#include "bussen.h"

#define IECKORT_TEXT 256
#define IECKORT_LOGG 16384

typedef struct Ieckort {
    Kort kort;                               /* kopplas till bussen */
    int adress;                              /* kodpluggen, 0-31 */
    int utan_lyssnare;                       /* BO kommer inte tillbaka */
    int valt_register;                           /* valt med OUT 2 */

    /* Kretsens läge. */
    int nollstalld, lyssnare, talare, atn, styr, haller, hdfa, eoi_nasta,
        tecken_inne;
    uint8_t avbrott0;                        /* INT STATUS 0 */
    uint8_t data_in;

    /* Talaren på bussen: tecknen och om EOI kommer med dem. */
    uint8_t text[IECKORT_TEXT], text_eoi[IECKORT_TEXT];
    int antal_text, nasta_text;

    char logg[IECKORT_LOGG];
    int logglangd, loggposter;
} Ieckort;

void ieckort_starta(Ieckort *k, int adress);

/* Talarens text: \r, \xHH och \\, och \e före ett tecken = EOI med det. */
void ieckort_text(Ieckort *k, const char *text);

#endif
