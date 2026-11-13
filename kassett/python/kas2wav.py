#!/usr/bin/env python3
"""
kas2wav.py  blockfil (.kas) eller vanliga filer -> WAV med ABC80:s signal.

  kas2wav.py -o BAND.wav BAND.kas
  kas2wav.py -o BAND.wav PROG.BAS DATA.BIN     (packas som kas.py packa,
                                               med -t, -b och --tk som där)

Signalen är FM med ROM:ens bitceller (3993 T för en nolla, 4065 T för en
etta vid 2,9952 MHz). --lead är tystnaden före varje namnblock och --gap
före varje datablock.

S. Lonnert 2026. Public domain.
"""

import argparse
import sys

import abckas as k


def main():
    ap = argparse.ArgumentParser(description='.kas eller filer -> WAV (ABC80)')
    ap.add_argument('-o', '--output', required=True)
    ap.add_argument('-r', '--rate', type=int, default=44100)
    ap.add_argument('--bits', type=int, choices=(8, 16), default=16)
    ap.add_argument('--lead', type=float, default=2.0, help='sekunder före namnblocket')
    ap.add_argument('--gap', type=float, default=0.5, help='sekunder före datablocken')
    g = ap.add_mutually_exclusive_group()
    g.add_argument('-t', '--text', action='store_true', help='packa filerna som text')
    g.add_argument('-b', '--binar', action='store_true', help='packa filerna binärt')
    ap.add_argument('--tk', type=int, help='tidkonstant i namnblocket (ABC 800)')
    ap.add_argument('filer', nargs='+')
    args = ap.parse_args()
    blocks = []
    for p in args.filer:
        if p.lower().endswith('.kas'):
            blocks += k.read_kas(p)
        else:
            kind = 'TEXT' if args.text else 'BIN' if args.binar else None
            blocks += k.make_file(p, open(p, 'rb').read(), kind, args.tk)
    k.write_wav(args.output, blocks, args.rate, args.lead, args.gap, bits=args.bits)
    print(f'{args.output}: {len(blocks)} block', file=sys.stderr)
    return 0


if __name__ == '__main__':
    sys.exit(main())
