#!/usr/bin/env bash
set -e
# sample script .. change paths
PICO_SDK_ROOT="/PATH_TO_SDK/.pico-sdk"
CMAKE="$PICO_SDK_ROOT/cmake/v3.31.5/CMake.app/Contents/bin/cmake"
NINJA="$PICO_SDK_ROOT/ninja/v1.12.1/ninja"
TOOLCHAIN="$PICO_SDK_ROOT/toolchain/14_2_Rel1/bin"
PICOTOOL="$PICO_SDK_ROOT/picotool/2.2.0-a4/picotool/picotool"

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
UF2="$BUILD_DIR/vic20.uf2"
JOBS=$(sysctl -n hw.logicalcpu 2>/dev/null || echo 4)

export PATH="$TOOLCHAIN:$PATH"
# picotool needs its bundled libusb on macOS
export DYLD_LIBRARY_PATH="$(dirname "$PICOTOOL"):${DYLD_LIBRARY_PATH:-}"

mkdir -p "$BUILD_DIR"

# Remove cmake's generator/cache files so switching generators never conflicts.
rm -rf "$BUILD_DIR/CMakeCache.txt" "$BUILD_DIR/CMakeFiles" "$BUILD_DIR/build.ninja" "$BUILD_DIR/.ninja_deps" "$BUILD_DIR/.ninja_log"

echo "-- CMake configure --"
# Any extra cmake flags (e.g. -DAUTORUN_AIRFIGHT=ON) can be passed as
# arguments to this script:  ./build_and_flash.sh -DAUTORUN_AIRFIGHT=ON
"$CMAKE" -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_MAKE_PROGRAM="$NINJA" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DPICO_SDK_PATH="$PICO_SDK_ROOT/sdk/2.2.0" \
    -DPICO_BOARD=pico2_w \
    -DPICO_COPY_TO_RAM=1 \
    "$@" \
    2>&1

echo "-- Build --"
"$CMAKE" --build "$BUILD_DIR" --parallel "$JOBS" 2>&1

echo ""
echo "-- Flash --"
# -f: force running firmware into BOOTSEL via USB (no button press needed)
# -x: reboot directly into the new app after flashing
"$PICOTOOL" load -f -x "$UF2"

echo ""
echo "Done. Pico 2W running vic20."
