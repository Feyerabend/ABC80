# Makefile — zx81_pico
#
# Targets:
#   make              — build airfight binary + Pico firmware (default)
#   make airfight     — assemble airfight.asm → games/airfight.p + src/airfight_bin.h
#   make firmware     — cmake/ninja build only (assumes airfight up to date)
#   make flash        — flash last-built .uf2 via picotool
#   make load         — build airfight.p and send to Pico over serial
#   make hello        — build smoke-test hello.p (HELLO on screen)
#   make send P=f.p   — send any .p file:  make send P=games/airfight.p
#   make clean        — remove build artifacts
#   make monitor      — open USB CDC serial monitor (screen)
#
# Serial port is auto-detected (/dev/tty.usbmodem*).
# Override:  make load PORT=/dev/ttyACM0

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

# Serial port — auto-detected; override with: make load PORT=/dev/ttyACM0
PORT ?= $(shell ls /dev/tty.usbmodem* 2>/dev/null | head -1)

.PHONY: all airfight firmware flash load hello send clean monitor

all: airfight firmware

# Assemble airfight.asm → games/airfight.p + src/airfight_bin.h
airfight:
	@echo "==> Building airfight..."
	python3 tools/make_p.py games/airfight.asm --header

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

# Build airfight.p and send to Pico
load: airfight
	@test -n "$(PORT)" || { echo "ERROR: No USB CDC device found (Pico plugged in? Or set PORT=)"; exit 1; }
	@echo "==> Sending games/airfight.p to $(PORT) ..."
	python3 tools/send_p_termios.py $(PORT) games/airfight.p

# Build smoke-test hello.p (writes HELLO to screen, loops)
hello:
	@echo "==> Building hello.p ..."
	python3 tools/make_hello.py games/hello.p
	@echo "==> games/hello.p ready  (use 'make send P=games/hello.p' to load)"

# Send any .p file:  make send P=games/airfight.p
send:
	@test -n "$(P)" || { echo "Usage: make send P=<file.p>"; exit 1; }
	@test -n "$(PORT)" || { echo "ERROR: No USB CDC device found (Pico plugged in? Or set PORT=)"; exit 1; }
	@echo "==> Sending $(P) to $(PORT) ..."
	python3 tools/send_p_termios.py $(PORT) $(P)

# Remove all generated artifacts
clean:
	rm -rf $(BUILD_DIR)
	rm -f src/airfight_bin.h games/airfight.p games/hello.p

# Open USB CDC serial monitor
monitor:
	@DEV=$$(ls /dev/tty.usbmodem* 2>/dev/null | head -1); \
	  [ -n "$$DEV" ] || { echo "ERROR: No USB CDC device found (Pico plugged in?)"; exit 1; }; \
	  echo "==> Monitoring $$DEV (Ctrl-A Ctrl-D to detach)"; \
	  screen "$$DEV" 115200
