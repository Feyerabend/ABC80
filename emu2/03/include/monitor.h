#pragma once
#include <stdint.h>
#include <stdbool.h>

// Enter monitor mode: freeze Z80, show amber display, print welcome.
void monitor_enter(void);

// Exit monitor mode: return to normal ABC80 operation.
void monitor_exit(void);

// Returns true while monitor is active.
bool monitor_is_active(void);

// Poll USB serial for monitor commands; call once per main-loop tick.
void monitor_serial_poll(void);

// Render the monitor UI into fb (DISPLAY_WIDTH × DISPLAY_HEIGHT RGB565).
void monitor_render(uint16_t *fb);
