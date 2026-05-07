#!/usr/bin/env python3
"""bin2c.py <input.bin> <output.h> <varname>  —  xxd -i replacement"""
import sys

def main():
    if len(sys.argv) != 4:
        print(f"usage: {sys.argv[0]} <input.bin> <output.h> <varname>", file=sys.stderr)
        sys.exit(1)

    infile, outfile, varname = sys.argv[1], sys.argv[2], sys.argv[3]

    with open(infile, "rb") as f:
        data = f.read()

    with open(outfile, "w") as f:
        f.write(f"unsigned char {varname}[] = {{\n")
        for i, b in enumerate(data):
            if i % 12 == 0:
                f.write("  ")
            f.write(f"0x{b:02x}")
            if i < len(data) - 1:
                f.write(", ")
            if i % 12 == 11:
                f.write("\n")
        if len(data) % 12 != 0:
            f.write("\n")
        f.write("};\n")
        f.write(f"unsigned int {varname}_len = {len(data)};\n")

if __name__ == "__main__":
    main()
