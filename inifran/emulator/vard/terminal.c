/*
 * terminal.c -- ABC80 i en POSIX-terminal, till boken ABC80 inifrån
 *
 * Skärmen ritas med escapesekvenser (ANSI) i terminalens alternativa
 * buffert, 40 tecken x 24 rader, och tangenterna läses utan eko och
 * utan radbuffert. Ctrl-C är BREAK; två gånger inom en halv sekund
 * avslutar (Ctrl-] också, men den är svår att nå på ett svenskt
 * tangentbord). Terminalen återställs också när programmet stoppas
 * med en signal (pkill, fönstret stängs). Piltangenterna
 * vänster och höger är ABC80:s ← och →, backsteg är ←. Maskinen körs en
 * bild (20 ms) i taget mot klockan; medan det finns kvar i tangentkön
 * (från -k och -f, med pauser) går den så fort datorn kan.
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <signal.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "../karna/tecken.h"
#include "vard.h"

static struct termios forra_lage;

static void aterstall_terminalen(void)
{
    tcsetattr(0, TCSANOW, &forra_lage);
    printf("\033[?25h\033[?1049l");          /* markören och vanliga bufferten */
    fflush(stdout);
}

static void vid_signal(int signal)
{
    aterstall_terminalen();
    _exit(128 + signal);
}

static void stall_in_terminalen(void)
{
    if (tcgetattr(0, &forra_lage) < 0)
        vard_fel("standard in är ingen terminal (använd -t för batchläget)", NULL);
    struct termios t = forra_lage;
    t.c_lflag &= ~(tcflag_t)(ICANON | ECHO | ISIG | IEXTEN);
    t.c_iflag &= ~(tcflag_t)(IXON | ICRNL | INLCR);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &t);
    atexit(aterstall_terminalen);
    signal(SIGTERM, vid_signal);
    signal(SIGHUP, vid_signal);
    signal(SIGINT, vid_signal);
    printf("\033[?1049h\033[?25l\033[H\033[2J");
}

static double klockan(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* Det som har kommit från tangentbordet, till kön; 0 vid Ctrl-] eller
 * två Ctrl-C inom en halv sekund.
 * Escapesekvenserna för piltangenterna läses i ett stycke; en ensam
 * ESC är ESC. */
static int las_tangenter(Maskin *m)
{
    uint8_t b[64];
    ssize_t n = read(0, b, sizeof b);
    for (ssize_t k = 0; k < n; k++) {
        uint8_t c = b[k];
        if (c == 0x1D)                       /* Ctrl-] */
            return 0;
        if (c == 0x03) {                     /* Ctrl-C: BREAK, två avslutar */
            static double forra_ctrl_c = -1.0;
            double nu = klockan();
            if (forra_ctrl_c >= 0.0 && nu - forra_ctrl_c < 0.5)
                return 0;
            forra_ctrl_c = nu;
        }
        if (c == 0x1B && k + 2 < n && b[k + 1] == '[') {
            if (b[k + 2] == 'D')
                tangentko_lagg(m, 0x08);     /* ← */
            else if (b[k + 2] == 'C')
                tangentko_lagg(m, 0x09);     /* → */
            k += 2;
            continue;
        }
        if (c == 0x7F)                       /* backsteg */
            c = 0x08;
        if (c == 0xC3 && k + 1 < n) {
            c = tecken_svensk(b[++k]);
            if (!c)
                continue;
        } else if (c >= 0x80)
            continue;
        tangentko_lagg(m, c);
    }
    return 1;
}

/* Rita skärmen om bildminnet har ändrats sedan förra gången. */
static void rita(const Maskin *m)
{
    static uint8_t visad[0x400];
    static int ritad;
    const uint8_t *bild = m->minne + MASKIN_BILDMINNE;
    if (ritad && !memcmp(visad, bild, sizeof visad))
        return;
    memcpy(visad, bild, sizeof visad);
    ritad = 1;
    char rad[TECKEN_RADLANGD + 40 * 10];
    printf("\033[H");
    for (int r = 0; r < TECKEN_RADER; r++) {
        tecken_rad(bild, r, rad, "\033[7m", "\033[27m");
        printf("%s\033[K\r\n", rad);
    }
    printf("\033[2m-- ABC80 -- Ctrl-C två gånger avslutar --\033[0m\033[K");
    fflush(stdout);
}

/* Vänta högst sekunder, eller tills en tangent kommer. */
static void vanta(double sekunder)
{
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    struct timeval tv = { 0, (long)(sekunder * 1e6) };
    select(1, &fds, NULL, NULL, &tv);
}

void terminal_kor(Maskin *m)
{
    stall_in_terminalen();
    double nasta_bild = klockan();
    for (;;) {
        if (!las_tangenter(m))
            return;
        maskin_kor(m, MASKIN_BILD);
        if (!tangentko_tom(m)) {             /* så fort det går, 20 ms i taget */
            double borjan = klockan();
            while (!tangentko_tom(m) && klockan() - borjan < 0.02)
                maskin_kor(m, MASKIN_BILD);
            rita(m);
            nasta_bild = klockan();
            continue;
        }
        rita(m);
        nasta_bild += 0.02;
        double kvar = nasta_bild - klockan();
        if (kvar > 0)
            vanta(kvar);
        else if (kvar < -0.5)
            nasta_bild = klockan();          /* för långsam dator: hoppa */
    }
}
