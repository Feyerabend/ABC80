#pragma once
#include <stdint.h>

/* Memory-read hook used by disasm.c.
 * Define the function body in the translation unit that includes disasm.c. */
extern uint8_t z80_read_mem(uint16_t addr);
