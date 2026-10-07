/*
 * z80.h -- Z80-processorn, till boken ABC80 inifrån
 *
 * Processorn är en struct med registren och fyra funktioner som
 * datorn runt den ger: läs och skriv minnet, läs och skriv en port.
 * z80_steg kör en instruktion och lämnar antalet T-cykler den tog.
 */

#ifndef Z80_INC_H
#define Z80_INC_H

#include <stdint.h>

typedef struct Z80 Z80;

struct Z80 {
    uint8_t a, f, b, c, d, e, h, l;          /* huvudregistren */
    uint8_t a2, f2, b2, c2, d2, e2, h2, l2;  /* de alternativa (EX AF,AF' och EXX) */
    uint16_t ix, iy, sp, pc;
    uint8_t i, r;
    uint8_t iff1, iff2;                      /* avbrott tillåtna; kopian för NMI */
    uint8_t im;                              /* avbrottsläge 0, 1 eller 2 */
    uint8_t halt;                            /* HALT: väntar på ett avbrott */
    uint8_t eivant;                          /* EI nyss: inget avbrott förrän
                                                efter nästa instruktion */
    uint8_t pfx;                             /* inne i steget: $DD, $FD eller 0 */
    int extra;                               /* inne i steget: extra T-cykler */

    void *dator;                             /* skickas med till funktionerna */
    uint8_t (*las)(void *dator, uint16_t adr);
    void (*skriv)(void *dator, uint16_t adr, uint8_t v);
    uint8_t (*in)(void *dator, uint16_t port);
    void (*ut)(void *dator, uint16_t port, uint8_t v);
};

/* Flaggorna i F. Y och X är bit 5 och 3, som Zilog inte beskriver. */
enum {
    Z80_S = 0x80, Z80_Z = 0x40, Z80_Y = 0x20, Z80_H = 0x10,
    Z80_X = 0x08, Z80_P = 0x04, Z80_N = 0x02, Z80_C = 0x01
};

void z80_reset(Z80 *z);
int  z80_steg(Z80 *z);                       /* en instruktion; T-cykler */
int  z80_nmi(Z80 *z);                        /* T-cykler */
int  z80_int(Z80 *z, uint8_t vektor);        /* 0 om avbrottet inte togs */

#endif
