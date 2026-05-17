#!/usr/bin/env python3
"""
make_p.py — Assemble a Z80 .asm file and package as a ZX81 .p file.

Usage:
    python3 tools/make_p.py game.asm [out.p]
    python3 tools/make_p.py game.asm [out.p] [--dfile 0x8800] [--org 0x8000]
    python3 tools/make_p.py game.asm --header   (also writes src/<name>_bin.h)

Reads ORG and DFILE EQU from the .asm source automatically.
Output defaults to same directory as .asm with .p extension.

ZX81 .p file layout produced:
    0x4009–0x407C  System variables (116 bytes)
    0x407D–...     BASIC line 10: RAND USR <org>
    VARS_ADDR      0x80 VARS end marker
    ...            Zeros from VARS to ORG
    ORG–...        Machine code from assembled binary
    DFILE–...      Display file: 0x76 + 24*(32 spaces + 0x76)  [793 bytes]
"""

import sys, os, re, struct, subprocess, tempfile, argparse, math

SCRIPT_DIR   = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.dirname(SCRIPT_DIR)
Z80ASM       = os.path.join(PROJECT_ROOT, 'old', 'z80asm')

PFILE_ORIGIN    = 0x4009
BASIC_LINE_ADDR = 0x407D   # right after system variables (0x74 = 116 bytes)
DFILE_ROWS      = 24
DFILE_COLS      = 32
DFILE_SIZE      = 1 + DFILE_ROWS * (DFILE_COLS + 1)   # 793 bytes

# ZX81 BASIC token codes (verified from ROM at 0x0111)
TOK_RAND = 0xF9
TOK_USR  = 0xD4


def parse_args():
    p = argparse.ArgumentParser(description='Assemble Z80 .asm → ZX81 .p file')
    p.add_argument('asm', help='Assembly source file')
    p.add_argument('out', nargs='?', help='Output .p file (default: <asm>.p)')
    p.add_argument('--dfile', type=lambda x: int(x, 0), default=None,
                   help='Display file base address (overrides DFILE EQU in .asm)')
    p.add_argument('--org', type=lambda x: int(x, 0), default=None,
                   help='Code origin address (overrides ORG in .asm)')
    p.add_argument('--header', action='store_true',
                   help='Write C header src/<basename>_bin.h with raw binary')
    return p.parse_args()


def extract_asm_params(text):
    """Return (org, dfile_addr) parsed from .asm source; either may be None."""
    org = None
    dfile = None
    for line in text.splitlines():
        line = line.split(';')[0].strip()
        if org is None:
            m = re.match(r'ORG\s+(0x[0-9A-Fa-f]+|\d+)', line, re.IGNORECASE)
            if m:
                org = int(m.group(1), 0)
        if dfile is None:
            m = re.match(r'DFILE\s+EQU\s+(0x[0-9A-Fa-f]+|\d+)', line, re.IGNORECASE)
            if m:
                dfile = int(m.group(1), 0)
    return org, dfile


def zx81_float(n):
    """Encode integer n as 5-byte ZX81 floating-point."""
    if n == 0:
        return b'\x00' * 5
    sign = 1 if n < 0 else 0
    n = abs(n)
    e = math.floor(math.log2(n)) + 1
    frac = n / (2 ** (e - 1)) - 1.0
    m31 = round(frac * (2 ** 31))
    return bytes([
        (e + 128) & 0xFF,
        (sign << 7) | ((m31 >> 24) & 0x7F),
        (m31 >> 16) & 0xFF,
        (m31 >> 8) & 0xFF,
        m31 & 0xFF,
    ])


def encode_number(n):
    """ZX81 digit chars + 0x7E number marker + 5-byte float."""
    digits = bytes(0x1C + int(d) for d in str(n))
    return digits + b'\x7E' + zx81_float(n)


def build_pfile(org, dfile_addr, code_bytes):
    # BASIC body: RAND USR <org> + 0x76 NEWLINE
    body = bytes([TOK_RAND, TOK_USR]) + encode_number(org) + b'\x76'

    VARS_ADDR   = BASIC_LINE_ADDR + 4 + len(body)
    ELINE_ADDR  = VARS_ADDR + 1
    STKBOT_ADDR = ELINE_ADDR + 1

    file_end = dfile_addr + DFILE_SIZE
    buf = bytearray(file_end - PFILE_ORIGIN)

    def wr(a, v):
        buf[a - PFILE_ORIGIN] = v & 0xFF

    def wr16(a, v):
        struct.pack_into('<H', buf, a - PFILE_ORIGIN, v)

    # System variables
    wr(0x4009, 0)                  # VERSN
    wr16(0x400A, 10)               # E_PPC = line 10
    wr16(0x400C, dfile_addr)       # D_FILE
    wr16(0x400E, dfile_addr + 1)   # DF_CC
    wr16(0x4010, VARS_ADDR)        # VARS
    wr16(0x4014, ELINE_ADDR)       # E_LINE
    wr16(0x4016, ELINE_ADDR)       # CH_ADD
    wr16(0x401A, STKBOT_ADDR)      # STKBOT
    wr16(0x401C, STKBOT_ADDR)      # STKEND
    wr(0x401E, 0)                  # BERG
    wr16(0x401F, 0x4000)           # MEM
    wr(0x4022, 2)                  # DF_SZ
    wr16(0x4023, 10)               # S_TOP
    wr(0x4028, 55)                 # MARGIN (PAL)
    wr16(0x4029, BASIC_LINE_ADDR)  # NXTLIN
    wr(0x403B, 0x40)               # CDFLAG: SLOW mode

    # BASIC line 10: RAND USR <org>
    p = BASIC_LINE_ADDR - PFILE_ORIGIN
    buf[p]     = 0x00
    buf[p + 1] = 0x0A
    buf[p + 2] = len(body)
    buf[p + 3] = 0
    buf[p + 4: p + 4 + len(body)] = body

    # VARS end marker
    buf[VARS_ADDR - PFILE_ORIGIN] = 0x80

    # Display file: leading HALT + 24 × (32 spaces + HALT)
    d = dfile_addr - PFILE_ORIGIN
    buf[d] = 0x76
    off = d + 1
    for _ in range(DFILE_ROWS):
        buf[off: off + DFILE_COLS] = b'\x00' * DFILE_COLS
        off += DFILE_COLS
        buf[off] = 0x76
        off += 1

    # Machine code
    g = org - PFILE_ORIGIN
    buf[g: g + len(code_bytes)] = code_bytes

    return bytes(buf), VARS_ADDR


def main():
    args = parse_args()

    with open(args.asm, 'r', encoding='utf-8', errors='replace') as f:
        asm_text = f.read()

    org_from_asm, dfile_from_asm = extract_asm_params(asm_text)
    org        = args.org   if args.org   is not None else org_from_asm
    dfile_addr = args.dfile if args.dfile is not None else dfile_from_asm

    if org is None:
        sys.exit('ERROR: No ORG found in .asm; use --org <addr>')
    if dfile_addr is None:
        sys.exit('ERROR: No DFILE EQU found in .asm; use --dfile <addr>')

    base     = os.path.splitext(os.path.basename(args.asm))[0]
    out_path = args.out or os.path.join(os.path.dirname(args.asm), base + '.p')

    print(f'ORG:   {org:#06x}')
    print(f'DFILE: {dfile_addr:#06x}')

    # Assemble to flat binary
    with tempfile.NamedTemporaryFile(suffix='.bin', delete=False) as tmp:
        tmp_bin = tmp.name
    try:
        result = subprocess.run(
            [Z80ASM, '-o', tmp_bin, args.asm],
            capture_output=True, text=True
        )
        if result.stderr:
            print(result.stderr, end='', file=sys.stderr)
        if result.returncode != 0:
            sys.exit(f'Assembler failed (exit {result.returncode})')
        with open(tmp_bin, 'rb') as f:
            flat_bin = f.read()
    finally:
        if os.path.exists(tmp_bin):
            os.unlink(tmp_bin)

    # Extract code: from ORG up to (but not including) DFILE; trim trailing zeros
    code_raw = flat_bin[org: min(dfile_addr, len(flat_bin))]
    last_nz = len(code_raw)
    while last_nz > 0 and code_raw[last_nz - 1] == 0:
        last_nz -= 1
    code_bytes = code_raw[:last_nz]

    print(f'Code:  {len(code_bytes)} bytes  ({org:#06x}–{org + len(code_bytes) - 1:#06x})')

    pdata, vars_addr = build_pfile(org, dfile_addr, code_bytes)

    with open(out_path, 'wb') as f:
        f.write(pdata)
    print(f'Wrote: {out_path}  ({len(pdata)} bytes)')
    print(f'  BASIC:  10 RAND USR {org} at {BASIC_LINE_ADDR:#06x}')
    print(f'  VARS:   {vars_addr:#06x}')
    print(f'  D_FILE: {dfile_addr:#06x}')

    if args.header:
        hdr_name = base + '_bin.h'
        hdr_path = os.path.join(PROJECT_ROOT, 'src', hdr_name)
        with open(hdr_path, 'w') as f:
            f.write('/* Auto-generated by make_p.py — do not edit */\n')
            f.write('#pragma once\n#include <stdint.h>\n')
            f.write(f'#define {base.upper()}_ORG {org:#06x}u\n\n')
            f.write(f'static const uint8_t {base}_bin[] = {{\n    ')
            for i, byte in enumerate(code_bytes):
                f.write(f'0x{byte:02X}')
                if i + 1 < len(code_bytes):
                    f.write(',\n    ' if i % 16 == 15 else ', ')
            f.write('\n};\n')
        print(f'Header: {hdr_path}  ({len(code_bytes)} bytes of code)')


if __name__ == '__main__':
    main()
