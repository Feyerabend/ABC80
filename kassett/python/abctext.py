#!/usr/bin/env python3
"""
abctext.py  text mellan ABC och dagens filer: teckenkod och radslut.

  abctext.py [--kod 7|dos|utf8] [--rad cr|crlf|lf] [-o UT] FIL ...
  abctext.py --abc [--kod 7|dos|utf8] [--rad cr|crlf|lf] [-o UT] FIL ...

Utan --abc: från ABC (7 bitar, radslut CR eller CR LF) till dagens fil.
  --kod  7     som den är ([\\]{|} och så vidare)
         dos   ÄÖÅÜÉ äöåüé i teckentabell 850 (MS-DOS)
         utf8  i UTF-8 (standard)
  --rad  cr, crlf eller lf (standard)

Med --abc: från dagens fil till ABC. --kod säger vad filen har (utan:
gissa; UTF-8 om det går, annars 850 om där finns byte över $7F,
annars 7). Tecken som ABC inte har blir '?'. --rad är radslutet i
filen som skrivs (standard cr, som ABC).

Filerna läses efter varandra (inga: stdin, '-' också) och skrivs till
UT (eller stdout). '$' är alltid '$': ¤ står bara på tangenten.

S. Lönnert 2026. Public domain.
"""

import argparse
import sys

import abckas


def main():
    ap = argparse.ArgumentParser(
        prog='abctext',
        description='text mellan ABC och dagens filer: teckenkod och radslut')
    ap.add_argument('--abc', action='store_true',
                    help='till ABC (annars från ABC)')
    ap.add_argument('--kod', choices=abckas.KODER,
                    help='teckenkod i dagens fil: 7, dos (850) eller utf8')
    ap.add_argument('--rad', choices=sorted(abckas.RADER),
                    help='radslut i filen som skrivs: cr, crlf eller lf')
    ap.add_argument('-o', metavar='UT', help='skriv hit i stället för stdout')
    ap.add_argument('fil', nargs='*', help='filer (inga: stdin)')
    a = ap.parse_args()

    fels = 0
    ut = bytearray()
    for f in a.fil or ['-']:
        if f == '-':
            data = sys.stdin.buffer.read()
        else:
            try:
                data = open(f, 'rb').read()
            except OSError as e:
                print(f'abctext: {f}: {e.strerror}', file=sys.stderr)
                fels += 1
                continue
        if a.abc:
            t = abckas.text_in(data, a.kod)
            ut += abckas.text_ut(t, '7', a.rad or 'cr')
        else:
            ut += abckas.text_ut(data, a.kod or 'utf8', a.rad or 'lf')
    if a.o:
        open(a.o, 'wb').write(ut)
    else:
        sys.stdout.buffer.write(ut)
    return 1 if fels else 0


if __name__ == '__main__':
    sys.exit(main())
