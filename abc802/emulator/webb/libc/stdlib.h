/*
 * stdlib.h -- minnet i webbläsaren, till boken ABC80 inifrån
 *
 * malloc och free i ../libc.c, ovanpå WebAssemblys linjära minne, som
 * växer när det behövs.
 */

#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>

void *malloc(size_t n);
void  free(void *p);

#endif
