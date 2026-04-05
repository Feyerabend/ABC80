#pragma once
#include <stdint.h>

// Return the byte length of the Z80 instruction at addr.
int z80_oplen(uint16_t addr);

// Disassemble the instruction at addr into out (NUL-terminated).
// Returns the number of bytes consumed (same as z80_oplen).
int z80_disasm(uint16_t addr, char *out, int outlen);
