#pragma once
#include <stdint.h>

// Called once at startup to zero RAM and copy ROMs into place.
void memory_init(void);

// Direct access to the 64 KB address space (for debugger/test use).
uint8_t *memory_raw(void);

// Read-only pointer to the 4 KB character ROM ($8000-$8FFF in CPU space).
const uint8_t *memory_char_rom(void);
