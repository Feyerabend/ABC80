/*
 * hr80.h -- ett grafikkort på V24 i HR-80:s anda, till ABC80 inifrån
 *
 * Utanför boken. Libers HR-80 (1984) var ett kort med egen processor,
 * 18 KB RAM och 4 KB EPROM som tog emot tecken på V24 och ritade i
 * färg: 256 x 256 punkter i fyra av åtta färger, eller 512 x 256
 * svartvitt med 80 tecken per rad. Det enda som finns kvar är en annons
 * (emulator/färg-abc80.pdf) med kommandona men inte deras argument.
 * Det här är en egen tolk som gör det annonsen säger; det som annonsen
 * inte säger är gissat och står som gissning i hr80.c och i PLAN.md.
 * Den är inte en kopia av kortets program.
 *
 * Tolken är ren C utan malloc, stdio och maskinen, så att samma fil kan
 * köras i emulatorn, i webbläsaren och på en mikrokontroller.
 *
 * Bildminnet är 16 KB, som på kortet: i färgläget (40 tecken) 256 x 256
 * punkter med 2 bitar vardera, i det svartvita (80 tecken) 512 x 256 med
 * 1 bit. Resten av minnet håller makrona.
 */

#ifndef HR80_H
#define HR80_H

#include <stdint.h>

#define HR80_MINNE    16384                  /* bildminnet */
#define HR80_MAKROMINNE 2048                 /* alla makron tillsammans */
#define HR80_BREDD    512                    /* bilden från hr80_rita */
#define HR80_HOJD     256

typedef struct Hr80 {
    uint8_t minne[HR80_MINNE];
    int svartvit;                            /* 1: 512 x 256, 80 tecken */
    uint8_t palett[4];                       /* färg 0-3 -> 0-7 */

    /* Grafiken: ritfärgen och den fasta punkten (28, 2, 4 ...). */
    int farg;
    int px, py;

    /* Texten: positionen i punkter (övre vänstra hörnet av rutan),
       färgerna och lägena. */
    int tx, ty;
    int forgrund, bakgrund;
    int markor;                              /* 14/15 */
    int semigrafik;                          /* 0, 1 sammanhängande, 2 åtskild */

    /* Kommandot som tar emot argument: koden, eller 256 + koden efter
       ESC. */
    int kommando;
    int esc;                                 /* efter 27 */
    uint8_t argument[4];
    int antal_argument, behover;

    /* Makron: 128-253. definierar >= 0 medan ett definieras. */
    uint8_t makrominne[HR80_MAKROMINNE];
    int makro_start[126], makro_langd[126];
    uint8_t makro_slag[126];                 /* 66 bitmapp, 82 instruktioner */
    int makro_slut;                          /* första lediga i makrominne */
    int definierar;                          /* makrots nummer, -1 */
    int makro_typ;
    int djup;                                /* makron som kör makron */
} Hr80;

/* Som efter ESC R: allt nollställt, makrona också. */
void hr80_starta(Hr80 *h);

/* Ett tecken från V24. */
void hr80_tecken(Hr80 *h, uint8_t tecken);

/* Bilden, HR80_BREDD x HR80_HOJD punkter uppifrån: i färgläget är varje
 * punkt två breda. farger[0-7] är de åtta färgerna i värdens format
 * (svart, röd, grön, gul, blå, magenta, cyan, vit). Med markor_synlig
 * ritas markören som ett streck under rutan, när den är på. */
void hr80_rita(const Hr80 *h, uint32_t *punkter, const uint32_t farger[8],
               int markor_synlig);

#endif
