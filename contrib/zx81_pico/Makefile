# Makefile — zx81_pico
#
# Targets:
#   make            — build game binary + firmware (default)
#   make airfight   — assemble airfight.asm → src/airfight_bin.h
#   make firmware   — cmake/ninja build only (assumes airfight up to date)
#   make flash      — flash last-built .uf2 via picotool
#   make clean      — remove build artifacts
#   make monitor    — open USB CDC serial monitor (screen)

# ── Pico SDK paths ────────────────────────────────────────────────────────────
PICO_HOME   := $(HOME)/.pico-sdk
PICO_SDK    := $(PICO_HOME)/sdk/2.2.0
CMAKE_BIN   := $(PICO_HOME)/cmake/v3.31.5/bin
TOOLCHAIN   := $(PICO_HOME)/toolchain/14_2_Rel1/bin
NINJA_BIN   := $(PICO_HOME)/ninja/v1.12.1
PICOTOOL    := $(PICO_HOME)/picotool/2.2.0-a4/picotool/picotool

EXPORT_PATH := $(CMAKE_BIN):$(TOOLCHAIN):$(NINJA_BIN):$(PATH)

# ── Build paths ───────────────────────────────────────────────────────────────
BUILD_DIR   := build
TARGET      := zx81_pico
UF2         := $(BUILD_DIR)/$(TARGET).uf2

NCPU := $(shell sysctl -n hw.logicalcpu 2>/dev/null || nproc 2>/dev/null || echo 4)

.PHONY: all airfight firmware flash clean monitor

all: airfight firmware

# Assemble airfight.asm → games/airfight.p + src/airfight_bin.h
airfight:
	@echo "==> Building airfight..."
	bash tools/build_airfight.sh

# Build Pico firmware (cmake + ninja)
firmware:
	@echo "==> Configuring..."
	PATH="$(EXPORT_PATH)" PICO_SDK_PATH="$(PICO_SDK)" \
	    cmake -S . -B $(BUILD_DIR) \
	          -DPICO_BOARD=pico2 \
	          -DPICO_COPY_TO_RAM=1 \
	          -DCMAKE_BUILD_TYPE=Release \
	          --log-level=WARNING
	@echo "==> Building..."
	PATH="$(EXPORT_PATH)" cmake --build $(BUILD_DIR) --parallel $(NCPU)
	@echo "==> Built: $(UF2)"

# Flash via picotool (no BOOTSEL button needed)
flash:
	@test -f $(UF2) || { echo "ERROR: $(UF2) not found — run 'make firmware' first"; exit 1; }
	@echo "==> Flashing $(UF2) ..."
	DYLD_LIBRARY_PATH="$(dir $(PICOTOOL)):$${DYLD_LIBRARY_PATH:-}" \
	    $(PICOTOOL) load -f -x $(UF2)
	@echo "==> Done — Pico is running the new firmware"

# Remove all generated artifacts
clean:
	rm -rf $(BUILD_DIR)
	rm -f tools/make_airfight src/airfight_bin.h games/airfight.p

# Open USB CDC serial monitor
monitor:
	@DEV=$$(ls /dev/tty.usbmodem* 2>/dev/null | head -1); \
	  [ -n "$$DEV" ] || { echo "ERROR: No USB CDC device found (Pico plugged in?)"; exit 1; }; \
	  echo "==> Monitoring $$DEV (Ctrl-A Ctrl-D to detach)"; \
	  screen "$$DEV" 115200
