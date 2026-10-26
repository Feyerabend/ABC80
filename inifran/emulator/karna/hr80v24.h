/*
 * hr80v24.h -- grafikkortet i HR-80:s anda på V24, till ABC80 inifrån
 *
 * Utanför boken; tolken är hr80.c. Här kopplas den till PIO B som
 * V24-skrivaren (skrivarkort.h): TxD avkodas med samma V24Avkodare och
 * varje helt tecken går till hr80_tecken. Kortet är alltid klart (port
 * $3A bit 1 = 0): tolken ritar på en gång, så CTS behövs inte här. Ett
 * riktigt kort med en egen processor behöver CTS när fyllningen tar tid.
 *
 * I BASIC II går tecknen med PR: på V24, helst som
 * OPEN "PR:VSA70A99.7": paritet oförändrad, inga NUL, ingen CR LF när
 * raden är full, FF som FF och 9600 baud. ABC80:s egen ROM har ingen
 * V24-utskrift; där behövs en rutin i maskinkod.
 */

#ifndef HR80V24_H
#define HR80V24_H

#include "hr80.h"
#include "maskin.h"
#include "skrivarkort.h"

typedef struct Hr80V24 {
    PioBEnhet enhet;                         /* maskin.pio_b_enhet = &enhet */
    V24Avkodare avkodare;
    Hr80 kort;
    long antal;                              /* tecknen hittills */
} Hr80V24;

void hr80_v24_starta(Hr80V24 *h);

#endif
