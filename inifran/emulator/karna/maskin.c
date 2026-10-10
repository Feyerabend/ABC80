/*
 * maskin.c -- ABC80 runt processorn, till boken ABC80 inifrån
 *
 * Se maskin.h för vad som finns med och varifrån uppgifterna kommer.
 * Ingenting här läser filer, skriver ut eller frågar efter klockan.
 */

#include <string.h>

#include "maskin.h"

/* ---------------------------------------------------------------- */
/* Minnet                                                            */

static uint8_t las_minne(void *dator, uint16_t adress)
{
    return ((Maskin *)dator)->minne[adress];
}

static void skriv_minne(void *dator, uint16_t adress, uint8_t varde)
{
    Maskin *m = dator;
    if (m->antal_banker && adress >= m->bankadress + 0x40
        && adress < m->bankadress + 0x40 + m->antal_banker) {
        memcpy(m->minne + m->bankadress, m->bank[adress - m->bankadress - 0x40],
               (size_t)m->bankstorlek);
        return;
    }
    if (!m->skydd[adress])
        m->minne[adress] = varde;
    else if (m->krokar.skyddad_skrivning)
        m->krokar.skyddad_skrivning(m->krokar.data, m, adress, varde);
}

/* ---------------------------------------------------------------- */
/* V24 in och labplattan på PIO B                                    */

/* Linjen RxD vid tiden nu: 8 bitar, ingen paritet, en stoppbit, och
 * 1 i vila. */
static int v24_linje(const Maskin *m)
{
    if (m->v24_start < 0 || m->tid < m->v24_start)
        return 1;
    long long bit = (m->tid - m->v24_start) * m->baud / MASKIN_KLOCKA;
    long long tecken = bit / 10;
    int bit_i_tecknet = (int)(bit % 10);
    if (tecken >= m->antal_v24 || bit_i_tecknet == 9)
        return 1;                            /* stoppbiten */
    if (bit_i_tecknet == 0)
        return 0;                            /* startbiten */
    return m->v24[tecken] >> (bit_i_tecknet - 1) & 1;
}

/* Strömbrytarna på labplattan vid tiden nu. Öppna ingångar läses som 1. */
static int labb_brytare(const Maskin *m)
{
    int brytare = 7;
    for (int k = 0; k < m->antal_labb; k++)
        if (m->tid >= m->labb_tid[k])
            brytare = m->labb[k];
    return brytare;
}

/* ---------------------------------------------------------------- */
/* Portarna                                                          */

static uint8_t las_port(void *dator, uint16_t port)
{
    Maskin *m = dator;
    if (!(port & 0x10))                      /* adressbit 4 = 0: ABC-bussen */
        return bussen_in(&m->bussen, port & 0xFF);
    switch (port & 0xFF) {
    case 0x38:
        return m->tangent;
    case 0x3A: {                             /* bit 3-6 läses tillbaka */
        uint8_t varde;
        if (m->pio_b_enhet)
            varde = m->pio_b_enhet->las(m->pio_b_enhet->data, m->pio_b_skrivet, v24_linje(m));
        else if (m->antal_labb)
            varde = (m->pio_b_skrivet & 0x78) | 0x80 | (labb_brytare(m) & (6 | v24_linje(m)));
        else
            varde = (m->pio_b_skrivet & 0x78) | 0x86 | v24_linje(m);
        if (m->kassett && m->kassett->latch)
            varde &= 0x7F;                   /* en flank har kommit */
        return varde;
    }
    default:
        return 0xFF;
    }
}

/* Ett styrord till en port i PIO:n. Efter läge 3 kommer riktningen,
 * som här inte används, och efter ett avbrottsord med bit 4 masken. */
static void pio_styrord(Pio *p, uint8_t varde)
{
    if (p->riktning_kommer) {
        p->riktning_kommer = 0;
        return;
    }
    if (p->mask_kommer) {
        p->mask_kommer = 0;
        p->mask = varde;
        return;
    }
    if ((varde & 0x0F) == 0x0F) {            /* läget */
        p->lage = varde >> 6;
        p->riktning_kommer = p->lage == 3;
    } else if ((varde & 0x0F) == 0x07) {     /* avbrott, med eller utan mask */
        p->avbrott = varde >> 7;
        p->avbrottsord = varde;
        p->mask_kommer = (varde & 0x10) != 0;
    } else if ((varde & 0x0F) == 0x03)       /* avbrott på eller av */
        p->avbrott = varde >> 7;
    else if (!(varde & 1))                   /* vektorn */
        p->vektor = varde;
    if (!p->avbrott)
        p->vantar = 0;
}

/* Kassettens latch har satts, så bit 7 i PIO B har gått låg. Det ger
 * INT i läge 3 om bit 7 övervakas och avbrottet är aktivt lågt (RCVSTA
 * skriver $97 och masken $7F). */
static void pio_b_flank(Maskin *m)
{
    Pio *p = &m->pio_b;
    if (p->lage == 3 && p->avbrott && !(p->mask & 0x80) && !(p->avbrottsord & 0x20))
        p->vantar = 1;
}

static void skriv_port(void *dator, uint16_t port, uint8_t varde)
{
    Maskin *m = dator;
    if (m->krokar.port_ut)
        m->krokar.port_ut(m->krokar.data, m, (uint8_t)port, varde);
    if ((port & 0xFF) == 6 && m->ljud)
        ljud_skriv(m->ljud, m->tid, varde);
    if (!(port & 0x10))
        bussen_ut(&m->bussen, port & 0xFF, varde);
    else if ((port & 0xFF) == 0x39)
        pio_styrord(&m->pio_a, varde);
    else if ((port & 0xFF) == 0x3B)
        pio_styrord(&m->pio_b, varde);
    else if ((port & 0xFF) == 0x3A) {
        m->pio_b_skrivet = varde;
        if (m->pio_b_enhet)
            m->pio_b_enhet->skriv(m->pio_b_enhet->data, varde);
        if (m->kassett) {
            kassett_skriv(m->kassett, m->tid, varde);
            if (!m->kassett->latch)
                m->pio_b.vantar = 0;         /* bit 7 är hög igen */
        }
    }
}

/* ---------------------------------------------------------------- */
/* Tangentkön                                                        */

static void tangentko_in(Maskin *m, int post)
{
    /* En tom kö börjar om sitt schema nu; annars räknas varje post från
       när den förra skulle sluta, så att tiderna inte glider. */
    if (m->ko_forst == m->ko_sist && !m->tangent_nere && m->tid > m->ko_nasta)
        m->ko_nasta = m->tid;
    int nasta = (m->ko_sist + 1) % TANGENTKO_STORLEK;
    if (nasta != m->ko_forst) {
        m->tangentko[m->ko_sist] = post;
        m->ko_sist = nasta;
    }
}

void tangentko_lagg(Maskin *m, uint8_t kod)
{
    tangentko_in(m, kod);
}

void tangentko_paus(Maskin *m, long ms)
{
    tangentko_in(m, 256 + (int)ms);
}

int tangentko_tom(const Maskin *m)
{
    return m->ko_forst == m->ko_sist && !m->tangent_nere && m->tid >= m->ko_nasta;
}

/* Nästa steg i kön: släpp tangenten som är nere, tryck ned nästa
 * eller vänta. En nedtryckt tangent ger INT om PIO A är i läge 3. */
static void tangentko_steg(Maskin *m)
{
    if (m->tangent_nere) {
        m->tangent &= 0x7F;
        m->tangent_nere = 0;
        m->ko_nasta += MASKIN_MS(m->slapp_ms);
        return;
    }
    if (m->ko_forst == m->ko_sist)
        return;
    int post = m->tangentko[m->ko_forst];
    m->ko_forst = (m->ko_forst + 1) % TANGENTKO_STORLEK;
    if (post >= 256) {
        m->ko_nasta += MASKIN_MS(post - 256);
        return;
    }
    m->tangent = (uint8_t)(post | 0x80);
    m->tangent_nere = 1;
    if (m->pio_a.lage == 3 && m->pio_a.avbrott)
        m->pio_a.vantar = 1;
    m->ko_nasta += MASKIN_MS(m->tryck_ms);
}

/* ---------------------------------------------------------------- */
/* V24 och labplattan                                                */

void v24_lagg(Maskin *m, uint8_t tecken)
{
    if (m->antal_v24 < V24_STORLEK)
        m->v24[m->antal_v24++] = tecken;
}

void v24_starta(Maskin *m, long long start)
{
    m->v24_start = start;
}

long long v24_tid(const Maskin *m)
{
    return (long long)m->antal_v24 * 10 * MASKIN_KLOCKA / m->baud;
}

void labb_lagg(Maskin *m, int brytare, long ms)
{
    if (!m->antal_labb)                      /* labbplattans utgångar börjar låga */
        m->pio_b_skrivet = 0;
    if (m->antal_labb < LABB_STORLEK) {
        m->labb[m->antal_labb] = brytare & 7;
        m->labb_tid[m->antal_labb++] = MASKIN_MS(ms);
    }
}

/* ---------------------------------------------------------------- */
/* Starten och minnets innehåll                                      */

void maskin_starta(Maskin *m, int ram_kb)
{
    memset(m, 0, sizeof *m);
    memset(m->minne, 0xFF, sizeof m->minne);
    memset(m->skydd, 1, sizeof m->skydd);
    memset(m->skydd + MASKIN_BILDMINNE, 0, 0x400);
    memset(m->minne + MASKIN_BILDMINNE, ' ', 0x400);
    int ram = 0x10000 - ram_kb * 1024;
    if (ram < 0x8000)
        ram = 0x8000;
    memset(m->skydd + ram, 0, (size_t)(0x10000 - ram));
    memset(m->bank, 0xFF, sizeof m->bank);
    bussen_tom(&m->bussen);

    m->pio_a.lage = 1;
    m->pio_a.vektor = 0x34;
    m->pio_b_skrivet = 0xFF;                 /* bit 3-6 läses som 1 före första OUT */
    m->nasta_nmi = MASKIN_BILD;
    m->tryck_ms = 40;
    m->slapp_ms = 60;
    m->v24_start = -1;
    m->baud = 1200;
}

void maskin_ram(Maskin *m, uint16_t adress, size_t storlek, uint8_t fyllnad)
{
    if (storlek > (size_t)(0x10000 - adress))
        storlek = (size_t)(0x10000 - adress);
    memset(m->minne + adress, fyllnad, storlek);
    memset(m->skydd + adress, 0, storlek);
}

uint8_t *maskin_cmos(Maskin *m, uint16_t adress, const uint8_t *data, size_t storlek)
{
    if (storlek > (size_t)(0x10000 - adress))
        storlek = (size_t)(0x10000 - adress);
    memcpy(m->minne + adress, data, storlek);
    memset(m->skydd + adress, 0, storlek);
    return m->minne + adress;
}

void maskin_krets(Maskin *m, uint16_t adress, const uint8_t *data, size_t storlek)
{
    if (storlek > (size_t)(0x10000 - adress))
        storlek = (size_t)(0x10000 - adress);
    memcpy(m->minne + adress, data, storlek);
    memset(m->skydd + adress, 1, storlek);
}

int maskin_rom(Maskin *m, const uint8_t *data, size_t storlek)
{
    if (storlek != 0x4000 && storlek != 0x8000)
        return 0;
    maskin_krets(m, 0, data, storlek);
    if (storlek == 0x8000) {                 /* BASIC II: bildminnet är kvar */
        memset(m->skydd + MASKIN_BILDMINNE, 0, 0x400);
        memset(m->minne + MASKIN_BILDMINNE, ' ', 0x400);
    }
    return 1;
}

void maskin_bank(Maskin *m, uint16_t adress, const uint8_t *data, size_t storlek)
{
    if (m->antal_banker == BANKER)
        return;
    if (storlek > BANKSTORLEK)
        storlek = BANKSTORLEK;
    memcpy(m->bank[m->antal_banker++], data, storlek);
    if ((int)storlek > m->bankstorlek)
        m->bankstorlek = (int)storlek;
    m->bankadress = adress;
    memcpy(m->minne + adress, m->bank[0], (size_t)m->bankstorlek);
    memset(m->skydd + adress, 1, (size_t)m->bankstorlek);
}

void maskin_aterstall(Maskin *m)
{
    m->z80.dator = m;
    m->z80.las = las_minne;
    m->z80.skriv = skriv_minne;
    m->z80.in = las_port;
    m->z80.ut = skriv_port;
    z80_reset(&m->z80);
}

/* ---------------------------------------------------------------- */
/* Körningen                                                         */

/* Tiden går fram t T-cykler: linjesignalen, tangentkön och bandet
 * följer med. */
static void ga_fram(Maskin *m, int t)
{
    m->tid += t;
    /* Linjesignalen: 15 625 / 2 gånger i sekunden. */
    m->linjesignal += (long long)t * 15625;
    if (m->linjesignal >= 2 * MASKIN_KLOCKA) {
        m->linjesignal -= 2 * MASKIN_KLOCKA;
        if (m->pio_a.lage == 1 && m->pio_a.avbrott)
            m->pio_a.vantar = 1;
    }
    if (m->tid >= m->ko_nasta)
        tangentko_steg(m);
    if (m->kassett && kassett_fram(m->kassett, m->tid))
        pio_b_flank(m);
}

void maskin_kor(Maskin *m, long long tcykler)
{
    long long slut = m->tid + tcykler;
    while (m->tid < slut && !m->stoppad) {
        if (m->krokar.fore_steg && m->krokar.fore_steg(m->krokar.data, m)) {
            m->stoppad = 1;
            return;
        }
        uint8_t operation = m->minne[m->z80.pc];
        uint16_t pc = m->z80.pc, sp = m->z80.sp;
        /* PIO:n ser RETI ($ED $4D) på bussen: PIO B:s avbrott är klart. */
        int reti = operation == 0xED && m->minne[(uint16_t)(pc + 1)] == 0x4D;
        int t = z80_steg(&m->z80);
        if (reti)
            m->pio_b.betjanas = 0;
        m->instruktioner++;
        if (m->krokar.efter_steg)
            m->krokar.efter_steg(m->krokar.data, m, operation, pc, sp);
        /* Ett avbrott som kom under förra instruktionen tas nu. */
        int avbrottet = 0;
        if (m->tid >= m->nasta_nmi) {
            m->nasta_nmi += MASKIN_BILD;
            if (!m->utan_nmi)
                avbrottet = z80_nmi(&m->z80);
        } else if (m->pio_a.vantar) {
            avbrottet = z80_int(&m->z80, m->pio_a.vektor);
            if (avbrottet)
                m->pio_a.vantar = 0;
        } else if (m->pio_b.vantar && !m->pio_b.betjanas) {
            avbrottet = z80_int(&m->z80, m->pio_b.vektor);
            if (avbrottet) {
                m->pio_b.vantar = 0;
                m->pio_b.betjanas = 1;
            }
        }
        if (m->instruktioner_per_bild)
            /* Tiden i instruktioner (som abc80host): alla tar lika lång
               tid och avbrotten ingen. */
            t = (int)(m->instruktioner * MASKIN_BILD / m->instruktioner_per_bild - m->tid);
        else
            t += avbrottet;
        ga_fram(m, t);
    }
}
