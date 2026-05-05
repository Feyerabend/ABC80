#!/usr/bin/env bash
set -e

PICO_SDK_ROOT="/Users/setlonnert/.pico-sdk"
CMAKE="$PICO_SDK_ROOT/cmake/v3.31.5/CMake.app/Contents/bin/cmake"
NINJA="$PICO_SDK_ROOT/ninja/v1.12.1/ninja"
TOOLCHAIN="$PICO_SDK_ROOT/toolchain/14_2_Rel1/bin"
PICOTOOL="$PICO_SDK_ROOT/picotool/2.2.0-a4/picotool/picotool"

PICO_EXTRAS_PATH="$PICO_SDK_ROOT/extras"

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_DIR/build"
UF2="$BUILD_DIR/abc_pico.uf2"
JOBS=$(sysctl -n hw.logicalcpu 2>/dev/null || echo 4)

export PATH="$TOOLCHAIN:$PATH"

echo "=== Clean build dir ==="
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

echo "=== CMake configure ==="
"$CMAKE" -S "$PROJECT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_MAKE_PROGRAM="$NINJA" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DPICO_EXTRAS_PATH="$PICO_EXTRAS_PATH" \
    2>&1

echo "=== Build ==="
"$CMAKE" --build "$BUILD_DIR" --parallel "$JOBS" 2>&1

echo ""
echo "=== Flash ==="
echo "Trying USB reboot into BOOTSEL mode..."
if "$PICOTOOL" reboot -f 2>/dev/null; then
    sleep 2
else
    echo "USB reboot failed — put the Pico 2W into BOOTSEL mode (hold BOOTSEL, press RUN), then press Enter..."
    read -r
fi

"$PICOTOOL" load -f "$UF2" && "$PICOTOOL" reboot

echo ""
echo "Done! Pico rebooting."
