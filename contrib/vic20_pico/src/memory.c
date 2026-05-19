#include <string.h>
#include <stdint.h>

#include "fake6502.h"
#include "memory.h"
#include "vic_chip.h"
#include "via.h"

// ---- Address space ----------------------------------------------------------
//
// A single 64 KB array covers all of RAM.  ROM regions are overlaid at read
// time so writes to them are silently ignored (matching real hardware).
//
// ROM arrays are embedded as const data from generated headers.
// Kernal is 901486-07 (complete, from VICE 3.10); includes full screen editor.

#include "rom_basic.h"    // rom_basic[8192]  — $C000–$DFFF
#include "rom_kernal.h"   // rom_kernal[8192] — $E000–$FFFF
#include "rom_char.h"     // rom_char[4096]   — $8000–$8FFF

// Optional PRG autorun — enabled by cmake -DAUTORUN_AIRFIGHT=ON.
// When enabled the PRG is embedded as a C array and loaded at its stated
// load address; the 6502 reset vector is redirected to that address so
// the program runs immediately, bypassing KERNAL/BASIC.
#ifdef AUTORUN_AIRFIGHT
#include "prg_airfight.h"
#define PRG_LOAD_ADDR  0x1000u
#endif

static uint8_t ram[65536];

void memory_init(void) {
    memset(ram, 0, sizeof(ram));
    memset(ram + 0x1E00, 0x20, 22 * 23);  // blank screen RAM

#ifdef AUTORUN_AIRFIGHT
    if (prg_airfight_len > 2)
        memcpy(ram + PRG_LOAD_ADDR, prg_airfight + 2, prg_airfight_len - 2);
#endif

    via_reset();
    vic_reset();
}

uint8_t          *memory_raw(void)      { return ram; }
const uint8_t   *memory_char_rom(void) { return rom_char; }

// ---- Memory bus callbacks required by fake6502 ------------------------------

uint8_t read6502(uint16_t addr) {
#ifdef AUTORUN_AIRFIGHT
    // Redirect the 6502 reset vector so the CPU jumps to our PRG, bypassing KERNAL.
    if (addr == 0xFFFC) return (uint8_t)(PRG_LOAD_ADDR & 0xFF);
    if (addr == 0xFFFD) return (uint8_t)(PRG_LOAD_ADDR >> 8);
#endif

    // Character ROM: $8000–$8FFF (only lower 4 KB populated)
    if (addr >= 0x8000 && addr <= 0x8FFF)
        return rom_char[addr - 0x8000];

    // VIC chip: $9000–$900F (mirrored every 16 bytes up to $90FF)
    if (addr >= 0x9000 && addr <= 0x90FF)
        return vic_read(addr);

    // VIA #1: $9110–$911F (mirrored every 16 bytes up to $911F)
    if (addr >= 0x9110 && addr <= 0x911F)
        return via1_read(addr);

    // VIA #2: $9120–$912F
    if (addr >= 0x9120 && addr <= 0x912F)
        return via2_read(addr);

    // BASIC ROM: $C000–$DFFF
    if (addr >= 0xC000 && addr <= 0xDFFF)
        return rom_basic[addr - 0xC000];

    // Kernal ROM: $E000–$FFFF
    if (addr >= 0xE000)
        return rom_kernal[addr - 0xE000];

    // Everything else: RAM (including expansion area and cartridge space,
    // which will just read as 0 until populated).
    return ram[addr];
}

void write6502(uint16_t addr, uint8_t val) {
    // VIC chip: $9000–$90FF
    if (addr >= 0x9000 && addr <= 0x90FF) { vic_write(addr, val); return; }

    // VIA #1: $9110–$911F
    if (addr >= 0x9110 && addr <= 0x911F) { via1_write(addr, val); return; }

    // VIA #2: $9120–$912F
    if (addr >= 0x9120 && addr <= 0x912F) { via2_write(addr, val); return; }

    // ROM ranges: silently ignore (real hardware ignores ROM writes too).
    if (addr >= 0x8000 && addr <= 0x8FFF) return;
    if (addr >= 0xC000) return;

    ram[addr] = val;
}
