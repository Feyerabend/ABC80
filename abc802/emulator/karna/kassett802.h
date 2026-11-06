/*
 * kassett802.h -- bandspelaren på SIO B, till ABC 802 inifrån
 *
 * Kassetten sitter på SIO/2:ns kanal B ($42/$43), och DTR på kanalen
 * drar reläet RE1 till motorn (servicemanualen s. 50, kretsschemat blad
 * 1). ROM:en (CAS: på $736D-$79C0, ../../dis/tools/abc802_notes.py)
 * skriver block teckensynkront: 32 x $00, $16 $02, 256 bytes, $03, en
 * summa på 16 bitar och $FF $FF, och läser med SIO:n i bisynk som letar
 * efter $16 $02 själv.
 *
 * Hur signalen ser ut på bandet beskriver varken ROM:en eller manualerna
 * (?), så bandet här är en följd av tecken, en byte för varje tecken som
 * går förbi huvudet. Ett tecken tar 8 bitar à 16 x tidkonstanten i CTC
 * kanal 0 T-cykler: vid skrivning går kanalen som timer med förskalning
 * 16 och ger SIO:n bitklockan (x1); vid läsning räknar den 1,5 MHz, åtta
 * gånger så fort, som om gränssnittet tar åtta prov per bit (?). Med
 * ROM:ens tidkonstant 129 blir det 1 453 bitar och 182 tecken i sekunden.
 *
 * Bandet går bara när motorn är på. Inspelningsknappen
 * (kassett802_spela_in) avgör om bandet spelas in eller upp: inspelat
 * blir det SIO B sänder, eller $00 när sändaren är av (pausen före
 * blocken); uppspelat går tecknen till SIO B:s mottagare när den är på.
 * Efter bandets slut kommer inget mer.
 */

#ifndef KASSETT802_H
#define KASSETT802_H

#include <stdint.h>

typedef struct Kassett802 {
    const uint8_t *spelas;                   /* bandet som spelas upp */
    long antal_spelas;
    uint8_t *inspelning;                     /* värdens buffert */
    long storlek_inspelning, antal_inspelat;
    int spelar_in;
    int full;                                /* bufferten räckte inte */
    long lage;                               /* tecken från bandets början */
    long long nasta;                         /* nästa tecken vid huvudet, -1
                                                när motorn står */
} Kassett802;

/* Ingen kassett, motorn av. */
void kassett802_starta(Kassett802 *k);

/* Lägg i ett band att spela upp, från början. */
void kassett802_spela(Kassett802 *k, const uint8_t *band, long antal);

/* Tryck ned inspelningsknappen: bandet spelas in från början i
 * värdens buffert. */
void kassett802_spela_in(Kassett802 *k, uint8_t *buffert, long storlek);

/* Ett tecken förbi huvudet (anropas av abc802.c): inspelat, eller
 * uppspelat i *tecken (0 efter bandets slut eller vid inspelning). */
void kassett802_skriv(Kassett802 *k, uint8_t tecken);
int kassett802_las(Kassett802 *k, uint8_t *tecken);

#endif
