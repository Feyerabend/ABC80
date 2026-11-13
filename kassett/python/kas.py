#!/usr/bin/env python3
"""
kas.py  blockfiler (.kas) för ABC-kassetten: lista, packa och packa upp.

  kas.py lista BAND.kas                 filerna och blocken på bandet
  kas.py packa -o BAND.kas FIL ...      filer -> block (.BAS/.TXT blir text)
  kas.py upp [-d KATALOG] BAND.kas      block -> filer

Vid packning kan typen anges med -t (text) eller -b (binär), och
tidkonstanten för ABC 800 med --tk. Text packas som ABC80-ROM:en
(TXTPRT) gör, med CR som radslut; LF tas bort.

S. Lonnert 2026. Public domain.
"""

import argparse
import os
import sys

import abckas as k


def lista(args):
    blocks = k.read_kas(args.band)
    files = k.group_files(blocks)
    print(f'{args.band}: {len(blocks)} block, {len(files)} fil(er)')
    for f in files:
        n = len(f.blocks)
        miss = f.missing()
        tk = f' tk {f.tk}' if f.tk else ''
        print(f'  {f.name or "(namn saknas)":12s} {f.kind():4s} {n:4d} block'
              f'{tk}{"  saknas: " + ",".join(map(str, miss)) if miss else ""}')
    return 0


def packa(args):
    kind = 'TEXT' if args.text else 'BIN' if args.binar else None
    blocks = []
    for p in args.filer:
        data = open(p, 'rb').read()
        fb = k.make_file(p, data, kind, args.tk)
        blocks += fb
        print(f'{p}: {k.show_name(fb[0][3:14])}, {len(fb) - 1} datablock', file=sys.stderr)
    k.write_kas(args.output, blocks)
    return 0


def upp(args):
    files = k.group_files(k.read_kas(args.band))
    os.makedirs(args.katalog, exist_ok=True)
    fel = 0
    for i, f in enumerate(files):
        name = f.name or f'NAMNLOS{i}'
        path = os.path.join(args.katalog, name.replace('/', '_'))
        open(path, 'wb').write(f.content())
        miss = f.missing()
        if miss:
            fel = 1
        print(f'{path}: {f.kind()}{"  saknar block " + ",".join(map(str, miss)) if miss else ""}',
              file=sys.stderr)
    return fel


def main():
    ap = argparse.ArgumentParser(description='blockfiler (.kas) för ABC-kassetten')
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('lista')
    p.add_argument('band')
    p.set_defaults(fn=lista)
    p = sub.add_parser('packa')
    p.add_argument('-o', '--output', required=True)
    g = p.add_mutually_exclusive_group()
    g.add_argument('-t', '--text', action='store_true')
    g.add_argument('-b', '--binar', action='store_true')
    p.add_argument('--tk', type=int, help='tidkonstant i namnblocket (ABC 800)')
    p.add_argument('filer', nargs='+')
    p.set_defaults(fn=packa)
    p = sub.add_parser('upp')
    p.add_argument('-d', '--katalog', default='ut')
    p.add_argument('band')
    p.set_defaults(fn=upp)
    args = ap.parse_args()
    return args.fn(args)


if __name__ == '__main__':
    sys.exit(main())
