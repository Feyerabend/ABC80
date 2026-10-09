/*
 * string.h -- det lilla av C:s bibliotek som kärnan behöver i
 * webbläsaren, till boken ABC80 inifrån
 *
 * WebAssembly-versionen byggs utan C-bibliotek (clang --target=wasm32
 * -ffreestanding); funktionerna finns i ../libc.c.
 */

#ifndef STRING_H
#define STRING_H

#include <stddef.h>

void  *memset(void *mal, int varde, size_t n);
void  *memcpy(void *mal, const void *kalla, size_t n);
void  *memmove(void *mal, const void *kalla, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
char  *strcpy(char *mal, const char *kalla);

#endif
