/*
 * uni80.h -- MyAB:s kort UNI-80 för CP/M, till boken ABC80 inifrån
 *
 * UNI-80 (band 2, kapitlet CP/M) har 64 KB RAM och en start-PROM. En
 * utport, $EF, väljer vilken minneskarta processorn ser. Uppgifterna om
 * kartorna kommer ur vad MyAB:s BIOS gör med porten (bara fakta om
 * hårdvaran används, ingen kod); det som inte går
 * att se där är vår egen modell och markerat med (?).
 *
 *     1  CP/M     kortets RAM på hela $0000-$FFFF
 *     2  system   ABC80 som vanligt: ROM, bildminnet, ABC80:s eget RAM;
 *                 här körs drivrutinerna för skärmen och skivan
 *     3  start    start-PROM:en på $0000-$07FF (BIOS:en läser 13
 *                 sektorer om 128 byte ur den från $0180, och de slutar
 *                 precis på $0800), kortets RAM på $0800-$7FFF och
 *                 ABC80:s minne på $8000-$FFFF (gränsen på $8000 är
 *                 vår (?); BIOS:en kopierar i den här kartan från
 *                 kortets RAM på $2000 till ABC80:s RAM på $F400)
 * I alla tre kartorna är $FF70-$FFFF samma RAM, så att data kan gå
 * mellan dem (BIOS:ens gemensamma område och sektorbuffert ligger där).
 * Här är det kortets RAM (?). Andra värden till porten ändrar inget
 * (?): MyAB:s BIOS för ABC80 skriver bara 1, 2 och 3.
 *
 * NMI (bildens vertikalsignal var 20:e ms) tas bara i systemkartan (?).
 * BIOS:en har ingen rutin för den, och i CP/M-kartan ligger $0066 mitt
 * i standard-FCB:n på $005C, så kortet måste hålla den borta.
 *
 * Utan knapp startar maskinen som ABC80, i systemkartan. Kortets RAM
 * börjar med $FF, som ABC80:s.
 *
 * ABC80-läget: innan något har skrivits till porten ger kortet 16 KB
 * extra (ABC-bladet 1981:3, s. 4) på $8000-$BFFF, där en ABC80 med 16
 * KB inte har något RAM. Det är karta 10 i kortets avkodnings-PROM,
 * UNI80-13 (roms/prom13.py): ROM, bildminnet och RAM på $8000-$FFFF.
 * Att RAM:et är kortets egna, på samma adresser, är vår modell (?).
 * Har ABC80 eget RAM där används det. Den första skrivningen till
 * porten avslutar läget, så att BIOS:en (och RAM-skivan C:) ser ABC80
 * som vanligt i systemkartan.
 *
 * UNI80-13 säger ingenting om kartorna 1-3 ovan: i alla dess fyra
 * kartor ligger ROM:en på $0000-$3FFF, så kortet måste stänga av ABC80:s
 * eget minne med ledningar som PROM:en inte visar. Kartorna 1-3 är därför
 * vår egen modell, byggd på vad BIOS:en gör med porten.
 */

#ifndef UNI80_H
#define UNI80_H

#include <stddef.h>
#include <stdint.h>

#define UNI80_PORT        0xEF
#define UNI80_GEMENSAMT   0xFF70           /* härifrån samma RAM i alla kartor */
#define UNI80_SIDA        0x800            /* kartorna delas i sidor om 2 KB */
#define UNI80_SIDOR       (0x10000 / UNI80_SIDA)
#define UNI80_STARTKRETS  0x800            /* start-PROM:en, 2 KB */

/* Kartorna, med värdena som skrivs till porten. */
enum { UNI80_CPM = 1, UNI80_SYSTEM = 2, UNI80_START = 3 };

/* Var en sida finns. UNI80_EXTRA: kortets RAM, om ABC80 inte har eget
 * på adressen (ABC80-läget). */
enum { UNI80_ABC80, UNI80_RAM, UNI80_KRETS, UNI80_EXTRA };

typedef struct Uni80 {
    uint8_t ram[0x10000];                  /* kortets RAM, på samma adresser */
    uint8_t startkrets[UNI80_STARTKRETS];  /* $FF utan krets */
    int karta;                             /* 1, 2 eller 3 */
    int abc80lage;                         /* porten inte skriven än */
    uint8_t sidor[UNI80_SIDOR];            /* kartan: var varje sida finns */
} Uni80;

/* Kortet utan start-PROM, i systemkartan och ABC80-läget. */
void uni80_starta(Uni80 *u);

/* Start-PROM:en, högst 2 KB. */
void uni80_startkrets(Uni80 *u, const uint8_t *data, size_t storlek);

/* OUT till porten $EF. */
void uni80_karta(Uni80 *u, uint8_t varde);

/* Var adressen finns i kartan som gäller: UNI80_ABC80, UNI80_RAM
 * (kortets RAM, på samma adress), UNI80_KRETS (start-PROM:en) eller
 * UNI80_EXTRA (kortets RAM där ABC80 inte har eget). */
static inline int uni80_var(const Uni80 *u, uint16_t adress)
{
    return adress >= UNI80_GEMENSAMT ? UNI80_RAM : u->sidor[adress / UNI80_SIDA];
}

#endif
