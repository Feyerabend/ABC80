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

target_include_directories(abc_pico PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${FATFS_SRC}
)

target_link_libraries(abc_pico
    pico_stdlib
    pico_multicore
    pico_scanvideo_dpi
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
