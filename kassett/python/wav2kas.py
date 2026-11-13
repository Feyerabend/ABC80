#!/usr/bin/env python3
"""
wav2kas.py  WAV från ett ABC80-band -> blockfil (.kas) och en rapport.

  wav2kas.py BAND.wav                  skriver BAND.kas
  wav2kas.py -o UT.kas -r BAND.wav     och rapporten block för block

Varje kanal avkodas flera gånger med olika inställningar, och ett block
godtas när summan stämmer. Rapporten visar också block som hittades men
aldrig gick igenom (fel summa, ETX saknas, oläsbara bitar).
Avslutas med 1 om någon fil saknar namnblock eller datablock.

S. Lonnert 2023, 2026. Public domain.
"""

import argparse
import os
import sys

import abckas as k


def main():
    ap = argparse.ArgumentParser(description='WAV (ABC80) -> .kas')
    ap.add_argument('-o', '--output')
    ap.add_argument('-r', '--rapport', action='store_true', help='rapport block för block')
    ap.add_argument('-q', '--quick', action='store_true', help='bara en omgång per kanal')
    ap.add_argument('-v', '--verbose', action='store_true')
    ap.add_argument('wav')
    args = ap.parse_args()
    log = (lambda s: print(s, file=sys.stderr)) if args.verbose else None
    blocks, report = k.decode_wav(args.wav, args.quick, log)
    out = args.output or os.path.splitext(args.wav)[0] + '.kas'
    k.write_kas(out, blocks)
    if args.rapport:
        for t, st in report:
            print(f'  {int(t // 60):3d}:{t % 60:05.2f}  {st}')
    files = k.group_files(blocks)
    bad = sum(1 for _, st in report if st != 'ok')
    fel = False
    print(f'{out}: {len(blocks)} block, {len(files)} fil(er), {bad} trasiga')
    for f in files:
        miss = f.missing()
        fel |= bool(miss) or f.header is None
        print(f'  {f.name or "(namn saknas)":12s} {f.kind():4s} {len(f.blocks):4d} block'
              f'{"  saknas: " + ",".join(map(str, miss)) if miss else ""}')
    return 1 if fel else 0


if __name__ == '__main__':
    sys.exit(main())
