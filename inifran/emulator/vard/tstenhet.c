/*
 * tstenhet.c -- filenheten TST: i BASIC II, till boken ABC80 inifrån
 *
 * En filenhet som värden sköter, för att prova BASIC II:s filhantering
 * utan skivor. Posten i enhetslistan ligger på $7800 (ROM-området, $FF
 * i BASIC II-ROM:en) och länkas in först i listan ($FF7B); drivrutinen
 * är ett RET på $7808, och när PC når den gör värden det rutinen ska.
 *
 * Enheten är en blockenhet som DOS:en: INPUT, PRINT, GET och PUT
 * (funktion 3-6) svarar med CY och A = $FF, så att BASIC:en själv
 * packar i bufferten, och funktion 7 och 8 läser och skriver blocket
 * DE (256 bytes) till och från buffertens sida (FCB+$14). Buffertarna
 * är $7900-$7BFF (görs till RAM), en per öppen fil. Filerna ligger i
 * värden, högst 16 om 64 block. OPEN av en fil som inte finns ger fel
 * 21 med DE orört; ett block bortom slutet ger fel 37 (filslut).
 */

#include <stdio.h>
#include <string.h>

#include "vard.h"

#define POSTEN       0x7800
#define DRIVRUTINEN  0x7808
#define LISTAN       0xFF7B                  /* första posten i enhetslistan */

static struct {
    char namn[12];                           /* 8 + 3 tecken */
    int block;
    uint8_t data[64][256];
} filer[16];
static int antal_filer;

static struct {
    uint16_t fcb;                            /* 0: ledig */
    int fil;
} oppna[3];

void tst_koppla(Vard *v)
{
    Maskin *m = v->maskin;
    uint8_t *minne = m->minne;
    minne[POSTEN] = minne[LISTAN];
    minne[POSTEN + 1] = minne[LISTAN + 1];
    memcpy(minne + POSTEN + 2, "TST", 3);
    minne[POSTEN + 5] = DRIVRUTINEN & 0xFF;
    minne[POSTEN + 6] = DRIVRUTINEN >> 8;
    minne[DRIVRUTINEN] = 0xC9;               /* RET */
    memset(m->skydd + 0x7900, 0, 0x300);    /* buffertarna: RAM, innehållet kvar */
    minne[LISTAN] = POSTEN & 0xFF;
    minne[LISTAN + 1] = POSTEN >> 8;
}

int tst_drivrutin(const Vard *v)
{
    return v->maskin->z80.pc == DRIVRUTINEN;
}

/* Återhoppet: CY och A = felet, eller ingen CY. */
static void svara(Z80 *z, int fel, uint8_t a)
{
    if (fel) {
        z->a = a;
        z->f |= Z80_C;
    } else
        z->f &= (uint8_t)~Z80_C;
}

static int oppen(uint16_t fcb)
{
    for (int k = 0; k < 3; k++)
        if (oppna[k].fcb == fcb)
            return k;
    return -1;
}

void tst_anrop(Vard *v)
{
    Maskin *m = v->maskin;
    Z80 *z = &m->z80;
    uint16_t de = (uint16_t)(z->d << 8 | z->e);
    int funktion = z->a, k = oppen(z->ix);

    if (funktion == 0 || funktion == 1) {    /* OPEN, PREPARE; DE = namnet */
        char namn[12];
        memcpy(namn, m->minne + de, 11);
        namn[11] = 0;
        int f;
        for (f = 0; f < antal_filer; f++)
            if (!memcmp(filer[f].namn, namn, 11))
                break;
        if (f == antal_filer) {
            if (funktion == 0 || antal_filer == 16) {
                svara(z, 1, 0x15);
                return;
            }
            memcpy(filer[antal_filer++].namn, namn, 12);
        }
        if (funktion == 1)
            filer[f].block = 0;
        if (k < 0)
            for (k = 0; k < 3; k++)
                if (!oppna[k].fcb)
                    break;
        if (k == 3) {
            svara(z, 1, 0x15);
            return;
        }
        oppna[k].fcb = z->ix;
        oppna[k].fil = f;
        z->c = 0;
        z->d = (uint8_t)(0x79 + k);         /* bufferten */
        svara(z, 0, 0);
    } else if (funktion == 2) {              /* CLOSE */
        if (k >= 0)
            oppna[k].fcb = 0;
        svara(z, 0, 0);
    } else if (funktion >= 3 && funktion <= 6) {
        svara(z, 1, 0xFF);                   /* BASIC:en packar själv */
    } else if ((funktion == 7 || funktion == 8) && k >= 0) {
        int f = oppna[k].fil;
        uint8_t *buffert = m->minne + (m->minne[(uint16_t)(z->ix + 0x14)] << 8);
        if (de >= 64) {
            svara(z, 1, 0x25);
            return;
        }
        if (funktion == 7) {                 /* läs blocket */
            if (de >= filer[f].block) {
                svara(z, 1, 0x25);
                return;
            }
            memcpy(buffert, filer[f].data[de], 256);
        } else {                             /* skriv blocket */
            memcpy(filer[f].data[de], buffert, 256);
            if (de >= filer[f].block)
                filer[f].block = de + 1;
        }
        svara(z, 0, 0);
    } else
        svara(z, 1, 0x15);
}

void tst_skriv(void)
{
    for (int f = 0; f < antal_filer; f++) {
        printf("FIL %.8s.%.3s %d block\n", filer[f].namn, filer[f].namn + 8, filer[f].block);
        for (int b = 0; b < filer[f].block; b++) {
            const uint8_t *p = filer[f].data[b];
            int n = 256;
            while (n > 3 && p[n - 1] == p[255])
                n--;
            for (int j = 0; j < n; j += 16) {
                printf("%02X%02X:", b, j);
                for (int q = j; q < j + 16 && q < n; q++)
                    printf(" %02X", p[q]);
                printf("\n");
            }
        }
    }
}
