/*
 * anrop.c -- anropsläget och loggningen, till boken ABC80 inifrån
 *
 * Anropsläget (KOLL, prov av enskilda rutiner i ROM:en): efter starten
 * läggs bytes i minnet (-p, också i ROM-området), registren sätts (-R),
 * en vaktadress läggs på stacken och PC sätts till rutinen (-x).
 * Rutinen är klar när PC når vaktadressen med SP tillbaka där den var
 * före anropet (eller högre, om rutinen tog argument från stacken, eller
 * upp till -G bytes lägre, om den lämnar ett större resultat). Sedan
 * skrivs registren och minnet (-d), och med -V skärmen.
 *
 * Krokarna i maskinen sköter också filenheten TST: (tstenhet.c), -P
 * (registren på standard fel när PC når en adress), -c (varje nytt mål
 * för CALL och RST under anropet), -O (OUT till en port på standard fel)
 * och -w (skrivningar till skrivskyddat minne på standard fel).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vard.h"

#define VAKT 0xDEAD                          /* återhoppsadressen */

static char flagga(uint8_t f, uint8_t bit, char tecken)
{
    return f & bit ? tecken : '-';
}

static void skriv_registren_kort(const Z80 *z)
{
    fprintf(stderr, "PC=%04X A=%02X F=%02X [%c%c%c%c] BC=%02X%02X DE=%02X%02X "
            "HL=%02X%02X IX=%04X SP=%04X\n", z->pc, z->a, z->f,
            flagga(z->f, Z80_S, 'S'), flagga(z->f, Z80_Z, 'Z'),
            flagga(z->f, Z80_P, 'P'), flagga(z->f, Z80_C, 'C'),
            z->b, z->c, z->d, z->e, z->h, z->l, z->ix, z->sp);
}

/* ---------------------------------------------------------------- */
/* Krokarna                                                          */

static int fore_steg(void *data, Maskin *m)
{
    Vard *v = data;
    Z80 *z = &m->z80;
    if (v->anrop_igang && z->pc == VAKT && z->sp + v->stacken_vaxer >= v->anrop_sp) {
        v->anrop_klart = 1;
        return 1;
    }
    if (v->tst && tst_drivrutin(v))
        tst_anrop(v);
    for (int k = 0; k < v->antal_bevakade; k++)
        if (z->pc == v->bevakade[k])
            skriv_registren_kort(z);
    return 0;
}

/* -c: en CALL eller RST har lagt returadressen på stacken. */
static void efter_steg(void *data, Maskin *m, uint8_t operation, uint16_t pc, uint16_t sp)
{
    Vard *v = data;
    uint16_t mal = m->z80.pc;
    int anrop = operation == 0xCD                  /* CALL nn */
             || (operation & 0xC7) == 0xC4          /* CALL cc,nn */
             || (operation & 0xC7) == 0xC7;         /* RST */
    if (v->anrop_igang && v->visa_anrop && anrop && m->z80.sp == (uint16_t)(sp - 2)
        && !v->sedda_anrop[mal]) {
        v->sedda_anrop[mal] = 1;
        printf("  anrop $%04X (från $%04X)\n", mal, pc);
    }
}

static void port_ut(void *data, Maskin *m, uint8_t port, uint8_t varde)
{
    Vard *v = data;
    if (port == v->logga_port)
        fprintf(stderr, "OUT %02X,%02X %lld\n", port, varde, m->instruktioner);
}

static void skyddad_skrivning(void *data, Maskin *m, uint16_t adress, uint8_t varde)
{
    Vard *v = data;
    if (v->logga_skrivningar)
        fprintf(stderr, "skriv $%04X = $%02X (PC $%04X)\n", adress, varde, m->z80.pc);
}

void anrop_krokar(Vard *v)
{
    Krokar *k = &v->maskin->krokar;
    k->data = v;
    k->fore_steg = v->anrop_adress >= 0 || v->tst || v->antal_bevakade ? fore_steg : NULL;
    k->efter_steg = v->visa_anrop ? efter_steg : NULL;
    k->port_ut = v->logga_port >= 0 ? port_ut : NULL;
    k->skyddad_skrivning = v->logga_skrivningar ? skyddad_skrivning : NULL;
}

/* ---------------------------------------------------------------- */
/* Anropet                                                           */

static void satt_register(Z80 *z, const char *namn, unsigned varde)
{
    struct { const char *namn; uint8_t *hog, *lag; } par[] = {
        { "HL", &z->h, &z->l }, { "DE", &z->d, &z->e }, { "BC", &z->b, &z->c },
        { "AF", &z->a, &z->f },
    };
    struct { const char *namn; uint8_t *r; } enkla[] = {
        { "A", &z->a }, { "F", &z->f }, { "B", &z->b }, { "C", &z->c },
        { "D", &z->d }, { "E", &z->e }, { "H", &z->h }, { "L", &z->l },
    };
    for (size_t k = 0; k < sizeof par / sizeof par[0]; k++)
        if (!strcmp(namn, par[k].namn)) {
            *par[k].hog = (uint8_t)(varde >> 8);
            *par[k].lag = (uint8_t)varde;
            return;
        }
    for (size_t k = 0; k < sizeof enkla / sizeof enkla[0]; k++)
        if (!strcmp(namn, enkla[k].namn)) {
            *enkla[k].r = (uint8_t)varde;
            return;
        }
    if (!strcmp(namn, "IX")) z->ix = (uint16_t)varde;
    else if (!strcmp(namn, "IY")) z->iy = (uint16_t)varde;
    else if (!strcmp(namn, "SP")) z->sp = (uint16_t)varde;
    else vard_fel("okänt register ", namn);
}

void anrop_borja(Vard *v)
{
    Maskin *m = v->maskin;
    Z80 *z = &m->z80;
    for (int k = 0; k < v->antal_andringar; k++) {
        char *s;
        int adress = (int)strtol(v->andringar[k], &s, 16);
        if (*s++ != '=')
            vard_fel("-p adr=hh,...: ", v->andringar[k]);
        while (*s) {
            m->minne[adress++ & 0xFFFF] = (uint8_t)strtol(s, &s, 16);
            if (*s == ',')
                s++;
        }
    }
    if (v->register_text) {
        char text[512];
        snprintf(text, sizeof text, "%s", v->register_text);
        for (char *t = strtok(text, ","); t; t = strtok(NULL, ",")) {
            char *lika = strchr(t, '=');
            if (!lika)
                vard_fel("-R REG=hhhh: ", t);
            *lika = 0;
            satt_register(z, t, (unsigned)strtol(lika + 1, NULL, 16));
        }
    }
    v->anrop_sp = z->sp;
    m->minne[--z->sp] = VAKT >> 8;
    m->minne[--z->sp] = VAKT & 0xFF;
    z->pc = (uint16_t)v->anrop_adress;
    v->anrop_igang = 1;
    v->anrop_instruktioner = m->instruktioner;
}

void anrop_skriv(const Vard *v)
{
    const Maskin *m = v->maskin;
    const Z80 *z = &m->z80;
    if (!v->anrop_klart)
        printf("STANNADE INTE (PC $%04X)\n", z->pc);
    printf("A=%02X F=%02X [%c%c%c%c] BC=%02X%02X DE=%02X%02X HL=%02X%02X "
           "IX=%04X IY=%04X SP=%04X  (%lld instr.)\n", z->a, z->f,
           flagga(z->f, Z80_S, 'S'), flagga(z->f, Z80_Z, 'Z'),
           flagga(z->f, Z80_P, 'P'), flagga(z->f, Z80_C, 'C'),
           z->b, z->c, z->d, z->e, z->h, z->l, z->ix, z->iy, z->sp,
           m->instruktioner - v->anrop_instruktioner);
    for (int k = 0; k < v->antal_visa_minne; k++)
        vard_skriv_minne(m, v->visa_minne[k]);
}
