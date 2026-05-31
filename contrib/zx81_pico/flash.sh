#!/bin/bash
# flash.sh    build zx81_pico and flash to a connected Pico 2
#
# No BOOTSEL button required: picotool reboots the Pico into flash mode
# automatically when it is running firmware with USB stdio enabled.
#
# Usage:
#   ./flash.sh          build (if needed) + flash
#   ./flash.sh build    force a clean build first
#   ./flash.sh flash    flash the last build without rebuilding
#   ./flash.sh clean    wipe the build directory

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
TARGET="zx81_pico"
UF2="$BUILD_DIR/${TARGET}.uf2"

PICOTOOL="$HOME/.pico-sdk/picotool/2.2.0-a4/picotool/picotool"

export PATH="$HOME/.pico-sdk/cmake/v3.31.5/bin:\
$HOME/.pico-sdk/toolchain/14_2_Rel1/bin:\
$HOME/.pico-sdk/ninja/v1.12.1:\
$PATH"
export PICO_SDK_PATH="$HOME/.pico-sdk/sdk/2.2.0"
# picotool needs its bundled libusb on macOS
export DYLD_LIBRARY_PATH="$(dirname "$PICOTOOL"):${DYLD_LIBRARY_PATH:-}"

# --> helpers

build() {
    echo "==> Building airfight game binary..."
    bash "$SCRIPT_DIR/tools/build_airfight.sh"

    echo "==> Configuring..."
    mkdir -p "$BUILD_DIR"
    cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
          -DPICO_BOARD=pico2 \
          -DPICO_COPY_TO_RAM=1 \
          -DCMAKE_BUILD_TYPE=Release \
          --log-level=WARNING
    echo "==> Building..."
    cmake --build "$BUILD_DIR" --parallel "$(sysctl -n hw.logicalcpu)"
    echo "==> Built: $UF2"
}

flash() {
    if [ ! -f "$UF2" ]; then
        echo "ERROR: $UF2 not found — run './flash.sh build' first"
        exit 1
    fi
    echo "==> Flashing $UF2 ..."
    # -f  force: reboot running firmware into BOOTSEL mode automatically
    # -x  execute: reboot into the new app after flashing
    "$PICOTOOL" load -f -x "$UF2"
    echo "==> Done — Pico is running the new firmware"
}

clean() {
    echo "==> Cleaning $BUILD_DIR ..."
    rm -rf "$BUILD_DIR"
    echo "==> Done"
}

monitor() {
    # Find the USB CDC serial device and open it
    DEV=$(ls /dev/tty.usbmodem* 2>/dev/null | head -1)
    if [ -z "$DEV" ]; then
        echo "ERROR: No USB CDC device found (is the Pico plugged in and running?)"
        exit 1
    fi
    echo "==> Monitoring $DEV (Ctrl-A Ctrl-D to detach from screen)"
    screen "$DEV" 115200
}

# --> main

case "${1:-auto}" in
    build)   build   ;;
    flash)   flash   ;;
    clean)   clean   ;;
    monitor) monitor ;;
    auto)    build && flash ;;
    *)
        echo "Usage: $0 [build|flash|clean|monitor]"
        echo "  (no argument) .. build then flash"
        exit 1
        ;;
esac
