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
 * 1 bit. Resten av minnet håller fönstertabellen och makrona.
 *
 * Fönstren (utanför fönsterförsöket, utanfor/fonster/PLAN.md, steg 2)
 * är påhittade, inte annonsens: ESC O, V, T och S i hr80.c.
 */

#ifndef HR80_H
#define HR80_H

#include <stdint.h>

#define HR80_MINNE    16384                  /* bildminnet */
#define HR80_ARBETSMINNE 2048                /* det som är kvar av 18 KB */
#define HR80_FONSTERMINNE 384                /* fönstren och kön, se hr80.c */
#define HR80_MAKROMINNE (HR80_ARBETSMINNE - HR80_FONSTERMINNE)
#define HR80_FONSTER  8                      /* fönster 0-7 */
#define HR80_SKRIVBORDET 8                   /* allt utanför fönstren */
#define HR80_TITEL    16                     /* titeln med 0 sist */
#define HR80_KO       16                     /* händelser som väntar */
#define HR80_BREDD    512                    /* bilden från hr80_rita */
#define HR80_HOJD     256

/* Ett fönster. Ytterkanten i bildminnets punkter (y nedåt), en hel
 * textruta på varje sida. */
typedef struct Hr80Fonster {
    uint8_t oppen;
    int16_t x, y, b, hojd;
    int16_t tx, ty, px, py;                  /* medan det inte är valt */
    uint8_t ram, bak;                        /* färgerna */
    char titel[HR80_TITEL];
} Hr80Fonster;

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
    uint8_t argument[5];
    int antal_argument, behover;

    /* Makron: 128-253. definierar >= 0 medan ett definieras. */
    uint8_t makrominne[HR80_MAKROMINNE];
    int makro_start[126], makro_langd[126];
    uint8_t makro_slag[126];                 /* 66 bitmapp, 82 instruktioner */
    int makro_slut;                          /* första lediga i makrominne */
    int definierar;                          /* makrots nummer, -1 */
    int makro_typ;
    int djup;                                /* makron som kör makron */

    /* Fönstren. fonster[HR80_SKRIVBORDET] är skrivbordet, som bara
       sparar markören och den fasta punkten. */
    Hr80Fonster fonster[HR80_FONSTER + 1];
    uint8_t ordning[HR80_FONSTER];           /* de öppna, understa först */
    int antal_oppna;
    int vald;                                /* 0-7 eller HR80_SKRIVBORDET */
    int titel_till;                          /* ESC O tar emot titeln, -1 */
    uint8_t skrivbord;                       /* skrivbordets bakgrund */
    uint16_t omrita;                         /* bit n: n har tappat innehåll */

    /* Ytan som ritas i, efter det valda: klippet (fönstren ovanför
       platsen kniva i ordningen klipper också), grafikens nedre vänstra
       hörn och texten. */
    int kx0, ky0, kx1, ky1, kniva;
    int gx0, gy1;
    int tx0, ty0, tkol, tymax;

    /* Musen, i bildens punkter (HR80_BREDD x HR80_HOJD), och fönstret
       som dras i titelraden (-1), med ytterkantens hörn där det släpps. */
    int mus_pa, mx, my, knapp;
    int drar, drag_dx, drag_dy, drag_x, drag_y;

    /* Händelserna till ABC80: slaget, fönstret och två tal. */
    uint8_t ko[HR80_KO][4];
    int ko_forsta, ko_antal;
} Hr80;

/* Som efter ESC R: allt nollställt, makrona också. */
void hr80_starta(Hr80 *h);

/* Ett tecken från V24. */
void hr80_tecken(Hr80 *h, uint8_t tecken);

/* Musen: en rörelse i bildens punkter och knappen (1 nere). Den första
 * tänder pekaren. Klick, lyft, dragning och stängning sköts på kortet
 * och läggs i kön; se hr80.c. */
void hr80_mus(Hr80 *h, int dx, int dy, int knapp);

/* Den äldsta händelsen ur kön till ut[4]; 0 om kön är tom. */
int hr80_handelse(Hr80 *h, uint8_t ut[4]);

/* Bilden, HR80_BREDD x HR80_HOJD punkter uppifrån: i färgläget är varje
 * punkt två breda. farger[0-7] är de åtta färgerna i värdens format
 * (svart, röd, grön, gul, blå, magenta, cyan, vit). Med markor_synlig
 * ritas markören som ett streck under rutan, när den är på. Pekaren och
 * ramen för ett fönster som dras ritas ovanpå, inte i bildminnet. */
void hr80_rita(const Hr80 *h, uint32_t *punkter, const uint32_t farger[8],
               int markor_synlig);

#endif
