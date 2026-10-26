# ABC80 i ett eget fönster med SDL3 (fonster.c), till boken ABC80 inifrån.
#   make         bygg abc80f (kräver SDL3: Homebrew sdl3, Debian/Ubuntu
#                libsdl3-dev, Fedora SDL3-devel)
#   make clean   ta bort det byggda
# Kärnan är densamma som i ../ (abc80 och webbsidan); bara värden är ny.

CC     ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra

SDL_CFLAGS ?= $(shell pkg-config --cflags sdl3 2>/dev/null || echo -I/opt/homebrew/include)
SDL_LIBS   ?= $(shell pkg-config --libs sdl3 2>/dev/null || echo -L/opt/homebrew/lib -lSDL3)

KARNA = ../karna/z80.c ../karna/maskin.c ../karna/tecken.c ../karna/bussen.c \
        ../karna/skivkort.c ../karna/ljud.c ../karna/kassett.c ../karna/p40.c \
        ../karna/bild.c ../karna/uni80.c ../karna/skrivarkort.c \
        ../karna/hr80.c ../karna/hr80v24.c
VARD  = ../vard/kassettfil.c ../vard/wav.c

abc80f: fonster.c $(KARNA) $(VARD) ../karna/*.h ../vard/*.h
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -o $@ fonster.c $(KARNA) $(VARD) $(SDL_LIBS) -lm

clean:
	rm -f abc80f

.PHONY: clean
