/*
 * z80_api.h  External interface to the Z80 CPU core (z80.c / z80.h)
 *
 * z80.h defines CPU state as plain globals so it must be included in exactly
 * one translation unit (z80.c).  This header provides extern re-declarations
 * for the subset of Z80 state and API needed by platform code (zx80.c).
 *
 * Do NOT include z80.h here or in any file that also includes this header —
 * that would produce duplicate-definition linker errors.
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>

/* 64 KB flat address space (defined in z80.c via z80.h) */
extern uint8_t m[];

/* Z80 registers needed by platform I/O handlers. */
extern uint8_t  a, b, c, h, l;
extern uint8_t  r;
extern bool     iff1;
extern uint8_t  im;
extern uint16_t pc, sp;
extern bool     halted;

/* T-states consumed by the last step() call. */
extern int      z80_ts;

/* Z80 CPU API */
void     init(void);
void     step(void);
void     gen_nmi(void);
void     gen_int(uint8_t data);
void     z80_warm_start(uint16_t new_pc, uint16_t new_sp, uint16_t new_iy, uint8_t new_i);
