#!/usr/bin/env python3
"""pasm.py -- assembler för p-kod, för provprogram skrivna för hand.

    python3 pasm.py prog.pa prog.pk

Kompilatorn behöver den inte; den skriver p-koden själv. Formen:

    ; kommentar
    GLOBAL 3          globala ord
    PROC HUVUD        den första proceduren är huvudprogrammet (nr 0)
    loop: LDG0        en etikett gäller inom sin procedur
          FJP slut
          CUP 2 FAK   argumentens bytes och procedurens namn
          CSP WRI     standardprocedurens namn
          LSA "Hej"   strängen läggs i koden
          XJP 1 3 a b c
          LDCB 'A'    ett tecken är dess byte

Tal skrivs decimalt eller med $ (hexadecimalt).
"""
import re
import shlex
import struct
import sys

from pkod import CSP, PER_NAMN, REALFLAGGA, VERSION


def tal(t, etiketter=None, adr=None, procs=None):
    if t.startswith("'") and t.endswith("'") and len(t) == 3:
        return ord(t[1])
    if t.startswith("$"):
        return int(t[1:], 16)
    if re.fullmatch(r"-?\d+", t):
        return int(t)
    if t in CSP:
        return CSP[t]
    if procs is not None and t in procs:
        return procs[t]
    if etiketter is not None and t in etiketter:
        return etiketter[t] - adr
    raise ValueError("okänt: " + t)


def utan_kommentar(rad):
    """Raden utan ; och det som följer, utom inom "" och ''."""
    i, ut = 0, None
    for i, c in enumerate(rad):
        if ut:
            if c == ut:
                ut = None
        elif c in "\"'":
            ut = c
        elif c == ";":
            return rad[:i]
    return rad


def las(text):
    """Rader till (procedurer, globala ord)."""
    procs, glob = [], 0
    for nr, rad in enumerate(text.splitlines(), 1):
        rad = utan_kommentar(rad)
        rad = rad.strip()
        if not rad:
            continue
        m = re.match(r"(\w+):\s*(.*)", rad)
        etikett = None
        if m and m.group(1).upper() not in PER_NAMN:
            etikett, rad = m.group(1), m.group(2)
        delar = shlex.split(rad, posix=False) if rad else []
        if delar and delar[0].upper() == "GLOBAL":
            glob = int(delar[1])
            continue
        if delar and delar[0].upper() == "PROC":
            procs.append((delar[1], []))
            continue
        if not procs:
            sys.exit("rad %d: utanför PROC" % nr)
        if etikett:
            procs[-1][1].append(("etikett", etikett, nr))
        if delar:
            procs[-1][1].append(("ins", delar, nr))
    return procs, glob


def koda(namn, ops, adr, etiketter, procs, nr):
    if namn not in PER_NAMN:
        sys.exit("rad %d: okänd instruktion %s" % (nr, namn))
    kod, typ = PER_NAMN[namn]
    ut = bytearray([kod])
    if typ == "n":
        s = ops[0]
        if not (s.startswith('"') and s.endswith('"')):
            sys.exit("rad %d: LSA vill ha en sträng" % nr)
        b = s[1:-1].replace('""', '"').encode("latin-1")
        return ut + bytes([len(b)]) + b
    if typ == "x":
        lo, hi = tal(ops[0]), tal(ops[1])
        mal = ops[2:]
        if len(mal) != hi - lo + 1:
            sys.exit("rad %d: XJP vill ha %d mål" % (nr, hi - lo + 1))
        slut = adr + 5 + 2 * len(mal)
        ut += struct.pack("<hh", lo, hi)
        for m in mal:
            ut += struct.pack("<h", etiketter[m] - slut if etiketter else 0)
        return ut
    if len(ops) != len(typ):
        sys.exit("rad %d: %s vill ha %d operander" % (nr, namn, len(typ)))
    slut = adr + 1 + sum(2 if t in "wr" else 1 for t in typ)
    for t, o in zip(typ, ops):
        if t in "sr":
            if etiketter is None:
                v = 0
            elif o in etiketter:
                v = etiketter[o] - slut
            else:
                v = tal(o)
        else:
            v = tal(o, procs=procs)
        if t == "b":
            if not 0 <= v <= 255:
                sys.exit("rad %d: %s utanför 0..255" % (nr, o))
            ut.append(v)
        elif t == "s":
            if not -128 <= v <= 127:
                sys.exit("rad %d: hoppet till %s är för långt" % (nr, o))
            ut += struct.pack("<b", v)
        else:
            ut += struct.pack("<H", v & 0xFFFF)
    return ut


def assemblera(text):
    procs, glob = las(text)
    procnr = {namn: i for i, (namn, _) in enumerate(procs)}
    koder, real = [], 0
    for namn, rader in procs:
        # Två varv: först etiketternas adresser, sedan koden.
        etiketter, adr = {}, 0
        for slag, d, nr in rader:
            if slag == "etikett":
                etiketter[d] = adr
            else:
                adr += len(koda(d[0].upper(), d[1:], adr, None, procnr, nr))
        kod, adr = bytearray(), 0
        for slag, d, nr in rader:
            if slag == "ins":
                b = koda(d[0].upper(), d[1:], adr, etiketter, procnr, nr)
                kod += b
                adr += len(b)
                if b[0] >= 0xA0:         # REAL: tolken laddar FPP.BIN
                    real = REALFLAGGA
        koder.append(kod)
    tabell, offs = b"", 0
    for k in koder:
        tabell += struct.pack("<H", offs)
        offs += len(k)
    return (b"PK" + bytes([VERSION, 1]) + b"".join(koder) + tabell
            + struct.pack("<HH", 2 * glob, len(koder) | real))


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    with open(sys.argv[1], encoding="utf-8") as f:
        data = assemblera(f.read())
    with open(sys.argv[2], "wb") as f:
        f.write(data)


if __name__ == "__main__":
    main()
