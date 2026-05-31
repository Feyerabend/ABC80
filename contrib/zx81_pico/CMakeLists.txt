# CMakeLists.txt — zx81_pico
# ZX81 emulator with DVI output for Raspberry Pi Pico 2 (RP2350)
#
# All source dependencies are bundled inside this directory:
#   src/          ZX81 emulator + Z80 core + DVI/color shared files
#   libdvi/       PicoDVI TMDS encoder library (Wren6991/PicoDVI)
#
# Prerequisites
# -------------
#   export PICO_SDK_PATH=~/.pico-sdk/sdk/2.2.0
#
# Build
# -----
#   mkdir build && cd build
#   cmake .. -DPICO_BOARD=pico2 -DPICO_COPY_TO_RAM=1
#   make -j$(nproc)
#
# Flash
# -----
#   ./flash.sh           — build + flash (uses picotool, no BOOTSEL needed)
#   ./flash.sh build     — build only
#   ./flash.sh flash     — flash only
#
# DVI wiring — Pico DVI Sock (castellated pads at Pico end)
# ----------------------------------------------------------
#   GPIO 12, 13 : TMDS lane 0 (blue + sync) D+/D−
#   GPIO 14, 15 : TMDS clock CK+/CK−
#   GPIO 16, 17 : TMDS lane 2 (red)   D+/D−
#   GPIO 18, 19 : TMDS lane 1 (green) D+/D−
#
# ZX81 keyboard wiring — 8×5 matrix
# -----------------------------------
#   Rows (output, driven HIGH one at a time): GPIO 0 – 7
#   Cols (input, pull-down; HIGH = pressed) : GPIO 8 – 11  (cols 0 – 3)
#                                              GPIO 20      (col 4)

cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2 CACHE STRING "Pico board type (pico2 or pico2w)")

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(zx81_pico C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# ------------------------------------------------------------------
# Re-generate the ROM header at configure time if zx81.rom changed.
# The rom binary is expected alongside CMakeLists.txt (project root).
# ------------------------------------------------------------------
set(ZX81_ROM_BIN  ${CMAKE_CURRENT_SOURCE_DIR}/roms/zx81.rom)
set(ZX81_ROM_HDR  ${CMAKE_CURRENT_SOURCE_DIR}/src/zx81_rom_data.h)

if(EXISTS ${ZX81_ROM_BIN})
    execute_process(
        COMMAND python3
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_rom_header.py
            ${ZX81_ROM_BIN}
            ${ZX81_ROM_HDR}
        RESULT_VARIABLE _rom_gen_rc
    )
    if(NOT _rom_gen_rc EQUAL 0)
        message(WARNING "Could not regenerate zx81_rom_data.h (pre-built copy will be used)")
    endif()
else()
    message(STATUS "zx81.rom not found alongside CMakeLists.txt — add it to enable Phase 1+")
endif()

# ------------------------------------------------------------------
# libdvi — bundled inside this project under libdvi/
# ------------------------------------------------------------------
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libdvi ${CMAKE_CURRENT_BINARY_DIR}/libdvi)

# ------------------------------------------------------------------
# zx81_pico executable
# ------------------------------------------------------------------
add_executable(zx81_pico
    src/main.c
    src/color.c
    src/dvi_out.c
    src/z80.c           # Z80 CPU emulator (Phase 1+)
    src/zx81.c          # ZX81 machine wrapper: ROM, memory map, I/O, NMI/INT (Phase 1+)
    src/zx81_screen.c   # Display file → RGB565 framebuffer renderer (Phase 3+)
    src/zx81_kbd.c      # USB-CDC keyboard input (Phase 4+)
    src/zx81_load.c     # .p file loader via USB-CDC serial (Phase 7)
)

target_include_directories(zx81_pico PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_options(zx81_pico PRIVATE
    -O2
    -Wall
    -Wextra
    -ffunction-sections
    -fdata-sections
)

target_link_libraries(zx81_pico PRIVATE
    pico_stdlib
    pico_multicore
    pico_util
    libdvi
    hardware_clocks
    hardware_vreg
    hardware_sync
    hardware_gpio
    hardware_irq
    m
)

pico_enable_stdio_usb (zx81_pico 1)
pico_enable_stdio_uart(zx81_pico 0)

pico_add_extra_outputs(zx81_pico)

message(STATUS "Board        : ${PICO_BOARD}")
message(STATUS "System clock : 252 MHz (TMDS bit rate for 640x480@60Hz)")
message(STATUS "DVI pins     : GPIO 12-19 (Pico DVI Sock)")
message(STATUS "KBD rows     : GPIO 0-7")
message(STATUS "KBD cols     : GPIO 8-11, 20")
message(STATUS "Display      : 256x192 (32x24 chars) centred in 320x240")
