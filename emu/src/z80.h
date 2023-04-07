#pragma once

// ---------------------------------------
// ABBREVIATIONS

typedef int8_t i8;
typedef uint8_t u8;
typedef uint16_t u16;
typedef _Bool bv;
#define SI static inline
#define R return

// Defining a single-expression function body.
// Equivalent declarations:
// u8 f(u8 a)_(a * 2)
// u8 f(u8 a) { return a * 2; }
#define _(a...) {return({a;});}

// ---------------------------------------
// PROCESSOR STATE.

// The z80 memory.
//u8 * m = NULL;
#define MEMORY_SIZE     (1 << 16)
uint8_t m[MEMORY_SIZE];

// ---------------------------------------
// API

// Port I/O functions supplied by the user.
extern u8 port_in(u8 port);
extern void port_out(u8 port, u8 val);

// Exposed API.
void init();
void step();
void gen_nmi();
void gen_int(u8 data);

// Instruction counter, stack pointer, index registers.
u16 pc, sp, ix, iy;

// "WZ" register.
// https://www.grimware.org/lib/exe/fetch.php/documentations/devices/z80/z80.memptr.eng.txt
u16 wz;

// Main and exchange registers.
u8 a, b, c, d, e, h, l;
u8 exa, exb, exc, exd, exe, exh, exl, exf;

// Interrupt vectors and the refresh register.
// Note that the refresh register is useless but since
// it can be loaded to a, we must implement it properly.
// Some programs use it as a semi-random entropy source.
u8 i, r;

// Flags: sign, zero, halfcarry, parity, negative, carry.
// yf and xf are undocumented flags. In this emulator,
// they are hardwired to 3rd and 5th bits of the result.
// http://www.z80.info/z80sflag.htm
bv sf, zf, yf, hf, xf, pf, nf, cf;

// Interrupt flip-flops. `iff_set' is set when `EI' is executed.
// https://eduinf.waw.pl/inf/retro/004_z80_inst/0005.php
u8 iff_set;
bv iff1, iff2;

// The interrupt mode. Set by `IM', is either 0, 1 or 2.
// Mode 2 is rarely used, but it needs to be implemented.
u8 im;
u8 int_vec;
bv int_pending, nmi_pending;

bv halted;

