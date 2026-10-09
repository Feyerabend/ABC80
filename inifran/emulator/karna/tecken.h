/*
 * tecken.h -- bildminnet som text, till boken ABC80 inifrån
 *
 * ABC80:s skärm är 24 rader om 40 tecken i bildminnet $7C00-$7FFF.
 * Raderna ligger inte i ordning: rad r börjar på
 *     $7C00 + (r & 7) * $80 + (r >> 3) * 40
 * Bildkretsen (SAA5050) visar teckenkoderna enligt SIS 662241, med
 * Å Ä Ö É Ü och små å ä ö é ü i stället för [ \ ] @ ^ och { | } ` ~,
 * och mosaikgrafik (2 x 3 punkter) efter en grafikkod. Här blir varje
 * rad en sträng i UTF-8; mosaiken blir tecknen i Unicodes block
 * "Symbols for Legacy Computing" (U+1FB00-).
 */

#ifndef TECKEN_H
#define TECKEN_H

#include <stdint.h>

#define TECKEN_RADER     24
#define TECKEN_KOLUMNER  40
#define TECKEN_RADLANGD  (TECKEN_KOLUMNER * 16 + 1)  /* räcker för en rad */

/* Raden rad (0-23) ur bildminnet (1 KB, från $7C00) som UTF-8 i ut, som
 * ska rymma TECKEN_RADLANGD bytes. Markören (bit 7; den syns inte på
 * koder under $20, Isaksson och Kärrsgård 1980, s. 72-73) skrivs med fore
 * och efter runt tecknet, t.ex. "\033[7m" och "\033[27m" i en
 * terminal; med fore == NULL blir den ett '_'. */
void tecken_rad(const uint8_t *bildminne, int rad, char *ut,
                const char *fore, const char *efter);

/* Samma rad utan blanksteg i slutet. */
void tecken_rad_kortad(const uint8_t *bildminne, int rad, char *ut);

/* ABC80:s kod för andra byten i ett UTF-8-tecken som börjar med $C3
 * (Å Ä Ö É Ü å ä ö é ü), eller 0 för andra tecken. */
uint8_t tecken_svensk(uint8_t andra_byten);

#endif
