/*
 * bussen.h -- ABC-bussen, till boken ABC80 inifrån
 *
 * Korten på ABC-bussen har portarna 0-7. OUT 1 väljer ett kort med ett
 * nummer (bit 0-5) och de andra portarna går till det valda kortet:
 *     OUT 0   data till kortet           IN 0   data från kortet
 *     OUT 1   kortvalet                  IN 1   kortets status
 *     OUT 2-4 kommandona C1-C3
 * Ett kort som inte är valt svarar inte, och en port som ingen svarar
 * på läses som $FF (Markesjö; ABC-bussens beskrivning i abc80old_hw.txt).
 *
 * Ett kort är en struct med fyra funktioner. svarar säger om kortet har
 * numret (ett skivkort kan ha flera), valj får varje kortval (också
 * till andra kort; en del kort nollställer sig då). När kortet är valt
 * får in portarna 0-7 och ut portarna 0 och 2-7.
 *
 * Bussen får varje port där adressbit 4 är 0 (med bit 4 = 1 är det PIO:n
 * och tangentbordet). Bara bit 0-2 väljer port; de flesta kort bryr sig
 * inte om resten och får 0-7. Ett kort med hela_porten satt får hela
 * portnumret och kan avkoda bit 3, 5, 6 och 7 själv, som P40-kortet
 * (p40.h) gör med bit 5 och 6.
 */

#ifndef BUSSEN_H
#define BUSSEN_H

#include <stdint.h>

#define BUSSEN_KORT 8

typedef struct Kort {
    void *data;
    int     (*svarar)(void *data, int nummer);
    void    (*valj)(void *data, int nummer);
    uint8_t (*in)(void *data, int port);
    void    (*ut)(void *data, int port, uint8_t varde);
    int hela_porten;                         /* 1: in och ut får port 0-$FF */
} Kort;

typedef struct Bussen {
    Kort *kort[BUSSEN_KORT];
    int antal;
    int valt;                                /* numret i senaste OUT 1, -1 först */
} Bussen;

void    bussen_tom(Bussen *b);
void    bussen_koppla(Bussen *b, Kort *kort);        /* högst BUSSEN_KORT */
void    bussen_koppla_ur(Bussen *b, Kort *kort);
uint8_t bussen_in(Bussen *b, int port);              /* port 0-$FF, bit 4 = 0 */
void    bussen_ut(Bussen *b, int port, uint8_t varde);

#endif
