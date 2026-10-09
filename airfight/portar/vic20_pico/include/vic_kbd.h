#pragma once

// Initialise the keyboard state (call once before the main loop).
void vic_kbd_init(void);

// Call once per frame.  Reads one character from USB-CDC (non-blocking),
// maps it to the VIC-20 8×8 key matrix via via_key_event(), and manages
// key-hold / inter-key-gap timing.
void vic_kbd_scan(void);
