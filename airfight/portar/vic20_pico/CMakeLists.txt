cmake_minimum_required(VERSION 3.17)

# Board must be set before pico_sdk_init().
# Use -DPICO_BOARD=pico2_w on the cmake command line, or set it here.
if(NOT DEFINED PICO_BOARD)
    set(PICO_BOARD pico2_w)
endif()

# Expects pico_sdk_import.cmake next to this file (copy from pico-sdk/external/).
include(pico_sdk_import.cmake)

project(vic20 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# ---------- fake6502 ----------------------------------------------------------
add_library(fake6502 STATIC fake6502/fake6502.c)
target_include_directories(fake6502 PUBLIC fake6502)

# ---------- libdvi (INTERFACE — configured per-app) ---------------------------
add_subdirectory(libdvi)

# ---------- main target -------------------------------------------------------
add_executable(vic20
    src/main.c
    src/memory.c
    src/vic_chip.c
    src/via.c
    src/vic_kbd.c
    src/dvi_output.c
)

target_include_directories(vic20 PRIVATE include)

# NTSC by default; pass -DVIC_PAL=1 to cmake for PAL build.
if(VIC_PAL)
    target_compile_definitions(vic20 PRIVATE VIC_PAL=1)
endif()

# Pass -DAUTORUN_AIRFIGHT=ON to embed airfight.prg and run it on startup,
# bypassing KERNAL/BASIC.  Default OFF = normal VIC-20 BASIC boot.
option(AUTORUN_AIRFIGHT "Embed and autorun airfight.prg on startup" OFF)
if(AUTORUN_AIRFIGHT)
    target_compile_definitions(vic20 PRIVATE AUTORUN_AIRFIGHT=1)
endif()

target_link_libraries(vic20
    fake6502
    libdvi
    pico_stdlib
    pico_multicore
    hardware_dma
    hardware_pio
    hardware_interp
    hardware_vreg
)

# USB for debug REPL; disable UART so both pins are free for DVI.
pico_enable_stdio_usb(vic20 1)
pico_enable_stdio_uart(vic20 0)

pico_add_extra_outputs(vic20)
