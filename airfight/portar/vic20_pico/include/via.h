#pragma once
#include <stdint.h>

void    via_reset(void);
uint8_t via1_read(uint16_t addr);
void    via1_write(uint16_t addr, uint8_t val);
uint8_t via2_read(uint16_t addr);
void    via2_write(uint16_t addr, uint8_t val);

// Called by keyboard handler to update the key matrix state.
// row/col are 0-based indices into the 8×8 matrix; pressed=true means key down.
void via_key_event(int row, int col, int pressed);

// Called once per frame from the main loop to simulate VIA1 Timer1 expiry.
// Sets IFR bit 6 so the KERNAL's IRQ handler takes the keyboard-scan path.
void via1_fire_timer1(void);
