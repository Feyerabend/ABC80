#!/usr/bin/env python3
"""
make_hello.py — Minimal ZX81 .p file: machine code writes HELLO to screen.

Tests the whole chain:
  .p load → BASIC RAND USR 32768 → machine code → display rendering

Usage:  python3 tools/make_hello.py [output.p]

Machine code (ORG 0x8000):
  1. Disable NMI
  2. Point D_FILE sysvar to 0x8100 (must be >=0x8000 for echo display to work)
  3. Fill display file with spaces and row-HALTs
  4. Write "HELLO" at row 12, col 12 in ZX81 char codes
  5. Infinite loop

If you see "HELLO" on screen: .p loading + display rendering + USR call all work.
"""

import sys

PFILE_ORIGIN    = 0x4009
DFILE_ADDR      = 0x8100   # MUST be >= 0x8000: SET 7,H in ROM is then a no-op,
                            # so the "echo display" stays in real RAM, not unmapped
                            # 0xC000+ (which would be 0xFF = RST 38h = stack corruption)
BASIC_LINE_ADDR = 0x407D   # standard ZX81 BASIC start (sysvars = 0x74 bytes = 0x4009+0x74)
VARS_ADDR       = 0x408F   # BASIC_LINE_ADDR + 18 (4 header + 14 body)
ELINE_ADDR      = 0x4090
STKBOT_ADDR     = 0x4092
GAME_CODE_ADDR  = 0x8000

# ZX81 char codes for HELLO
# A=0x26 B=0x27 C=0x28 D=0x29 E=0x2A F=0x2B G=0x2C H=0x2D I=0x2E
# J=0x2F K=0x30 L=0x31 M=0x32 N=0x33 O=0x34 P=0x35 ...

# Row 12, col 12 address in display file:
#   D_FILE+1 = first char of row 0
#   Row n starts at DFILE+1 + n*33
#   Row 12, col 12 = 0x5101 + 12*33 + 12 = 0x5101 + 408 = 0x5299
HELLO_ADDR = DFILE_ADDR + 1 + 12*33 + 12   # 0x5299

# Machine code (hand-assembled):
code = bytes([
    0xAF,                    # XOR A
    0xD3, 0xFD,              # OUT (0xFD), A      ; disable NMI
    # D_FILE sysvar (0x400C:0x400D) = 0x5100
    0x21, DFILE_ADDR & 0xFF, DFILE_ADDR >> 8,  # LD HL, 0x5100
    0x22, 0x0C, 0x40,        # LD (0x400C), HL
    # Init display: leading HALT at 0x5100, then 24 rows of 32 spaces + HALT
    0x21, DFILE_ADDR & 0xFF, DFILE_ADDR >> 8,  # LD HL, 0x5100
    0x36, 0x76,              # LD (HL), 0x76      ; leading HALT
    0x23,                    # INC HL
    0x06, 24,                # LD B, 24           ; 24 rows
    # INIT_ROW @ offset 17 = addr 0x8011
    0x0E, 32,                # LD C, 32           ; 32 cols per row
    # INIT_COL @ offset 19 = addr 0x8013
    0x36, 0x00,              # LD (HL), 0         ; space
    0x23,                    # INC HL
    0x0D,                    # DEC C
    0x20, 0xFA,              # JR NZ, -6          ; → INIT_COL
    0x36, 0x76,              # LD (HL), 0x76      ; row HALT
    0x23,                    # INC HL
    0x10, 0xF3,              # DJNZ -13           ; → INIT_ROW
    # Write "HELLO" at HELLO_ADDR
    0x21, HELLO_ADDR & 0xFF, HELLO_ADDR >> 8,    # LD HL, HELLO_ADDR
    0x36, 0x2D,              # LD (HL), 'H'  (0x2D)
    0x23,                    # INC HL
    0x36, 0x2A,              # LD (HL), 'E'  (0x2A)
    0x23,                    # INC HL
    0x36, 0x31,              # LD (HL), 'L'  (0x31)
    0x23,                    # INC HL
    0x36, 0x31,              # LD (HL), 'L'  (0x31)
    0x23,                    # INC HL
    0x36, 0x34,              # LD (HL), 'O'  (0x34)
    # LOOP: JP LOOP (infinite loop at offset 47 = addr 0x802F)
    0xC3, (GAME_CODE_ADDR + 47) & 0xFF, (GAME_CODE_ADDR + 47) >> 8,
])

assert len(code) == 50, f"code length mismatch: {len(code)}"

# Verify HELLO_ADDR
expected_hello = DFILE_ADDR + 1 + 12 * 33 + 12
assert HELLO_ADDR == expected_hello, f"HELLO_ADDR {HELLO_ADDR:#x} != {expected_hello:#x}"

FILE_END  = max(DFILE_ADDR + 1 + 24*33, GAME_CODE_ADDR + len(code))
buf       = bytearray(FILE_END - PFILE_ORIGIN)

def wr(addr, val):
    buf[addr - PFILE_ORIGIN] = val & 0xFF

def wr16(addr, val):
    buf[addr - PFILE_ORIGIN]     = val & 0xFF
    buf[addr - PFILE_ORIGIN + 1] = (val >> 8) & 0xFF

# Sysvars
wr(0x4009, 0)
wr16(0x400A, 10)               # E_PPC = line 10
wr16(0x400C, DFILE_ADDR)       # D_FILE
wr16(0x400E, DFILE_ADDR + 1)   # DF_CC
wr16(0x4010, VARS_ADDR)        # VARS
wr16(0x4014, ELINE_ADDR)       # E_LINE
wr16(0x4016, ELINE_ADDR)       # CH_ADD
wr16(0x401A, STKBOT_ADDR)      # STKBOT
wr16(0x401C, STKBOT_ADDR)      # STKEND
wr(0x401E, 0)
wr16(0x401F, 0x4000)           # MEM
wr(0x4022, 2)                  # DF_SZ
wr16(0x4023, 10)               # S_TOP
wr(0x4028, 55)                 # MARGIN
wr16(0x4029, BASIC_LINE_ADDR)  # NXTLIN
wr(0x403B, 0x40)               # CDFLAG: bit6=1 → ROM proceeds to BASIC

# BASIC line 10: RAND USR 32768
# body: FD(RAND) D8(USR) "32768" 7E(num) float(32768) 76(NEWLINE)
body = bytes([
    0xF9,                               # RAND token
    0xD4,                               # USR token
    0x1F, 0x1E, 0x23, 0x22, 0x24,       # "32768" in ZX81 char codes
    0x7E,                               # number follows
    0x90, 0x00, 0x00, 0x00, 0x00,       # float 32768.0 (exp=0x90)
    0x76,                               # NEWLINE (ZX81 BASIC line terminator)
])
p = BASIC_LINE_ADDR - PFILE_ORIGIN
buf[p]   = 0x00           # line number hi
buf[p+1] = 0x0A           # line number lo = 10
buf[p+2] = len(body)      # body length lo
buf[p+3] = 0              # body length hi
buf[p+4:p+4+len(body)] = body

# VARS marker
buf[VARS_ADDR  - PFILE_ORIGIN] = 0x80
buf[ELINE_ADDR - PFILE_ORIGIN] = 0x0D

# Display file: leading HALT + 24 rows of (32 spaces + HALT)
d = DFILE_ADDR - PFILE_ORIGIN
buf[d] = 0x76
off = d + 1
for _ in range(24):
    for __ in range(32):
        buf[off] = 0x00
        off += 1
    buf[off] = 0x76
    off += 1

# Machine code
g = GAME_CODE_ADDR - PFILE_ORIGIN
buf[g:g+len(code)] = code

out = sys.argv[1] if len(sys.argv) > 1 else 'games/hello.p'
with open(out, 'wb') as f:
    f.write(buf)

print(f'Written {len(buf)} bytes → {out}')
print(f'  D_FILE  = {DFILE_ADDR:#06x}')
print(f'  HELLO   at {HELLO_ADDR:#06x} (row 12, col 12)')
print(f'  Code    = {GAME_CODE_ADDR:#06x}–{GAME_CODE_ADDR+len(code)-1:#06x}')
print(f'  To load: python3 tools/send_p_termios.py /dev/tty.usbmodem* {out}')
