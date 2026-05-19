#include <string.h>
#include "via.h"

// Minimal 6522 VIA stub.  Only the registers the Kernal actually polls at
// startup are handled; everything else reads back the last written value.
//
// VIA #1 ($9110): joystick / misc I/O (NOT the keyboard)
// VIA #2 ($9120): keyboard matrix — Port B (colm) drives columns, Port A (rows) reads rows

static uint8_t via1_regs[16];
static uint8_t via2_regs[16];

// Key matrix: key_matrix[col] is a bitmask of rows pressed in that column (active-low).
// KERNAL writes to VIA2 Port B ($9120) to select column(s), reads VIA2 Port A ($9121) for rows.
static uint8_t key_matrix[8];  // indexed by column, bits = rows

void via_reset(void) {
    // Zero everything, then pull input port lines high.
    // IFR (0x0D) and IER (0x0E) must start at 0 — a non-zero IFR on first IRQ
    // sends the KERNAL's handler down wrong paths and crashes the 6502.
    memset(via1_regs, 0x00, sizeof(via1_regs));
    memset(via2_regs, 0x00, sizeof(via2_regs));
    via1_regs[0x00] = 0xFF;   // Port B inputs pulled high
    via1_regs[0x01] = 0xFF;   // Port A inputs pulled high
    via2_regs[0x00] = 0xFF;   // Port B (column drive idle = all columns deselected)
    via2_regs[0x01] = 0xFF;   // Port A (row read idle = no keys)
    memset(key_matrix, 0xFF, sizeof(key_matrix));  // all keys up = all bits high
}

void via1_fire_timer1(void) {
    via1_regs[0x0D] |= 0x40;  // Set Timer1 interrupt flag in IFR
}

void via_key_event(int row, int col, int pressed) {
    if (row < 0 || row > 7 || col < 0 || col > 7) return;
    if (pressed)
        key_matrix[col] &= ~(1 << row);  // active-low: pull bit low = key pressed
    else
        key_matrix[col] |=  (1 << row);  // release
}

// ---------- VIA #1 -----------------------------------------------------------

uint8_t via1_read(uint16_t addr) {
    uint8_t reg = addr & 0x0F;
    if (reg == 0x04)            // Reading T1CL clears the Timer1 IFR flag (real 6522 behaviour)
        via1_regs[0x0D] &= ~0x40;
    return via1_regs[reg];
}

void via1_write(uint16_t addr, uint8_t val) {
    via1_regs[addr & 0x0F] = val;
}

// ---------- VIA #2 -----------------------------------------------------------

uint8_t via2_read(uint16_t addr) {
    uint8_t reg = addr & 0x0F;
    // Reg $01 = ORA (with CA2 handshake) — used by SCNKEY for full keyboard scan.
    // Reg $0F = ORA (no handshake)       — used by UDTIM each IRQ to read STOP key.
    // Both must perform the matrix scan so $91 (STOP flag) is updated correctly.
    if (reg == 0x01 || reg == 0x0F) {
        uint8_t col_drive = ~via2_regs[0x00];  // active-low → high bits = selected columns
        uint8_t result = 0xFF;
        for (int col = 0; col < 8; col++) {
            if (col_drive & (1 << col))
                result &= key_matrix[col];
        }
        return result;
    }
    return via2_regs[reg];
}

void via2_write(uint16_t addr, uint8_t val) {
    via2_regs[addr & 0x0F] = val;
}
