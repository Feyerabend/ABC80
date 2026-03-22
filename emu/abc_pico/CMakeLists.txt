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

set(PICO_BOARD pico2_w CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(abc_pico C CXX ASM)

pico_sdk_init()

add_executable(abc_pico
    main.c
    abc80.c
    z80.c
    display.c
)

target_include_directories(abc_pico PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(abc_pico
    pico_stdlib
    hardware_spi
    hardware_dma
    hardware_gpio
    hardware_irq
    hardware_sync
    hardware_timer
    m
)

target_compile_options(abc_pico PRIVATE
    -Wall
    -Wextra
    -O2
    -fcommon      # z80.h defines globals in the header; merge tentative defs
)

# Keyboard/serial input over USB CDC; no UART
pico_enable_stdio_usb(abc_pico 1)
pico_enable_stdio_uart(abc_pico 0)

pico_add_extra_outputs(abc_pico)
