#!/usr/bin/env python3
"""Convert a binary ROM file to a C header with a const uint8_t array."""
import sys
import os

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} input.rom output.h")
        sys.exit(1)

    rom_path = sys.argv[1]
    hdr_path = sys.argv[2]

    with open(rom_path, 'rb') as f:
        data = f.read()

    name = "zx81_rom"
    lines = [
        "#pragma once",
        "#include <stdint.h>",
        "",
        f"#define ZX81_ROM_SIZE {len(data)}",
        "",
        f"static const uint8_t {name}[ZX81_ROM_SIZE] = {{",
    ]
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        lines.append("    " + ", ".join(f"0x{b:02X}" for b in chunk) + ",")
    lines.extend(["};", ""])

    with open(hdr_path, 'w') as f:
        f.write("\n".join(lines))

    print(f"Generated {hdr_path} ({len(data)} bytes)")

if __name__ == "__main__":
    main()
