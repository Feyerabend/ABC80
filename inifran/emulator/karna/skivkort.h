/*
 * skivkort.h -- skivkorten på ABC-bussen, till boken ABC80 inifrån
 *
 * Ett skivkort med upp till SKIVKORT_ENHETER avbilder. Varje avbild hör
 * till ett kortnummer och en enhet:
 *     kort 44 och 36   kontrollern som DOS:en i BASIC II och UFD-DOS
 *                      talar med genom SECIO ($608F): 44 för disketterna
 *                      (MF0-MF3), 36 för hårddisken (HD)
 *     kort 45          FD2 (ABC-DOS för FD2); protokollet ur kontrollerns
 *                      PROM FD2-M1.02 (dis/roms/orig/floppy)
 * Avbilden är en buffert med 256 bytes per sektor, som värden har läst
 * in; skrivningar går direkt till bufferten, och värden får skriva
 * tillbaka den.
 *
 * Kort 44 och 36 (SECIO): OUT 2 = nytt kommando, OUT 4 = återställ;
 * kommandot är 4 bytes på port 0: 3 (läs) eller $0C (skriv), enhet,
 * sektor hög, låg. Sedan 256 bytes på port 0 (IN eller OUT) och till
 * sist statusen på IN 0 (0 = rätt). IN 1: $81 = väntar på kommando
 * eller status (bit 7 = status kan läsas, bit 0 = redo för en byte),
 * $01 = data. Kort 36 har sektornumret som det står; på kort 44 har
 * SECIO flyttat det (kortbytens bit 6-7: $40 = 4 sektorer per 32, $80 =
 * 1 per 32), och här räknas det tillbaka till en löpande sektor.
 * Fel: status $80 (enheten finns inte; DFFIND och DFCRE går då vidare
 * till nästa enhet), $01 (sektorn finns inte eller är trasig), $40 vid
 * skrivning till en skrivskyddad avbild.
 *
 * Kort 45 (FD2): OUT 2 (C1) ger kontrollerns NMI och ett nytt kommando;
 * IN 1 är statuslåset (port $E0 på kortet) med bit 0 från PIO:ns
 * handskakning:
 *     bit 7 = klart (status kan läsas på IN 0), bit 6 = riktning (1 =
 *     ABC80 -> FD2), bit 3 = 0 efter fel, bit 2 = 0 under
 *     dataöverföringen, bit 1 = 0 medan kommandot tas emot, bit 4-5 = 1.
 * Kommandot: byte 1 bitvis (bit 0 läs från skivan, bit 1 skicka
 * bufferten, bit 2 ta emot bufferten, bit 3 skriv på skivan; 3 = läs,
 * $0C = skriv), byte 2 = enhet (bit 0-2; bit 6-7 = buffert), byte 3 =
 * spår, byte 4 bit 5-7 = sektor. Status: $80 = enhet >= 2 eller ingen
 * skiva, $02 = spår >= 40, annars FDC:ns status (läsning AND $98,
 * skrivning AND $FC: $40 = skrivskydd). Vid skrivning tas bufferten
 * emot innan enheten prövas. FD2 behåller sitt läge när ett annat kort
 * väljs.
 */

#ifndef SKIVKORT_H
#define SKIVKORT_H

#include <stdint.h>

#include "bussen.h"

#define SKIVKORT_ENHETER 8
#define SKIVKORT_TRASIGA 8

typedef struct Skivenhet {
    int kort, enhet;                         /* t.ex. 44 och 0 = MF0 */
    int skrivskydd;
    long sektorer;
    uint8_t *data;                           /* sektorer * 256 bytes */
} Skivenhet;

typedef struct Skivkort {
    Kort kort;                               /* kopplas till bussen */
    Skivenhet enhet[SKIVKORT_ENHETER];
    int antal_enheter;

    /* Trasiga sektorer: n trasig, -n - 1 trasig bara vid skrivning. */
    long trasig[SKIVKORT_TRASIGA];
    int antal_trasiga;

    /* FD2 hänger (svarar inte) på kommando nummer hang_forsta till
     * hang_sista, räknat från 1, tills C3 (OUT 4) återställer den. */
    int hang_forsta, hang_sista;

    /* Läget. */
    int valt;                                /* kortnumret i OUT 1 */
    int lage;                                /* SECIO: 0 kommando, 1 data ut,
                                                2 data in, 3 status */
    int antal;                               /* bytes i kommando eller buffert */
    int aktuell;                             /* enheten i kommandot, -1 ingen */
    uint8_t kommando[4], buffert[256], status;
    int fd2_lage;                            /* 0 kommando, 1 data ut, 2 data in,
                                                3 klart, 4 hänger */
    int fd2_utan_fel;                        /* bit 3 i statuslåset */
    int fd2_status_olast;                    /* bit 0 i statuslåset */
    int fd2_kommandon;                       /* räknade, för hang_forsta */
} Skivkort;

/* Ett tomt kort; koppla sedan &kort.kort till bussen. */
void skivkort_starta(Skivkort *s);

/* En avbild på kort och enhet; 0 om det inte finns plats. */
int skivkort_enhet(Skivkort *s, int kort, int enhet, uint8_t *data, long storlek,
                   int skrivskydd);

#endif
