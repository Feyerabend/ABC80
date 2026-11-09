/*
 * natmapp.h -- centralen i LUXNET (../karna/natkort.h) ur en mapp
 */

#ifndef NATMAPP_H
#define NATMAPP_H

#include "../karna/natkort.h"

/* Töm centralen och läs in mappen som UFD:. 0 om mappen inte går att läsa. */
int natmapp_las(Natkort *n, const char *mapp);

/* Skriv tillbaka det som har ändrats och ta bort det som har tagits bort. */
int natmapp_spara(Natkort *n, const char *mapp);

#endif
