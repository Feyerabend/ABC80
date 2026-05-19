#!/usr/bin/env python3
"""
Extract kernal binary from kernal.lst.

Listing column layout (CR6502 assembler):
  col  0- 4  error field (usually blank)
  col  5- 8  address (4 hex digits)
  col  9-11  spacing
  col 12-25  code field (bytes/words) — STOPS HERE; seq number starts at 26
  col 26+    sequence number + source text (ignored)

Token rules within the code field:
  2-char hex  → one byte, in memory order
  4-char hex  → 16-bit value displayed big-endian, stored little-endian

Coverage: $E4A0–$FFFF; gap $E000–$E49F is filled with 0xFF (editor section
was in the commented-out .include files and is not in this listing).
"""
import re, sys

ROM_START = 0xE000
ROM_END   = 0xFFFF
ROM_SIZE  = ROM_END - ROM_START + 1

buf = bytearray([0xFF] * ROM_SIZE)

addr_re = re.compile(r'^\s{4,6}([0-9A-F]{4})   ', re.IGNORECASE)

with open('kernal.lst', 'r', errors='replace') as f:
    for line in f:
        m = addr_re.match(line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        if addr < ROM_START or addr > ROM_END:
            continue

        # Code field is columns 12–25 (14 chars); slice before seq number.
        code_field = line[12:26]
        tokens = code_field.split()

        pos = addr
        for tok in tokens:
            if not all(c in '0123456789ABCDEFabcdef' for c in tok):
                break
            if len(tok) == 2:
                b = int(tok, 16)
                if ROM_START <= pos <= ROM_END:
                    buf[pos - ROM_START] = b
                pos += 1
            elif len(tok) == 4:
                # big-endian display → little-endian storage
                word = int(tok, 16)
                if ROM_START <= pos <= ROM_END:
                    buf[pos - ROM_START] = word & 0xFF
                if ROM_START <= pos + 1 <= ROM_END:
                    buf[pos + 1 - ROM_START] = (word >> 8) & 0xFF
                pos += 2

with open('kernal.901486-04.bin', 'wb') as f:
    f.write(buf)

nmi   = buf[0x1FFA] | (buf[0x1FFB] << 8)
reset = buf[0x1FFC] | (buf[0x1FFD] << 8)
irq   = buf[0x1FFE] | (buf[0x1FFF] << 8)
print(f"NMI   vector: ${nmi:04X}")
print(f"RESET vector: ${reset:04X}")
print(f"IRQ   vector: ${irq:04X}")
print(f"Bytes at $FD22 (reset entry): {buf[0x1D22]:02X} {buf[0x1D23]:02X} {buf[0x1D24]:02X}")
print(f"Wrote kernal.901486-04.bin ({ROM_SIZE} bytes, 0xFF fill for $E000–$E49F)")
