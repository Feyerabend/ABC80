/*
 * batch.c -- batchläget, till boken ABC80 inifrån
 *
 * Kör maskinen utan terminal: först starten (en sekund), sedan
 * tangenterna i kön, tecknen på V24 och till sist en given tid till.
 * Efter starten kopplas TST: in och anropet börjar, om de är begärda;
 * anropet kan sluta körningen tidigare.
 *
 * Sedan skrivs på standard ut, i den här ordningen och bara det som är
 * begärt: skärmen (rad för rad i UTF-8 utan blanksteg i slutet och med
 * markören som '_'), minnet (-d), filerna på TST: (-FD), sektorerna i
 * den första avbilden (-DX), V24-skrivarens tecken ("PR:"), labplattans
 * utgångar, skrivarkortets tecken ("PR60:"), IEC-bussen ("IEC:") och
 * skrivarkortets bildminne ("K:"). I anropsläget skrivs registren och
 * minnet i stället, och med -V skärmen efter dem.
 *
 * Går kassettens motor när tangenterna är slut körs maskinen tills den
 * stannar (högst 30 minuter) innan tiden i -t börjar.
 *
 * Med -a skrivs ljudet till en WAV-fil (mono, 16 bitar), från starten
 * till slutet; maskinen körs då i bilder om 20 ms och proven för varje
 * bild skrivs efter den.
 */

#include <stdio.h>
#include <stdlib.h>

#include "../karna/tecken.h"
#include "vard.h"

static void skriv_skarm(const uint8_t *bildminne, const char *forst)
{
    char rad[TECKEN_RADLANGD];
    for (int r = 0; r < TECKEN_RADER; r++) {
        tecken_rad_kortad(bildminne, r, rad);
        printf("%s%s\n", forst, rad);
    }
}

void vard_skriv_minne(const Maskin *m, const char *omrade)
{
    char *slut;
    int adress = (int)strtol(omrade, &slut, 16), antal = 16;
    if (*slut == ',')
        antal = (int)strtol(slut + 1, NULL, 0);
    for (int k = 0; k < antal; k++) {
        if (k % 16 == 0)
            printf("%s%04X:", k ? "\n" : "", (adress + k) & 0xFFFF);
        printf(" %02X", m->minne[(adress + k) & 0xFFFF]);
    }
    printf("\n");
}

/* Bytes i hex, 16 per rad efter forst. */
static void skriv_bytes(const char *forst, const uint8_t *data, int antal)
{
    for (int j = 0; j < antal; j += 16) {
        printf("%s", forst);
        for (int k = j; k < j + 16 && k < antal; k++)
            printf(" %02X", data[k]);
        printf("\n");
    }
}

/* -DX n[,m]: sektorerna n-m i den första avbilden; rader som bara
 * upprepar sektorns sista byte utelämnas. */
static void skriv_sektorer(const Vard *v)
{
    const Skivkort *s = v->skivkort;
    if (!s || !s->antal_enheter)
        return;
    const Skivenhet *e = &s->enhet[0];
    for (int k = 0; k < v->antal_visa_sektorer; k++) {
        char *slut;
        long forsta = strtol(v->visa_sektorer[k], &slut, 0), sista = forsta;
        if (*slut == ',')
            sista = strtol(slut + 1, NULL, 0);
        for (long sektor = forsta; sektor <= sista && sektor < e->sektorer; sektor++) {
            const uint8_t *p = e->data + sektor * 256;
            int n = 256;
            while (n > 16 && p[n - 1] == p[255])
                n--;
            for (int j = 0; j < n; j += 16) {
                printf("S%ld+%02X:", sektor, j);
                for (int q = j; q < j + 16; q++)
                    printf(" %02X", p[q]);
                printf("\n");
            }
        }
    }
}

static void spara_skivor(const Vard *v)
{
    const Skivkort *s = v->skivkort;
    for (int k = 0; s && k < s->antal_enheter; k++) {
        FILE *f = fopen(v->skivfil[k], "wb");
        if (!f)
            vard_fel("kan inte skriva ", v->skivfil[k]);
        fwrite(s->enhet[k].data, 256, (size_t)s->enhet[k].sektorer, f);
        fclose(f);
    }
}

/* ---------------------------------------------------------------- */
/* Ljudet till en WAV-fil                                            */

static FILE *wav;
static long long wav_prov;

static void skriv_16(uint16_t x)
{
    putc(x & 0xFF, wav);
    putc(x >> 8, wav);
}

static void skriv_32(uint32_t x)
{
    skriv_16((uint16_t)(x & 0xFFFF));
    skriv_16((uint16_t)(x >> 16));
}

/* Huvudet: RIFF, fmt (PCM, mono, 16 bitar) och data med antal prov. */
static void wav_huvud(long provtakt, long long antal)
{
    uint32_t data = (uint32_t)(antal * 2);
    fputs("RIFF", wav);
    skriv_32(36 + data);
    fputs("WAVEfmt ", wav);
    skriv_32(16);
    skriv_16(1);                             /* PCM */
    skriv_16(1);                             /* en kanal */
    skriv_32((uint32_t)provtakt);
    skriv_32((uint32_t)provtakt * 2);        /* bytes per sekund */
    skriv_16(2);                             /* bytes per prov */
    skriv_16(16);
    fputs("data", wav);
    skriv_32(data);
}

/* Proven fram till maskinens tid. */
static void wav_fyll(Maskin *m)
{
    int16_t prov[1024];
    long long antal = ljud_prov_till(m->ljud, m->tid) - m->ljud->prov;
    while (antal > 0) {
        int n = antal > 1024 ? 1024 : (int)antal;
        ljud_prov(m->ljud, prov, n);
        for (int k = 0; k < n; k++)
            skriv_16((uint16_t)prov[k]);
        wav_prov += n;
        antal -= n;
    }
}

/* Kör t T-cykler; med ljudet en bild i taget. Bilderna slutar där en
 * enda körning hade gått förbi, så instruktionerna blir desamma. */
static void kor(Maskin *m, long long t)
{
    if (!wav) {
        maskin_kor(m, t);
        return;
    }
    long long slut = m->tid + t;
    while (m->tid < slut && !m->stoppad) {
        long long bild = slut - m->tid;
        maskin_kor(m, bild < MASKIN_BILD ? bild : MASKIN_BILD);
        wav_fyll(m);
    }
}

void batch_kor(Vard *v)
{
    Maskin *m = v->maskin;
    if (v->ljudfil) {
        wav = fopen(v->ljudfil, "wb");
        if (!wav)
            vard_fel("kan inte skriva ", v->ljudfil);
        wav_huvud(m->ljud->provtakt, 0);
    }
    /* Starten: kön börjar med en paus på en sekund. Körningen går i
     * bilder om 20 ms som tangenterna nedan, så att tiderna blir desamma
     * med och utan TST: och anrop. */
    while (m->tid < MASKIN_MS(1000))
        kor(m, MASKIN_BILD);
    if (v->tst)
        tst_koppla(v);
    if (v->anrop_adress >= 0)
        anrop_borja(v);
    while (!tangentko_tom(m) && !m->stoppad)
        kor(m, MASKIN_BILD);
    v24_starta(m, m->tid + MASKIN_KLOCKA / 10);   /* 100 ms efter tangenterna */
    if (m->antal_v24)
        kor(m, m->v24_start - m->tid + v24_tid(m));
    /* Kassettens motor går (SAVE, LOAD): vänta tills den stannar. */
    long long tak = m->tid + MASKIN_MS(30L * 60 * 1000);
    while (m->kassett && m->kassett->motorrelaet && m->tid < tak && !m->stoppad)
        kor(m, MASKIN_BILD);
    kor(m, MASKIN_MS(v->efter_ms));
    if (wav) {
        fseek(wav, 0, SEEK_SET);
        wav_huvud(m->ljud->provtakt, wav_prov);
        fclose(wav);
    }

    if (v->anrop_adress >= 0) {
        anrop_skriv(v);
        if (v->skarmen_ocksa)
            skriv_skarm(m->minne + MASKIN_BILDMINNE, "");
        return;
    }
    skriv_skarm(m->minne + MASKIN_BILDMINNE, "");
    for (int k = 0; k < v->antal_visa_minne; k++)
        vard_skriv_minne(m, v->visa_minne[k]);
    if (v->tst_visa)
        tst_skriv();
    skriv_sektorer(v);
    if (v->v24_skrivare)
        skriv_bytes("PR:", v->v24_skrivare->utskrift, v->v24_skrivare->antal);
    if (m->antal_labb)
        printf("V24 ut: bit 3 = %d, bit 4 = %d\n", m->pio_b_skrivet >> 3 & 1, m->pio_b_skrivet >> 4 & 1);
    if (v->skrivarkort)
        skriv_bytes("PR60:", v->skrivarkort->utskrift, v->skrivarkort->antal);
    if (v->ieckort && v->ieckort->logglangd)
        printf("%s\n", v->ieckort->logg);
    if (v->kortets_bildminne)
        skriv_skarm(m->minne + 0x4000, "K:");
    if (v->spara_skivor)
        spara_skivor(v);
}
