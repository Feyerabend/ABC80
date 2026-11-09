/*
 * natmapp.c -- centralen i LUXNET ur en mapp och tillbaka (natmapp.h)
 *
 * Mappen är kontots UFD:. Filerna med namn som går i DOS:en (högst åtta
 * tecken, en punkt och högst tre) läses in, och undermapparna blir
 * kataloger (CD), högst fyra nivåer ned. Annat hoppas över.
 */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "natmapp.h"

static int las_katalog(Natkort *n, const char *mapp, const char *katalog, int niva)
{
    char sokvag[1024];
    snprintf(sokvag, sizeof sokvag, "%s%s%s", mapp, *katalog ? "/" : "", katalog);
    DIR *d = opendir(sokvag);
    if (!d)
        return 0;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.')
            continue;
        char fil[1024], under[NATKORT_KATALOG + 16];
        snprintf(fil, sizeof fil, "%s/%s", sokvag, e->d_name);
        struct stat st;
        if (stat(fil, &st))
            continue;
        if (S_ISDIR(st.st_mode)) {
            if (niva == 4 || strlen(e->d_name) > 8
                || strlen(katalog) + strlen(e->d_name) + 2 > NATKORT_KATALOG)
                continue;
            snprintf(under, sizeof under, "%s%s", katalog, *katalog ? "/" : "");
            for (char *s = e->d_name, *u = under + strlen(under); ; s++, u++)
                if (!(*u = (char)(*s >= 'a' && *s <= 'z' ? *s - 32 : *s)))
                    break;
            if (!natkort_katalog(n, under))
                continue;
            las_katalog(n, mapp, under, niva + 1);
            continue;
        }
        if (!S_ISREG(st.st_mode))
            continue;
        FILE *f = fopen(fil, "rb");
        if (!f)
            continue;
        uint8_t *data = malloc((size_t)st.st_size + 1);
        long langd = data ? (long)fread(data, 1, (size_t)st.st_size, f) : 0;
        fclose(f);
        if (data && !natkort_fil(n, katalog, e->d_name, data, langd))
            fprintf(stderr, "abc802: %s går inte in i centralen\n", fil);
        free(data);
    }
    closedir(d);
    return 1;
}

int natmapp_las(Natkort *n, const char *mapp)
{
    natkort_starta(n);
    return las_katalog(n, mapp, "", 0);
}

int natmapp_spara(Natkort *n, const char *mapp)
{
    int ok = 1;
    /* Först det som är borta, sedan det som är ändrat: NAME A AS B och
     * B AS A lämnar en borttagen A efter den nya. */
    for (int varv = 0; varv < 2; varv++)
    for (int k = 0; k < n->antal; k++) {
        NatFil *f = &n->filer[k];
        if (f->ar_katalog || !f->andrad || f->borta != !varv)
            continue;
        char namn[13], fil[1024];
        natkort_filnamn(f, namn);
        snprintf(fil, sizeof fil, "%s/%s%s%s", mapp, f->katalog,
                 *f->katalog ? "/" : "", namn);
        if (f->borta) {
            remove(fil);
            f->andrad = 0;
            continue;
        }
        FILE *ut = fopen(fil, "wb");
        if (!ut || fwrite(f->data, 1, (size_t)f->langd, ut) != (size_t)f->langd) {
            fprintf(stderr, "abc802: kan inte skriva %s\n", fil);
            ok = 0;
        }
        if (ut)
            fclose(ut);
        f->andrad = 0;
    }
    return ok;
}
