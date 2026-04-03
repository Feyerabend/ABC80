#pragma once

// ---------------------------------------------------------------------------
// SD: device for ABC80 on Raspberry Pi Pico 2W
//
// Provides a fake "SD:" device injected into the ABC80 enhetslistan
// (device list).  The device intercepts Z80 execution at known trap
// addresses and delegates file I/O to an SD-Pico via UART1.
//
// External interface — called from abc80.c:
//
//   sd_device_init()
//       Call once from abc80_init(), after the ROM image is loaded.
//       Patches emulated RAM to insert the SD: entry into enhetslistan
//       and installs HALT guard bytes at each trap address.
//
//   sd_device_dispatch(pc)
//       Call from abc80_step() after every step().
//       Returns true if pc matched a trap address and was handled.
//       When it returns true the caller must NOT execute the guard HALT.
// ---------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>

void sd_device_init(void);
bool sd_device_dispatch(uint16_t trap_pc);
