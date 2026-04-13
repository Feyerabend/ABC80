#pragma once

#include <stdint.h>
#include <stdbool.h>

// Initialise ABC80 machine: load ROM image, reset Z80.
void abc80_init(void);

// Execute one Z80 instruction.
void abc80_step(void);

// Poll USB serial for a keypress and inject it into the keyboard buffer.
// Call once per main-loop iteration (after abc80_step).
void abc80_keyboard_poll(void);

// Generate the 50 Hz keyboard strobe interrupt.
// Call from the main loop when the 20 ms hardware timer fires.
void abc80_strobe(void);

// Read one character from the ABC80 screen RAM.
//   row : 0-23, col : 0-39
//   bit 7 of the return value = cursor attribute (show as reverse video)
//   bits 6-0 = ABC80 character code (SIS 662241, 7-bit)
uint8_t abc80_screen_char(int row, int col);

// Write one byte directly to ABC80 screen RAM (row 0-23, col 0-39).
// Used by setdot/clrdot to update graphics cells without going through Z80.
void abc80_screen_write(int row, int col, uint8_t val);

// Monitor support: read Z80 registers and memory without running the CPU.
typedef struct {
    uint8_t  a, b, c, d, e, h, l, f;
    uint16_t pc, sp, ix, iy;
    uint8_t  im;
    bool     iff1, iff2;
} abc80_regs_t;

void    abc80_get_regs(abc80_regs_t *r);
uint8_t abc80_read_mem(uint16_t addr);
