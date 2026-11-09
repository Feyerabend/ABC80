/*
 * abc802.c -- ABC 802 runt processorn, till ABC 802 inifrån
 *
 * Se abc802.h för vad som finns med och varifrån uppgifterna kommer.
 * Bilden och texten på skärmen ligger i skarm802.c.
 */

#include <string.h>

#include "abc802.h"

/* Tangentbordets tecken: 11 bitar i 19 200 baud (?). */
#define TANGENT_TECKEN_TID (11 * ABC802_KLOCKA / 19200)

/* ---------------------------------------------------------------- */
/* Minnet                                                            */

/* 64 KB RAM: DTR på DART B (WR5 bit 7) ger LRS*. */
static int bara_ram(const Abc802 *m)
{
    return m->dart.k[1].wr[5] & 0x80;
}

static uint8_t las_minne(void *dator, uint16_t adress)
{
    Abc802 *m = dator;
    if (adress >= 0x8000 || bara_ram(m))
        return m->ram[adress];
    if (adress < 0x7800)
        return m->hamtning_7800 ? m->ram[adress] : m->rom[adress];
    return m->hamtning_7800 ? m->rom[adress] : m->bildminne[adress & 0x7FF];
}

static void skriv_minne(void *dator, uint16_t adress, uint8_t varde)
{
    Abc802 *m = dator;
    if (adress >= 0x8000 || bara_ram(m))
        m->ram[adress] = varde;
    else if (adress < 0x7800) {
        if (m->hamtning_7800)
            m->ram[adress] = varde;
    } else if (!m->hamtning_7800)
        m->bildminne[adress & 0x7FF] = varde;
}

/* ---------------------------------------------------------------- */
/* Klockorna till de seriella kanalerna                              */

/* T-cykler per tecken: startbiten, databitarna, pariteten och
 * stoppbitarna (asynkront) eller 8 bitar (synkront), gånger klockans
 * delning (WR4 bit 6-7) och perioden från CTC:n. databitar är WR3 eller
 * WR5 skiftat så att bit 0-1 är fältet. */
static long tecken_tid(const SioKanal *k, int synkron, int databitar, long period)
{
    static const int delning[4] = { 1, 16, 32, 64 };
    static const int bitar[4] = { 5, 7, 6, 8 };
    if (synkron)
        return 8 * period;
    long halva = 2 * (1 + bitar[databitar & 3] + (k->wr[4] & 1));
    int stopp = k->wr[4] >> 2 & 3;           /* 1, 1,5 eller 2 */
    halva += stopp == 3 ? 4 : stopp == 2 ? 3 : 2;
    return halva * delning[k->wr[4] >> 6] * period / 2;
}

/* Efter en skrivning till DART:en, SIO:n eller CTC:n. En kanal i CTC:n
 * som står ändrar inget. */
static void klockor(Abc802 *m)
{
    long p0 = ctc_period(&m->ctc, 0), p1 = ctc_period(&m->ctc, 1),
         p2 = ctc_period(&m->ctc, 2);
    SioKanal *a = &m->dart.k[0], *v24 = &m->sio.k[0], *kas = &m->sio.k[1];
    if (p2) {
        a->tecken_tid = tecken_tid(a, 0, a->wr[5] >> 5, p2);
        m->mottagning_tid[ABC802_CH_A] = tecken_tid(a, 0, a->wr[3] >> 6, p2);
    }
    if (p0)
        v24->tecken_tid = tecken_tid(v24, sio_synkron(&m->sio, 0), v24->wr[5] >> 5, p0);
    if (p1)
        m->mottagning_tid[ABC802_CH_B] = tecken_tid(v24, sio_synkron(&m->sio, 0), v24->wr[3] >> 6, p1);
    if (p0)
        kas->tecken_tid = tecken_tid(kas, sio_synkron(&m->sio, 1), kas->wr[5] >> 5, p0);
}

/* ---------------------------------------------------------------- */
/* Portarna                                                          */

static uint8_t las_port(void *dator, uint16_t port)
{
    Abc802 *m = dator;
    uint8_t p = port & 0xFF;
    if (p < 0x03)                            /* INP 0-2; IN 7 är RST på bussen */
        return bussen_in(&m->bussen, p);
    if (p == 0x05) {                         /* 8205:an (15F) klockar 74LS390:an */
        m->summer ^= 1;
        m->summer_omslag++;
        if (m->summer_antal < ABC802_SUMMER_MAX)
            m->summer_tid[m->summer_antal++] = m->tid;
        return 0xFF;
    }
    switch (p & 0xF0) {
    case 0x20:
        return sio_las(&m->dart, p >> 1 & 1, p & 1, m->tid);
    case 0x30:
        if ((p & 9) == 1 && m->crtc.adress >= 14 && m->crtc.adress <= 17)
            return m->crtc.r[m->crtc.adress];
        return 0;
    case 0x40:
        return sio_las(&m->sio, p >> 1 & 1, p & 1, m->tid);
    case 0x60:
        return ctc_las(&m->ctc, p & 3);
    }
    return 0xFF;                             /* bara strober på bussen, eller inget */
}

static void skriv_port(void *dator, uint16_t port, uint8_t v)
{
    Abc802 *m = dator;
    uint8_t p = port & 0xFF;
    if (p < 0x06) {
        bussen_ut(&m->bussen, p, v);
        return;
    }
    switch (p & 0xF0) {
    case 0x20:
        sio_skriv(&m->dart, p >> 1 & 1, p & 1, v, m->tid);
        klockor(m);
        break;
    case 0x30:
        if (!(p & 8))                        /* A3 = 0: 6845 läser (s. 66) */
            break;
        if (!(p & 1))
            m->crtc.adress = v & 0x1F;
        else if (m->crtc.adress < 16)
            m->crtc.r[m->crtc.adress] = v;
        break;
    case 0x40:
        sio_skriv(&m->sio, p >> 1 & 1, p & 1, v, m->tid);
        klockor(m);
        break;
    case 0x60:
        ctc_skriv(&m->ctc, p & 3, v);
        klockor(m);
        break;
    }
}

/* Ett tecken ut på en kontakt. XOFF och XON stoppar och startar det
 * som kommer in där (till dess mottagaren slås på igen, eftersom
 * binära data som SAVE V24: också kan innehålla $13). */
static void ut(Abc802 *m, int kontakt, uint8_t tecken)
{
    if (tecken == 0x13)
        m->seriell_xoff[kontakt] = 1;
    else if (tecken == 0x11)
        m->seriell_xoff[kontakt] = 0;
    if (m->seriellt_ut)
        m->seriellt_ut(m->seriell_data, kontakt, tecken);
}

/* DART A sänder på CH A. Tangentbordet får ett tecken från DART B: kod
 * 7 är ett pip (servicemanualen s. 86). */
static void dart_sant(void *data, int kanal, uint8_t tecken)
{
    Abc802 *m = data;
    if (kanal == 0)
        ut(m, ABC802_CH_A, tecken);
    else if (tecken == 7)
        m->pip++;
}

/* SIO A sänder på CH B, SIO B till kassetten (när motorn går). */
static void sio_sant(void *data, int kanal, uint8_t tecken)
{
    Abc802 *m = data;
    if (kanal == 0)
        ut(m, ABC802_CH_B, tecken);
    else if (abc802_motor(m))
        kassett802_skriv(&m->kassett, tecken);
}

/* ---------------------------------------------------------------- */
/* CH A och CH B in, och kassetten                                   */

int abc802_seriellt_in(Abc802 *m, int kontakt, uint8_t tecken)
{
    int nasta = (m->seriell_sist[kontakt] + 1) % ABC802_SERIELL_KO;
    if (nasta == m->seriell_forst[kontakt])
        return 0;
    m->seriell_ko[kontakt][m->seriell_sist[kontakt]] = tecken;
    m->seriell_sist[kontakt] = nasta;
    return 1;
}

int abc802_seriellt_vantar(const Abc802 *m, int kontakt)
{
    return (m->seriell_sist[kontakt] - m->seriell_forst[kontakt] + ABC802_SERIELL_KO)
        % ABC802_SERIELL_KO;
}

/* Nästa tecken i kön går in när mottagaren är på, ett tecken i taget i
 * mottagningens takt; annars väntar det. Det första är framme ett
 * tecken efter att mottagaren slogs på (V24OPN ställer in SIO A två
 * gånger och återställer kanalen däremellan). */
static void seriellt_fram(Abc802 *m, int kontakt)
{
    Sio *s = kontakt == ABC802_CH_A ? &m->dart : &m->sio;
    if (!(s->k[0].wr[3] & 0x01)) {
        m->seriell_pa[kontakt] = 0;
        return;
    }
    if (!m->seriell_pa[kontakt]) {
        m->seriell_pa[kontakt] = 1;
        m->seriell_xoff[kontakt] = 0;
        m->seriell_nasta[kontakt] = m->tid + m->mottagning_tid[kontakt];
    }
    if (m->seriell_forst[kontakt] == m->seriell_sist[kontakt]
        || m->seriell_xoff[kontakt] || m->tid < m->seriell_nasta[kontakt])
        return;
    sio_ta_emot(s, 0, m->seriell_ko[kontakt][m->seriell_forst[kontakt]]);
    m->seriell_forst[kontakt] = (m->seriell_forst[kontakt] + 1) % ABC802_SERIELL_KO;
    m->seriell_nasta[kontakt] = m->tid + m->mottagning_tid[kontakt];
}

int abc802_motor(const Abc802 *m)
{
    return (m->sio.k[1].wr[5] & 0x80) != 0;
}

/* Bandet går ett tecken i taget medan motorn är på (kassett802.h). Vid
 * inspelning med sändaren på skriver sio_sant tecknen; annars spelas
 * $00 in. Uppspelat går tecknet till SIO B. */
static void kassett_fram(Abc802 *m)
{
    Kassett802 *k = &m->kassett;
    if (!abc802_motor(m)) {
        k->nasta = -1;
        return;
    }
    long tid = 8L * 16 * m->ctc.k[0].konstant;
    if (k->nasta < 0)
        k->nasta = m->tid + tid;
    while (m->tid >= k->nasta) {
        k->nasta += tid;
        uint8_t tecken;
        if (k->spelar_in) {
            if (!(m->sio.k[1].wr[5] & 0x08))
                kassett802_skriv(k, 0);
        } else if (kassett802_las(k, &tecken))
            sio_ta_emot(&m->sio, 1, tecken);
    }
}

/* ---------------------------------------------------------------- */
/* Tangentkön                                                        */

void abc802_tangent(Abc802 *m, uint8_t kod)
{
    if ((m->ko_sist + 1) % ABC802_TANGENTKO != m->ko_forst) {
        m->tangentko[m->ko_sist] = kod;
        m->ko_sist = (m->ko_sist + 1) % ABC802_TANGENTKO;
    }
}

void abc802_paus(Abc802 *m, long ms)
{
    if ((m->ko_sist + 1) % ABC802_TANGENTKO != m->ko_forst) {
        m->tangentko[m->ko_sist] = 256 + (int)ms;
        m->ko_sist = (m->ko_sist + 1) % ABC802_TANGENTKO;
    }
}

int abc802_ko_tom(const Abc802 *m)
{
    return m->ko_forst == m->ko_sist && m->ko_steg == 0 && m->tecken_framme < 0
        && m->tid >= m->ko_nasta;
}

/* Kön ett steg: tangenten ned (DCD, och koden sänds), upp, eller en
 * paus. Varje steg räknas från när det förra skulle sluta. */
static void tangentko_steg(Abc802 *m)
{
    switch (m->ko_steg) {
    case 1:                                  /* nere: släpp */
        sio_ingang(&m->dart, 1, SIO_DCD, 0);
        m->ko_steg = 2;
        m->ko_nasta += ABC802_MS(m->slapp_ms);
        return;
    case 2:
        m->ko_steg = 0;
        break;
    }
    if (m->ko_forst == m->ko_sist) {
        m->ko_nasta = m->tid;
        return;
    }
    int post = m->tangentko[m->ko_forst];
    m->ko_forst = (m->ko_forst + 1) % ABC802_TANGENTKO;
    if (m->ko_nasta < m->tid)
        m->ko_nasta = m->tid;
    if (post >= 256) {
        m->ko_nasta += ABC802_MS(post - 256);
        return;
    }
    sio_ingang(&m->dart, 1, SIO_DCD, 1);
    m->tecken = (uint8_t)post;
    m->tecken_framme = m->ko_nasta + TANGENT_TECKEN_TID;
    m->ko_steg = 1;
    m->ko_nasta += ABC802_MS(m->tryck_ms);
}

/* ---------------------------------------------------------------- */
/* Starten                                                           */

void abc802_starta(Abc802 *m)
{
    memset(m, 0, sizeof *m);
    memset(m->rom, 0xFF, sizeof m->rom);
    memset(m->bildminne, ' ', sizeof m->bildminne);
    kassett802_starta(&m->kassett);
    m->bygel_80 = 1;
    m->tryck_ms = 40;
    m->slapp_ms = 60;
    m->tecken_framme = -1;
    bussen_tom(&m->bussen);
}

int abc802_rom(Abc802 *m, const uint8_t *data, size_t storlek)
{
    if (storlek != sizeof m->rom)
        return 0;
    memcpy(m->rom, data, storlek);
    return 1;
}

int abc802_teckenrom(Abc802 *m, const uint8_t *data, size_t storlek)
{
    if (storlek != ABC802_TECKENROM && storlek != 2 * ABC802_TECKENROM)
        return 0;
    memcpy(m->teckenrom, data, ABC802_TECKENROM);
    return 1;
}

void abc802_aterstall(Abc802 *m)
{
    m->z80.dator = m;
    m->z80.las = las_minne;
    m->z80.skriv = skriv_minne;
    m->z80.in = las_port;
    m->z80.ut = skriv_port;
    z80_reset(&m->z80);
    m->hamtning_7800 = 0;
    m->summer_fore = m->summer;
    m->summer_fran = m->tid;
    m->summer_antal = 0;

    sio_starta(&m->dart, 1);
    sio_starta(&m->sio, 0);
    m->dart.data = m;
    m->dart.sant = dart_sant;
    m->dart.k[1].tecken_tid = TANGENT_TECKEN_TID;
    m->sio.data = m;
    m->sio.sant = sio_sant;
    sio_ingang(&m->dart, 0, SIO_RI, m->bygel_80);
    sio_ingang(&m->dart, 0, SIO_DCD, 1);     /* CH A ansluten */
    sio_ingang(&m->dart, 0, SIO_CTS, 1);
    sio_ingang(&m->sio, 0, SIO_DCD, 1);      /* CH B ansluten */
    sio_ingang(&m->sio, 0, SIO_CTS, 1);
    sio_ingang(&m->dart, 1, SIO_CTS, m->bygel_60hz);
    sio_ingang(&m->sio, 1, SIO_DCD, m->bygel_slackare);
    for (int k = 0; k < 2; k++)
        m->dart.k[k].extavbrott = m->sio.k[k].extavbrott = 0;

    ctc_starta(&m->ctc);
    for (int k = 0; k < 3; k++)
        m->ctc.k[k].period_klocka = 2;       /* 1,5 MHz på CLK/TRG */
    for (int k = 0; k < 2; k++) {
        m->seriell_xoff[k] = 0;
        m->seriell_nasta[k] = 0;
        m->seriell_pa[k] = 0;
        m->mottagning_tid[k] = 1;
    }
    m->kassett.nasta = -1;
}

int abc802_fyrtio(const Abc802 *m)
{
    return (m->dart.k[1].wr[5] & 0x02) != 0;
}

/* ---------------------------------------------------------------- */
/* Körningen                                                         */

/* Avbrottskedjan: CTC, SIO/2, DART. En krets som betjänar ett avbrott
 * stänger för sig själv på lägre nivåer och för alla efter den. Ger
 * T-cyklerna, 0 om inget avbrott togs. */
static int avbrott(Abc802 *m)
{
    if (!m->z80.iff1 || m->z80.eivant)
        return 0;
    uint8_t vektor;
    int b, n;
    if ((b = ctc_begar(&m->ctc)) >= 0 && ((n = ctc_betjanad(&m->ctc)) < 0 || b < n))
        vektor = ctc_kvittera(&m->ctc, b);
    else if (ctc_betjanad(&m->ctc) >= 0)
        return 0;
    else if ((b = sio_begar(&m->sio)) >= 0 && ((n = sio_betjanad(&m->sio)) < 0 || b < n))
        vektor = sio_kvittera(&m->sio, b);
    else if (sio_betjanad(&m->sio) >= 0)
        return 0;
    else if ((b = sio_begar(&m->dart)) >= 0 && ((n = sio_betjanad(&m->dart)) < 0 || b < n))
        vektor = sio_kvittera(&m->dart, b);
    else
        return 0;
    return z80_int(&m->z80, vektor);
}

/* RETI på bussen: den första kretsen i kedjan som betjänar ett avbrott
 * tar den. */
static void reti(Abc802 *m)
{
    if (ctc_betjanad(&m->ctc) >= 0)
        ctc_reti(&m->ctc);
    else if (sio_betjanad(&m->sio) >= 0)
        sio_reti(&m->sio);
    else
        sio_reti(&m->dart);
}

static void ga_fram(Abc802 *m, int t)
{
    m->tid += t;
    ctc_fram(&m->ctc, t);
    sio_fram(&m->dart, m->tid);
    sio_fram(&m->sio, m->tid);
    kassett_fram(m);
    seriellt_fram(m, ABC802_CH_A);
    seriellt_fram(m, ABC802_CH_B);
    if (m->tecken_framme >= 0 && m->tid >= m->tecken_framme) {
        sio_ta_emot(&m->dart, 1, m->tecken);
        m->tecken_framme = -1;
    }
    if (m->tid >= m->ko_nasta)
        tangentko_steg(m);
}

void abc802_kor(Abc802 *m, long long tcykler)
{
    long long slut = m->tid + tcykler;
    while (m->tid < slut) {
        uint16_t pc = m->z80.pc;
        /* Låskretsen 7D är öppen under hämtningen (M1). */
        m->hamtning_7800 = (pc & 0xF800) == 0x7800;
        uint8_t operation = las_minne(m, pc);
        int ar_reti = operation == 0xED && las_minne(m, (uint16_t)(pc + 1)) == 0x4D;
        int t = z80_steg(&m->z80);
        if (ar_reti)
            reti(m);
        m->instruktioner++;
        ga_fram(m, t);
        t = avbrott(m);
        if (t)
            ga_fram(m, t);
    }
}

int abc802_summer(Abc802 *m, int takt, float *ut, int max)
{
    long long fran = m->summer_fran, till = m->tid;
    int antal = m->summer_antal, niva = m->summer_fore, n = 0;
    m->summer_fran = till;
    m->summer_fore = m->summer;
    m->summer_antal = 0;
    if (antal == 0)
        return 0;
    n = (int)((till - fran) * takt / ABC802_KLOCKA);
    if (n > max)
        n = max;
    for (int k = 0, j = 0; k < n; k++) {
        long long t = fran + (long long)k * ABC802_KLOCKA / takt;
        while (j < antal && m->summer_tid[j] <= t) {
            niva ^= 1;
            j++;
        }
        ut[k] = niva ? 1.0f : -1.0f;
    }
    return n;
}
