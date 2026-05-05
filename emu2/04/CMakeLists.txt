cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == Pico VS Code extension paths ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ==================================

set(PICO_BOARD pico2 CACHE STRING "Board type")

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(abc_pico C CXX ASM)

pico_sdk_init()

set(FATFS_SRC ${PICO_SDK_PATH}/lib/tinyusb/lib/fatfs/source)

add_executable(abc_pico
    src/main.c
    src/monitor.c
    src/disasm.c
    src/abc80.c
    src/sn76477.c
    src/sd_device.c
    src/z80.c
    src/display.c
    src/z80asm.c
    src/diskio.c
    src/sd_fat.c
    ${FATFS_SRC}/ff.c
    ${FATFS_SRC}/ffunicode.c
)

# ---- Pre-assemble AIRFIGHT on the host ----
# Build a native z80asm (no cross-compile flags, no Z80ASM_EMBEDDED).
add_custom_command(
    OUTPUT  ${CMAKE_BINARY_DIR}/z80asm_host
    COMMAND cc -O2 -o ${CMAKE_BINARY_DIR}/z80asm_host
                ${CMAKE_SOURCE_DIR}/src/z80asm.c
    DEPENDS ${CMAKE_SOURCE_DIR}/src/z80asm.c
    COMMENT "Building host z80asm"
)
# Assemble airfight.asm -> airfight.bin
add_custom_command(
    OUTPUT  ${CMAKE_BINARY_DIR}/airfight.bin
    COMMAND ${CMAKE_BINARY_DIR}/z80asm_host
                -o ${CMAKE_BINARY_DIR}/airfight.bin
                -g 0x8000
                ${CMAKE_SOURCE_DIR}/src/airfight.asm
    DEPENDS ${CMAKE_BINARY_DIR}/z80asm_host
            ${CMAKE_SOURCE_DIR}/src/airfight.asm
    COMMENT "Assembling airfight.asm"
)
# airfight.bin -> airfight_bin.h  (C array embedded in firmware)
add_custom_command(
    OUTPUT  ${CMAKE_BINARY_DIR}/airfight_bin.h
    COMMAND xxd -i -n airfight_bin
                ${CMAKE_BINARY_DIR}/airfight.bin
                > ${CMAKE_BINARY_DIR}/airfight_bin.h
    DEPENDS ${CMAKE_BINARY_DIR}/airfight.bin
    COMMENT "Generating airfight_bin.h"
)
add_custom_target(gen_airfight DEPENDS ${CMAKE_BINARY_DIR}/airfight_bin.h)
add_dependencies(abc_pico gen_airfight)

target_include_directories(abc_pico PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_BINARY_DIR}
    ${FATFS_SRC}
)

target_link_libraries(abc_pico
    pico_stdlib
    pico_multicore
    pico_scanvideo_dpi
    pico_rand
    hardware_sync
    hardware_timer
    hardware_gpio
    hardware_pwm
    hardware_clocks
    m
)

target_compile_options(abc_pico PRIVATE
    -Wall
    -Wextra
    -O2
    -fcommon      # z80.h defines globals in the header; merge tentative defs
)

target_compile_definitions(abc_pico PRIVATE
    Z80ASM_EMBEDDED       # build z80asm.c as an embedded library (no file I/O, no main)
)

# Keyboard/serial input over USB CDC; no UART
pico_enable_stdio_usb(abc_pico 1)
pico_enable_stdio_uart(abc_pico 0)

pico_add_extra_outputs(abc_pico)
