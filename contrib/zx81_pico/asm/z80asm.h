// z80asm.h — embedded Z80 assembler API
//
// Assemble a source text buffer directly into Z80 memory (m[]).
// Define Z80ASM_EMBEDDED before including z80asm.c to activate this API.
//
// Usage:
//   int z80asm_assemble(src, src_len, m, 0x8000, mon_print, true);
//
// Returns: number of bytes assembled (>= 0) on success, negative on error.

#pragma once
#include <stdint.h>
#include <stdbool.h>

// Assemble `src_len` bytes of Z80 assembly source text (`src`) into `mem`
// starting at `origin`.  Output and errors are delivered through `emit_fn`
// (may be NULL to suppress all output).  If `listing` is true each assembled
// line is printed through `emit_fn` in the form:
//   ADDR  XX XX XX   MNEMONIC  operands
//
// Returns the number of bytes assembled on success (>= 0).
// Returns a negative value (-(error count)) on failure; mem[] is unchanged
// in the error regions but the output buffer is still written during pass 2.
int z80asm_assemble(const char *src, int src_len,
                    uint8_t *mem, uint16_t origin,
                    void (*emit_fn)(const char *s),
                    bool listing);
