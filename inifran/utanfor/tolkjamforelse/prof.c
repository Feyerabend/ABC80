/*
 * prof.c -- utanför boken: mäter ett BASIC-program i ABC80-BASIC:en
 * eller i BASIC II, med T-cykler och instruktioner per adress.
 *
 * Skriver in programmet rad för rad, sedan RUN. Räkningen börjar när
 * tangentkön är tom efter RUN:s CR och slutar när PC når adressen
 * klar (tolkens READY). Ut på stdout: T-cykler, instruktioner och
 * sekunder; med en fjärde parameter också en tabell per adress
 * (adress, T-cykler, instruktioner) för profil.py.
 *
 *   ./prof rom ram_kb klar(hex) prog.bas [profil.txt]
 *
 * Med KRETS=fil@adr i miljön sitter en krets till på adr (hex), som en
 * tilläggs-ROM; där ligger de nya rutinerna för ABC80 (se lappa.py).
 * Med MINNE=fil skrivs hela minnet (64 KB) till fil efter körningen.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "maskin.h"

static long long tcyk[65536], antal[65536];
static int raknar, redo, klar, slut;
static uint16_t forra_pc;
static long long forra_tid, start_tid, start_instr;

static int fore(void *d, Maskin *m)
{
    (void)d;
    /* räkningen börjar när CR trycks ned (kön är tom först efter släppet) */
    if (redo && !raknar && m->tangent_nere) {
        raknar = 1;
        forra_pc = m->z80.pc;
        forra_tid = start_tid = m->tid;
        start_instr = m->instruktioner;
    }
    if (raknar && m->z80.pc == klar) {
        slut = 1;
        return 1;
    }
    return 0;
}

static void efter(void *d, Maskin *m, uint8_t op, uint16_t pc, uint16_t sp)
{
    (void)d; (void)op; (void)sp;
    if (!raknar)
        return;
    /* tiden för förra instruktionen (och ett avbrott efter den) */
    tcyk[forra_pc] += m->tid - forra_tid;
    antal[pc]++;
    forra_pc = pc;
    forra_tid = m->tid;
}

static void vanta(Maskin *m)
{
    while (!tangentko_tom(m))
        maskin_kor(m, MASKIN_MS(20));
}

int main(int argc, char **argv)
{
    static Maskin m;
    static uint8_t rom[32768];
    if (argc < 5) {
        fprintf(stderr, "prof rom ram_kb klar prog.bas [profil.txt]\n");
        return 1;
    }
    FILE *f = fopen(argv[1], "rb");
    size_t n = f ? fread(rom, 1, sizeof rom, f) : 0;
    if (!n) {
        fprintf(stderr, "prof: kan inte läsa %s\n", argv[1]);
        return 1;
    }
    fclose(f);
    klar = (int)strtol(argv[3], NULL, 16);
    maskin_starta(&m, atoi(argv[2]));
    maskin_rom(&m, rom, n);
    const char *krets = getenv("KRETS");
    if (krets && *krets) {
        static uint8_t k[16384];
        char namn[512];
        snprintf(namn, sizeof namn, "%s", krets);
        char *at = strrchr(namn, '@');
        if (!at || !(f = fopen((*at = 0, namn), "rb"))) {
            fprintf(stderr, "prof: kan inte läsa kretsen %s\n", krets);
            return 1;
        }
        size_t kn = fread(k, 1, sizeof k, f);
        fclose(f);
        maskin_krets(&m, (uint16_t)strtol(at + 1, NULL, 16), k, kn);
    }
    maskin_aterstall(&m);
    m.krokar.fore_steg = fore;
    m.krokar.efter_steg = efter;
    maskin_kor(&m, MASKIN_MS(1000));
    if (!(f = fopen(argv[4], "r"))) {
        fprintf(stderr, "prof: kan inte läsa %s\n", argv[4]);
        return 1;
    }
    char rad[256];
    while (fgets(rad, sizeof rad, f)) {
        size_t k = strcspn(rad, "\r\n");
        rad[k] = 0;
        if (!k)
            continue;
        for (char *s = rad; *s; s++)
            tangentko_lagg(&m, (uint8_t)*s);
        tangentko_lagg(&m, 13);
        vanta(&m);
        maskin_kor(&m, MASKIN_MS(100));
    }
    fclose(f);
    for (const char *s = "RUN"; *s; s++)
        tangentko_lagg(&m, (uint8_t)*s);
    vanta(&m);
    maskin_kor(&m, MASKIN_MS(100));
    tangentko_lagg(&m, 13);
    redo = 1;
    /* högst 10 minuter i maskinen */
    for (int i = 0; i < 600 && !slut; i++)
        maskin_kor(&m, MASKIN_KLOCKA);
    long long t = m.tid - start_tid;
    printf("%lld %lld %.3f%s\n", t, m.instruktioner - start_instr,
           (double)t / MASKIN_KLOCKA, slut ? "" : " (ej klar)");
    if (argc > 5 && (f = fopen(argv[5], "w"))) {
        for (int a = 0; a < 65536; a++)
            if (antal[a] || tcyk[a])
                fprintf(f, "%04X %lld %lld\n", a, tcyk[a], antal[a]);
        fclose(f);
    }
    /* hela minnet efter körningen, för att se koden som kompilatorn skrev */
    const char *minne = getenv("MINNE");
    if (minne && *minne && (f = fopen(minne, "wb"))) {
        fwrite(m.minne, 1, 65536, f);
        fclose(f);
    }
    /* skärmen, så att man ser att det gick */
    if (getenv("SKARM"))
        for (int r = 0; r < 24; r++) {
            int k = 0;
            for (int c = 0; c < 40; c++)
                rad[k++] = m.minne[0x7C00 + (r % 8) * 128 + (r / 8) * 40 + c] & 0x7F;
            while (k && rad[k - 1] == ' ')
                k--;
            rad[k] = 0;
            if (k)
                fprintf(stderr, "%s\n", rad);
        }
    return 0;
}
