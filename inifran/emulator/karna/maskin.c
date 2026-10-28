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
    Maskin *m = dator;
    if (m->uni80)                            /* kartan på UNI-80 */
        switch (uni80_var(m->uni80, adress)) {
        case UNI80_RAM:
            return m->uni80->ram[adress];
        case UNI80_KRETS:
            return m->uni80->startkrets[adress % UNI80_STARTKRETS];
        case UNI80_EXTRA:
            if (m->skydd[adress])            /* inget eget RAM här */
                return m->uni80->ram[adress];
        }
    return m->minne[adress];
}

static void skriv_minne(void *dator, uint16_t adress, uint8_t varde)
{
    Maskin *m = dator;
    if (m->uni80)
        switch (uni80_var(m->uni80, adress)) {
        case UNI80_EXTRA:
            if (!m->skydd[adress])
                break;
            /* fallthrough */
        case UNI80_RAM:
            m->uni80->ram[adress] = varde;
            return;
        case UNI80_KRETS:
            if (m->krokar.skyddad_skrivning)
                m->krokar.skyddad_skrivning(m->krokar.data, m, adress, varde);
            return;
        }
    if (m->antal_banker && adress >= m->bankadress + 0x40
        && adress < m->bankadress + 0x40 + m->antal_banker) {
        memcpy(m->minne + m->bankadress, m->bank[adress - m->bankadress - 0x40],
               (size_t)m->bankstorlek);
        return;
    }
    if (!m->skydd[adress]) {
        m->minne[adress] = varde;
        /* TKN80: $5C00-$5FFF och $7C00-$7FFF är samma bytes. */
        if (m->tkn80 && (adress & 0xDC00) == 0x5C00)
            m->minne[adress ^ 0x2000] = varde;
    } else if (m->krokar.skyddad_skrivning)
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
/* TKN80                                                             */

/* 40 eller 80 tecken: TKN80-III:s block eller ROM:ens egna på
 * $0000-$03FF och $2000-$23FF. */
static void tkn80_lage(Maskin *m, int atti)
{
    if (m->tkn80_80 == atti)
        return;
    m->tkn80_80 = atti;
    memcpy(m->minne, m->tkn80_block[atti], 0x400);
    memcpy(m->minne + 0x2000, m->tkn80_block[atti] + 0x400, 0x400);
}

int maskin_tkn80(Maskin *m, const uint8_t *krets, size_t storlek)
{
    if (storlek != 0x1000)
        return 0;
    /* EPROM:en har två block om 2 KB, för 9913 och för 11273. Båda är
       ROM:ens kod med några få konstanter ändrade (52 bytes), så det
       block som skiljer sig minst från ROM:en hör till den. */
    int basta = 0, minst = 0x800;
    for (int b = 0; b < 2; b++) {
        int olika = 0;
        for (int k = 0; k < 0x400; k++)
            olika += (krets[b * 0x800 + k] != m->minne[k])
                   + (krets[b * 0x800 + 0x400 + k] != m->minne[0x2000 + k]);
        if (olika < minst) {
            minst = olika;
            basta = b;
        }
    }
    if (minst >= 0x100)
        return 0;
    memcpy(m->tkn80_block[0], m->minne, 0x400);
    memcpy(m->tkn80_block[0] + 0x400, m->minne + 0x2000, 0x400);
    memcpy(m->tkn80_block[1], krets + basta * 0x800, 0x800);
    maskin_ram(m, MASKIN_TKN80, 0x800, ' ');
    memcpy(m->minne + 0x5C00, m->minne + MASKIN_BILDMINNE, 0x400);
    m->tkn80 = 1;
    m->tkn80_80 = 0;
    tkn80_lage(m, 1);                        /* kortet startar med 80 tecken */
    return 1;
}

const uint8_t *maskin_skarm(const Maskin *m, int *kolumner)
{
    *kolumner = m->tkn80_80 ? 80 : 40;
    return m->minne + (m->tkn80_80 ? MASKIN_TKN80 : MASKIN_BILDMINNE);
}

/* ---------------------------------------------------------------- */
/* Portarna                                                          */

static uint8_t las_port(void *dator, uint16_t port)
{
    Maskin *m = dator;
    /* TKN80 läser port 3 och 4 (adressbit 3 och 5-7 spelar ingen roll,
       som i MAME); på bussen går de bara att skriva till. */
    if (m->tkn80 && ((port & 0x17) == 3 || (port & 0x17) == 4)) {
        tkn80_lage(m, (port & 0x17) == 4);
        return 0xFF;
    }
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
    if (m->uni80 && (port & 0xFF) == UNI80_PORT)
        uni80_karta(m->uni80, varde);        /* kortet avkodar hela porten */
    else if (!(port & 0x10))
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

void tangentko_hall(Maskin *m, long ms)
{
    tangentko_in(m, -1 - (int)ms);
}

int tangentko_tom(const Maskin *m)
{
    return m->ko_forst == m->ko_sist && !m->tangent_nere && m->tid >= m->ko_nasta;
}

/* Väntar PIO A på att bit 7 går låg? Ett avbrottsord med mask (bit 4)
 * och aktivt låg (bit 5 = 0), där masken släpper fram bit 7, som $97
 * och $7F: avbrott när tangenten släpps. ROM:en ställer $B7, nedtryckt. */
static int pio_a_slapp(const Pio *p)
{
    return (p->avbrottsord & 0x30) == 0x10 && !(p->mask & 0x80);
}

/* Nästa steg i kön: släpp tangenten som är nere, tryck ned nästa
 * eller vänta. I läge 3 ger PIO A INT när tangenten trycks ned, eller
 * när den släpps om avbrottsordet säger så (pio_a_slapp). */
static void tangentko_steg(Maskin *m)
{
    if (m->tangent_nere) {
        m->tangent &= 0x7F;
        m->tangent_nere = 0;
        if (m->pio_a.lage == 3 && m->pio_a.avbrott && pio_a_slapp(&m->pio_a))
            m->pio_a.vantar = 1;
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
    long nere = m->tryck_ms;
    if (post < 0) {                          /* hålltiden, och tangenten */
        nere = -1 - post;
        if (m->ko_forst == m->ko_sist)
            return;
        post = m->tangentko[m->ko_forst];
        m->ko_forst = (m->ko_forst + 1) % TANGENTKO_STORLEK;
    }
    m->tangent = (uint8_t)(post | 0x80);
    m->tangent_nere = 1;
    m->fysisk = 0;                           /* kön tar över */
    if (m->pio_a.lage == 3 && m->pio_a.avbrott && !pio_a_slapp(&m->pio_a))
        m->pio_a.vantar = 1;
    m->ko_nasta += MASKIN_MS(nere);
}

static void fysisk_slapp(Maskin *m)
{
    m->fysisk = 0;
    if (m->tangent_nere)
        return;                              /* kön har tagit över */
    m->tangent &= 0x7F;
    if (m->pio_a.lage == 3 && m->pio_a.avbrott && pio_a_slapp(&m->pio_a))
        m->pio_a.vantar = 1;
}

/* En tangent som kommer inom ett tangentvarv (tryck_ms + slapp_ms) från
 * den förra går genom kön, så att tolken hinner läsa LASTKY. */
void maskin_tangent_ned(Maskin *m, uint8_t kod)
{
    long long varv = m->fysisk_ned + MASKIN_MS(m->tryck_ms + m->slapp_ms);
    if (!tangentko_tom(m) || (m->fysisk && m->tid < varv)) {
        int tom = m->ko_forst == m->ko_sist && !m->tangent_nere;
        tangentko_lagg(m, kod);
        if (tom && m->fysisk && m->ko_nasta < varv)
            m->ko_nasta = varv;
        return;
    }
    m->tangent = (uint8_t)(kod | 0x80);
    m->fysisk = 1;
    m->fysisk_ned = m->tid;
    m->fysisk_upp = m->tid + MASKIN_MS(m->tryck_ms);
    if (m->pio_a.lage == 3 && m->pio_a.avbrott && !pio_a_slapp(&m->pio_a))
        m->pio_a.vantar = 1;
}

void maskin_tangent_upp(Maskin *m)
{
    if (m->fysisk != 1)
        return;
    if (m->tid >= m->fysisk_upp)
        fysisk_slapp(m);
    else
        m->fysisk = 2;                       /* släpps efter tryck_ms */
}

/* ---------------------------------------------------------------- */
/* V24 och labplattan                                                */

void v24_lagg(Maskin *m, uint8_t tecken)
{
    if (m->antal_v24 < V24_STORLEK)
        m->v24[m->antal_v24++] = tecken;
}

void v24_ny(Maskin *m)
{
    m->antal_v24 = 0;
    m->v24_start = -1;
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
    /* Före första OUT läses bit 3, 4 och 6 som 1 och bit 5 som 0, så att
     * ett program som läser porten och skriver tillbaka (BASIC II:s PROUT,
     * HR80:s rutin) inte drar kassettmotorns relä. */
    m->pio_b_skrivet = 0xDF;
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
    if (m->fysisk == 2 && m->tid >= m->fysisk_upp)
        fysisk_slapp(m);
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
        uint16_t pc = m->z80.pc, sp = m->z80.sp;
        uint8_t operation = las_minne(m, pc);
        /* PIO:n ser RETI ($ED $4D) på bussen: PIO B:s avbrott är klart. */
        int reti = operation == 0xED && las_minne(m, (uint16_t)(pc + 1)) == 0x4D;
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
            /* UNI-80 släpper bara fram NMI när ABC80 syns (uni80.h). */
            if (!m->utan_nmi && !(m->uni80 && m->uni80->karta != UNI80_SYSTEM))
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
