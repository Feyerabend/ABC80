// most simple emulator of/for ABC80

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ncurses.h>

#include "abcprom.h"
#include "z80.h"

#define TRUE 1
#define FALSE 0

char current_key;
int done = FALSE;

uint16_t rowstart[] = {
    0x7C00, // 00
    0x7C80, // 01
    0x7D00, // 02 
    0x7D80, // 03 
    0x7E00, // 04 
    0x7E80, // 05 
    0x7F00, // 06
    0x7F80, // 07

    0x7C28, // 08
    0x7CA8, // 09
    0x7D28, // 10
    0x7DA8, // 11
    0x7E28, // 12
    0x7EA8, // 13 
    0x7F28, // 14
    0x7FA8, // 15

    0x7C50, // 16
    0x7CD0, // 17
    0x7D50, // 18
    0x7DD0, // 19
    0x7E50, // 20
    0x7ED0, // 21
    0x7F50, // 22
    0x7FD0  // 23
};


u8 kbhit() {
    int character = getch();

    if (character != ERR) {
        ungetch(character);
        return 1;
    }
    return 0;
}

static uint8_t keyboard_check() {

    // no key hit
    if (!kbhit()) {

        // clear bit 7 to assume no key currently pressed
        current_key &= ~(1 << 7);
        return current_key;
    }

    // else
    char input = getch(); // ncurses

    switch (input) {
        case 27 : done = TRUE; break; // ESC
        case 127: input = '\b'; break; // DEL (backspace)
        // ...
    }

    // get ASCII code
    current_key = input;

    // if key was pressed set the bit 7 high
    current_key |= (1 << 7);

    // generate the INT
    gen_int(0x0034); // 52 -- version dependent?

    return current_key;
}

void beep() {
    putchar(7); // print chr$(7)
    fflush(stdout);
}

u8 port_in(u8 port) {
    if (port == 56) {
        current_key = keyboard_check() | (1 << 7);
        R current_key;
    }
    R 0;
}

void port_out(u8 port, u8 val) {
    if (port == 6 && val == 131)
        beep();
}

// screen represented through ncurses
void screen() {

    for (uint16_t jloop = 0; jloop < 24; jloop++) {
        for (uint16_t iloop = 0; iloop < 40; iloop++) {
            uint16_t cursor = m[rowstart[jloop] + iloop];
            if (cursor & 0x80) {
                attron(A_BLINK);
                mvprintw(jloop, iloop, "_", cursor);
                attroff(A_BLINK);
         
            } else {
                mvprintw(jloop, iloop, "%c", cursor & 0x7f);
         
            }
        }
    }

    refresh();
}

static void run() {
    init();
    memcpy(m + 0x0000, abcprom, sizeof(abcprom));

    // ncurses
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, true);
    nonl();

    pc = 0x0000;
    do {
        step();
        screen();
    }
    while (pc && done == FALSE);
    putchar('\n');
}

int main(void) {
    run();
}
