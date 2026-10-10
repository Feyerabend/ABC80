/*
 * bussen.c -- ABC-bussen, till boken ABC80 inifrån
 *
 * Se bussen.h.
 */

#include "bussen.h"

void bussen_tom(Bussen *b)
{
    b->antal = 0;
    b->valt = -1;
}

void bussen_koppla(Bussen *b, Kort *kort)
{
    if (b->antal < BUSSEN_KORT)
        b->kort[b->antal++] = kort;
}

/* Ta ur kortet; de andra behåller sin ordning. */
void bussen_koppla_ur(Bussen *b, Kort *kort)
{
    for (int k = 0; k < b->antal; k++)
        if (b->kort[k] == kort) {
            for (int j = k + 1; j < b->antal; j++)
                b->kort[j - 1] = b->kort[j];
            b->antal--;
            return;
        }
}

/* Det valda kortet, eller NULL. */
static Kort *valt_kort(const Bussen *b)
{
    for (int k = 0; k < b->antal; k++)
        if (b->kort[k]->svarar(b->kort[k]->data, b->valt))
            return b->kort[k];
    return 0;
}

uint8_t bussen_in(Bussen *b, int port)
{
    Kort *kort = valt_kort(b);
    if (!kort)
        return 0xFF;
    return kort->in(kort->data, kort->hela_porten ? port : port & 7);
}

void bussen_ut(Bussen *b, int port, uint8_t varde)
{
    if ((port & 7) == 1) {
        b->valt = varde & 0x3F;
        for (int k = 0; k < b->antal; k++)
            if (b->kort[k]->valj)
                b->kort[k]->valj(b->kort[k]->data, b->valt);
        return;
    }
    Kort *kort = valt_kort(b);
    if (kort)
        kort->ut(kort->data, kort->hela_porten ? port : port & 7, varde);
}
