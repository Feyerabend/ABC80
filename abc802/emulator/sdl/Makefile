# ABC 802 i ett eget fönster med SDL3 (fonster.c), till ABC 802 inifrån.
#   make         bygg abc802f (kräver SDL3: Homebrew sdl3, Debian/Ubuntu
#                libsdl3-dev, Fedora SDL3-devel)
#   make clean   ta bort det byggda
# Kärnan är densamma som i ../ (abc802); bara värden är ny.

CC     ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra

SDL_CFLAGS ?= $(shell pkg-config --cflags sdl3 2>/dev/null || echo -I/opt/homebrew/include)
SDL_LIBS   ?= $(shell pkg-config --libs sdl3 2>/dev/null || echo -L/opt/homebrew/lib -lSDL3)

KARNA = ../karna/z80.c ../karna/sio.c ../karna/ctc.c ../karna/abc802.c ../karna/skarm802.c \
        ../karna/bussen.c ../karna/skivkort.c ../karna/kassett802.c ../karna/skrivarkort.c

abc802f: fonster.c $(KARNA) ../karna/*.h
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -o $@ fonster.c $(KARNA) $(SDL_LIBS) -lm

clean:
	rm -f abc802f

.PHONY: clean
