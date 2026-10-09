/*
 * vard.h -- värdlagren runt kärnan, till boken ABC80 inifrån
 *
 * abc80.c läser flaggorna och filerna, kopplar in korten och startar
 * maskinen; sedan tar ett av värdlagren över: terminalen (interaktivt)
 * eller batchläget, med anropsläget och filenheten TST: som tillägg.
 */

#ifndef VARD_H
#define VARD_H

#include "../karna/ieckort.h"
#include "../karna/maskin.h"
#include "../karna/skivkort.h"
#include "../karna/skrivarkort.h"

#define VARD_LISTA 32

/* Allt som värden håller reda på utöver maskinen. */
typedef struct Vard {
    Maskin *maskin;
    long efter_ms;                           /* -t; -1 = terminalen */
    int basic_ii;                            /* ROM:en är 32 KB */
    const char *visa_minne[VARD_LISTA];      /* -d adr,n */
    int antal_visa_minne;

    /* Korten och skrivarna; NULL när de inte är inkopplade. */
    Skivkort *skivkort;
    const char *skivfil[SKIVKORT_ENHETER];   /* för -DS */
    int spara_skivor;                        /* -DS */
    const char *visa_sektorer[8];            /* -DX n[,m] */
    int antal_visa_sektorer;
    Skrivarkort *skrivarkort;
    V24Skrivare *v24_skrivare;
    Ieckort *ieckort;
    int kortets_bildminne;                   /* -CR: $4000-$43FF */

    /* Anropsläget (anrop.c). */
    long anrop_adress;                       /* -x; -1 = inget anrop */
    const char *register_text;               /* -R */
    const char *andringar[VARD_LISTA];       /* -p adr=hh,... */
    int antal_andringar;
    int visa_anrop;                          /* -c */
    int stacken_vaxer;                       /* -G n */
    int skarmen_ocksa;                       /* -V */
    uint16_t bevakade[8];                    /* -P adr */
    int antal_bevakade;
    int logga_port;                          /* -O pp; -1 = ingen */
    int logga_skrivningar;                   /* -w */
    int anrop_igang, anrop_klart;
    uint16_t anrop_sp;                       /* SP före anropet */
    long long anrop_instruktioner;           /* instruktionerna före anropet */
    uint8_t sedda_anrop[65536];

    /* Ljudet (-a): proven skrivs till en WAV-fil i batchläget. */
    const char *ljudfil;

    /* Kassetten (kassettfil.c): bandet som spelas upp och det som
       spelas in, som WAV-filer. */
    Kassett *kassett;
    const char *bandfil, *inspelningsfil;    /* -T, -U */

    /* Filenheten TST: (tstenhet.c). */
    int tst, tst_visa;                       /* -F, -FD */
} Vard;

/* Avsluta med ett felmeddelande: "abc80: " text tillagg. */
void vard_fel(const char *text, const char *tillagg);

/* Kör ABC80 i terminalen tills Ctrl-] trycks. */
void terminal_kor(Maskin *m);

/* Batchläget: kör tangenterna och V24 till slut och sedan efter_ms till
 * (eller tills anropet är klart), och skriv det som har begärts. */
void batch_kor(Vard *v);

/* Anropsläget och loggningen: krokarna i maskinen. */
void anrop_krokar(Vard *v);
void anrop_borja(Vard *v);                   /* efter starten, före tangenterna */
void anrop_skriv(const Vard *v);             /* registren efter anropet */

/* Filenheten TST: i BASIC II. */
void tst_koppla(Vard *v);                    /* efter starten */
void tst_anrop(Vard *v);                     /* när PC når drivrutinen */
int  tst_drivrutin(const Vard *v);           /* PC är vid drivrutinen */
void tst_skriv(void);                        /* filerna, efter skärmen */

/* Kassetten: en WAV-fil till flanker (i T-cykler från början, malloc)
 * och flankerna till en WAV-fil som räcker till slut och en halv
 * sekund till. */
long long *kassett_las_wav(const char *namn, long *antal);
void kassett_skriv_wav(const char *namn, const long long *flanker, long antal,
                       long long slut);

/* n bytes av minnet från adr ("adr,n", hex och decimalt), 16 per rad. */
void vard_skriv_minne(const Maskin *m, const char *omrade);

#endif
