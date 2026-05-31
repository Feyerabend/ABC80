/*
 * zx81.c  ZX81 machine wrapper — ROM, memory map, I/O, NMI/INT generation
 */

#include "zx81.h"
#include "z80_api.h"
#include "zx81_kbd.h"
#include "zx81_rom_data.h"
#include <string.h>
#include <stdbool.h>

/* ---- Memory map ---------------------------------------------------------- */

/* ROM occupies 0x0000–0x1FFF and is mirrored at 0x2000–0x3FFF.
 * The real ZX81 ULA intercepts reads from 0x2000–0x3FFF during display
 * generation and substitutes character bitmap bytes keyed by the I register.
 * We skip that for now and mirror the ROM, which is sufficient for BASIC. */
#define ROM_SIZE     0x2000   /* 8 KB */
#define ROM_END      0x3FFF
#define RAM_START    0x4000
#define RAM_END      0xBFFF
#define UNMAP_START  0xC000

/* ---- Interrupt state ----------------------------------------------------- */

static int  hsync_counter;   /* T-states until next NMI/hsync */
static int  row_counter;     /* 3-bit character row (0–7), incremented at each hsync */
static bool nmi_enabled;     /* NMI flip-flop: OUT(0xFE) sets, OUT(0xFD) clears */

/* ---- Port I/O ------------------------------------------------------------ */

/* port_in — called by the Z80 core for every IN instruction.
 *   hi   = high byte of the 16-bit I/O address (= keyboard row selector on ZX81)
 *   port = low byte (0xFE = keyboard/ULA; other ports ignored) */
uint8_t port_in(uint8_t hi, uint8_t port)
{
    if ((port & 1) == 0)
        return zx81_kbd_read_port(hi);
    return 0xFF;
}

/* port_out — ZX81 ULA NMI flip-flop control:
 *   OUT (0xFE), A → set flip-flop   → enables NMI generation (SLOW mode)
 *   OUT (0xFD), A → clear flip-flop → disables NMI generation (FAST mode)
 * At reset the flip-flop is cleared (0x0000: OUT (0xFD),A), so NMIs don't
 * interrupt ROM init. */
void port_out(uint8_t port, uint8_t val)
{
    (void)val;
    if (port == 0xFE) nmi_enabled = true;   /* even port → NMI enable (SLOW mode) */
    if (port == 0xFD) nmi_enabled = false;  /* odd port  → NMI disable (FAST mode) */
}

/* ---- Initialisation ------------------------------------------------------ */

void zx81_init(void)
{
    /* Reset Z80 CPU state. */
    init();

    /* Load ROM into 0x0000–0x1FFF and mirror to 0x2000–0x3FFF. */
    memcpy(m + 0x0000, zx81_rom, ROM_SIZE);
    memcpy(m + 0x2000, zx81_rom, ROM_SIZE);

    /* Clear RAM (0x4000–0xBFFF). */
    memset(m + RAM_START, 0x00, RAM_END - RAM_START + 1);

    /* Unmapped space returns 0xFF (open bus).
     * 0xFF decodes as RST 38h on Z80; on the real ZX81 this triggers the
     * display handler when executed from 0x8000+ — fine to replicate. */
    memset(m + UNMAP_START, 0xFF, 0x10000 - UNMAP_START);

    hsync_counter = ZX81_TS_PER_NMI;
    row_counter   = 0;
    nmi_enabled   = false;
    zx81_kbd_init();
}

bool zx81_slow_mode(void) { return nmi_enabled; }

/* ---- Post-.p-load launch ------------------------------------------------- */

void zx81_p_load_launch(void)
{
    /* Initial system-variable bytes for a 16K ZX81 (RAMTOP = 0x7FFF).
     * Matches the JS/TS reference emulator:
     *   u = [n, r, lo(RAMTOP-3), hi(RAMTOP-3), lo(RAMTOP+1), hi(RAMTOP+1), 0, 0, 0]
     * where n=0xFF, r=0x80, RAMTOP-3=0x7FFC, RAMTOP+1=0x8000.
     * The .p file overwrites 0x4009+ (including D_FILE at 0x400C) with correct
     * saved values, so only these first 9 bytes need to be seeded explicitly. */
    static const uint8_t sv[9] = {
        0xFF, 0x80, 0xFC, 0x7F, 0x00, 0x80, 0x00, 0x00, 0x00
    };
    memcpy(m + 0x4000, sv, sizeof(sv));

    /* Minimal display-file header at 0x7FFC (D_FILE points here initially).
     * ROM at 0x0207 will quickly update D_FILE to the real display file
     * found inside the loaded .p data. */
    m[0x7FFC] = 0x76;   /* HALT (row terminator) */
    m[0x7FFD] = 0x06;
    m[0x7FFE] = 0x00;
    m[0x7FFF] = 0x3E;

    nmi_enabled   = false;
    hsync_counter = ZX81_TS_PER_NMI;

    /* IY = system-vars base (0x4000), I = character-set page (0x1E),
     * SP = RAMTOP-3 (clean stack), PC = ROM post-load entry (0x0207). */
    z80_warm_start(0x0207, 0x7FFC, 0x4000, 0x1E);
}

/* ---- Direct game launch -------------------------------------------------- */

void zx81_game_launch(uint16_t entry)
{
    nmi_enabled   = false;
    hsync_counter = ZX81_TS_PER_NMI;
    z80_warm_start(entry, 0x7FFC, 0x4000, 0x1E);
}

/* ---- Frame boundary ------------------------------------------------------ */

void zx81_new_frame(void)
{
    /* hsync_counter carries its remainder across frame boundaries so NMI
     * intervals don't drift.  row_counter is also persistent. */
}

/* ---- Main CPU loop with NMI/INT ----------------------------------------- */

/* T-state-accurate NMI/INT generation, matching the TS/JS reference:
 *   - hsync_counter counts down by actual T-states per instruction
 *   - NMI fires each time it hits zero (when nmi_enabled = SLOW mode)
 *   - INT fires when R register bit 6 = 0 (ZX81 hardware end-of-frame signal)
 *   - Overshoot is absorbed naturally: counter stays negative until += 207 */
void zx81_run_ts(int ts_budget)
{
    int ts_run = 0;

    while (ts_run < ts_budget) {
        step();
        ts_run += z80_ts;

        /* R bit 6 = 0: ZX81 end-of-display INT signal.
         * gen_int() sets int_pending; step() only acts on it when IFF1=1. */
        if ((r & 0x40) == 0)
            gen_int(0xFF);

        hsync_counter -= z80_ts;

        if (hsync_counter <= 0) {
            if (nmi_enabled) {
                gen_nmi();
                row_counter = (row_counter + 1) & 7;
            }
            /* Adding 207 absorbs the overshoot (hsync_counter was negative),
             * preserving phase accuracy across NMI intervals. */
            hsync_counter += ZX81_TS_PER_NMI;
        }
    }
}
