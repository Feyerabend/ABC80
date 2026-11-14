#!/usr/bin/env python3
"""
fetch_rom.py  --  get the ZX81 ROM into roms/zx81.rom

The Sinclair ZX81 ROM is not part of this repository. This script downloads
it (or copies it from a file you already have), checks it against a known
SHA-256 and saves it as roms/zx81.rom. CMake then generates
src/zx81_rom_data.h from it (tools/make_rom_header.py) when you build.

The expected ROM is the 8 KB "version 2, improved" ZX81 ROM.

Usage (from zx81_pico/):
    python3 roms/fetch_rom.py               download from k1.spdns.de
    python3 roms/fetch_rom.py --from FILE   use a ZX81 ROM file you have
    python3 roms/fetch_rom.py --url URL     download from another address

Only the Python standard library is used.
"""
import argparse
import hashlib
import os
import sys
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, 'zx81.rom')

URL = ('https://k1.spdns.de/Vintage/Sinclair/80/Sinclair%20ZX81/ROMs/'
       "zx81%20version%202%20'improved'%20rom%20(Sinclair).rom")
SIZE = 8192
SHA256 = '14ad84f4243efcd41587ff46ab932d11087043e8d455a1ed2a227b9657828dfa'


def ok(data):
    return len(data) == SIZE and hashlib.sha256(data).hexdigest() == SHA256


def main():
    p = argparse.ArgumentParser(description='Get the ZX81 ROM into roms/zx81.rom')
    p.add_argument('--from', dest='src', metavar='FILE',
                   help='copy the ROM from FILE instead of downloading')
    p.add_argument('--url', default=URL, help='download from this address')
    args = p.parse_args()

    if os.path.exists(OUT):
        with open(OUT, 'rb') as f:
            if ok(f.read()):
                print('roms/zx81.rom is already there')
                return

    if args.src:
        with open(args.src, 'rb') as f:
            data = f.read()
        where = args.src
    else:
        req = urllib.request.Request(args.url, headers={'User-Agent': 'fetch_rom.py'})
        with urllib.request.urlopen(req, timeout=30) as r:
            data = r.read()
        where = args.url

    if not ok(data):
        sys.exit(f'ERROR: {where} has the wrong size or SHA-256 '
                 f'({len(data)} bytes, {hashlib.sha256(data).hexdigest()})')
    with open(OUT, 'wb') as f:
        f.write(data)
    print(f'roms/zx81.rom  <-  {where}')
    print('Done. Build with make')


if __name__ == '__main__':
    main()
