#!/usr/bin/env python3
"""
fetch_roms.py  --  get the VIC-20 ROMs and generate include/rom_*.h

The Commodore ROMs are not part of this repository. This script downloads
them (or copies them from a directory you already have, e.g. a VICE
installation), checks each file against a known SHA-256, keeps the .bin
files in roms/ and writes the C headers the firmware is built from:

    roms/basic.901486-01.bin       -> include/rom_basic.h   (8 KB)
    roms/kernal.901486-07.bin      -> include/rom_kernal.h  (8 KB)
    roms/characters.901460-03.bin  -> include/rom_char.h    (4 KB)

Usage (from vic20_pico/):
    python3 roms/fetch_roms.py              download from zimmers.net
    python3 roms/fetch_roms.py --from DIR   use files with these names in DIR
    python3 roms/fetch_roms.py --base URL   download from another mirror

Only the Python standard library is used.
"""
import argparse
import hashlib
import os
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

BASE = 'https://www.zimmers.net/anonftp/pub/cbm/firmware/computers/vic20/'

# file name, size, SHA-256, header, array name
ROMS = [
    ('basic.901486-01.bin', 8192,
     '8be2d882e0eeb1c6923a43660236ea69d43ca8a3bd0613b1b6c910601584fbed',
     'include/rom_basic.h', 'rom_basic'),
    ('kernal.901486-07.bin', 8192,
     'ff6d1416b91a71f5240a5bb8adc9fdc1b955374c48322ca47b3e436334f168fb',
     'include/rom_kernal.h', 'rom_kernal'),
    ('characters.901460-03.bin', 4096,
     'bda280726cfe069ca70ef5e2f2d95a29f69d3950c859a12e6979464963fcf502',
     'include/rom_char.h', 'rom_char'),
]


def ok(data, size, sha):
    return len(data) == size and hashlib.sha256(data).hexdigest() == sha


def get(name, size, sha, args):
    path = os.path.join(HERE, name)
    if os.path.exists(path):
        with open(path, 'rb') as f:
            data = f.read()
        if ok(data, size, sha):
            print(f'  {name}: already in roms/')
            return data
        print(f'  {name}: roms/ copy does not match, fetching again')

    if args.src:
        src = os.path.join(args.src, name)
        with open(src, 'rb') as f:
            data = f.read()
        where = src
    else:
        url = args.base.rstrip('/') + '/' + name
        req = urllib.request.Request(url, headers={'User-Agent': 'fetch_roms.py'})
        with urllib.request.urlopen(req, timeout=30) as r:
            data = r.read()
        where = url

    if not ok(data, size, sha):
        sys.exit(f'ERROR: {name} from {where} has the wrong size or SHA-256 '
                 f'({len(data)} bytes, {hashlib.sha256(data).hexdigest()})')
    with open(path, 'wb') as f:
        f.write(data)
    print(f'  {name}: {where}')
    return data


def header(name, array, data, path):
    lines = [f'/* Auto-generated from {name} by roms/fetch_roms.py — do not edit */',
             '#pragma once',
             '#include <stdint.h>',
             f'static const uint8_t {array}[{len(data)}] = {{']
    for i in range(0, len(data), 16):
        lines.append('    ' + ', '.join(f'0x{b:02X}' for b in data[i:i + 16]) + ',')
    lines.append('};')
    with open(os.path.join(ROOT, path), 'w') as f:
        f.write('\n'.join(lines) + '\n')
    print(f'  -> {path}')


def main():
    p = argparse.ArgumentParser(description='Get the VIC-20 ROMs and generate include/rom_*.h')
    p.add_argument('--from', dest='src', metavar='DIR',
                   help='copy the ROM files from DIR instead of downloading')
    p.add_argument('--base', default=BASE, metavar='URL',
                   help=f'download from this directory (default: {BASE})')
    args = p.parse_args()

    for name, size, sha, hdr, array in ROMS:
        header(name, array, get(name, size, sha, args), hdr)
    print('Done. Build with ./build_and_flash.sh')


if __name__ == '__main__':
    main()
