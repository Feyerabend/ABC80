#!/usr/bin/env python3
"""Fyll mallen med koden ur ROM:en: fyll.py MALL ROM > LISTNING

Mallen är den kommenterade listningen utan själva koden. Varje kodrad har
kvar adress, namn och kommentar, men byten och instruktionen är ersatta av
hål: \\By{?} och \\Fy{slag}{tips}. Det här skriptet läser ROM:en, avkodar
instruktionen eller datan på radens adress och fyller i hålen.

  slag  c       en instruktion (avkodas med z80dec)
        w<n>    DEFW med n ord (w = ett)
        m<n>    DEFM med n tecken
        b<lista> DEFB; listan har ett tecken per del: x = $XX,
                k = 'c'+$80, s<n>. = sträng med n tecken
  tips  vad som ska stå i stället för talen, i tur och ordning, åtskilda
        med |; tomt betyder talet som det är ($XX, $XXXX, +$XX)

Mallens huvud har raden "%% ROM sha256 ..." med ROM:ens kontrollsumma och
"%% LÄNK x:NAMN ..." med de namn som är länkar i listningen.
"""

import hashlib
import os
import re
import sys

_här = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _här)
sys.path.insert(1, os.path.join(_här, "../../../dis/tools"))
from z80dec import decode                               # noqa: E402

ESC = {"\\": r"\textbackslash{}", "{": r"\{", "}": r"\}", "$": r"\$",
       "&": r"\&", "#": r"\#", "^": r"\^{}", "_": r"\_", "%": r"\%",
       "~": r"\textasciitilde{}", '"': r"{\char34}", "'": r"{\char39}",
       "<": r"{\char60}", ">": r"{\char62}", "|": r"{\char124}"}


def esc(s):
    return "".join(ESC.get(c, c) for c in s)


def tt(s):
    """Monospace text där varje blanksteg är exakt en teckenbredd."""
    return "".join(r"\SP{%d}" % len(t) if t[0] == " " else esc(t)
                   for t in re.findall(r" +|[^ ]+", s))


def instr_tex(s, links):
    """Instruktionen i TeX: namnen i links blir länkar, strängar orörda."""
    out = []
    for k, p in enumerate(re.split(r'("[^"]*")', s)):
        if k % 2:
            out.append(tt(p))
            continue
        pos = 0
        for m in re.finditer(r"[A-Za-z_][A-Za-z0-9_]*", p):
            out.append(tt(p[pos:m.start()]))
            w = m.group(0)
            out.append(r"\hyperlink{%s:%s}{%s}" % (links[w], w, esc(w))
                       if w in links else esc(w))
            pos = m.end()
        out.append(tt(p[pos:]))
    return "".join(out)


def standard(o):
    """Operanden som tal, som dis80.py skriver den."""
    if o.kind in ("addr", "imm16"):
        return "$%04X" % o.value
    if o.kind == "disp":
        return "+$%02X" % o.value if o.value >= 0 else "-$%02X" % -o.value
    return "$%02X" % o.value


def koda(rom, org, a, slag, tips):
    """(antal byte, instruktionens text) för raden på adress a."""
    mem = lambda x: rom[x - org] if 0 <= x - org < len(rom) else 0
    if slag == "c":
        ins = decode(mem, a)
        if ins is None:
            raise ValueError("ingen instruktion på $%04X" % a)
        arg = [t or standard(o) for o, t in zip(ins.ops, tips + [""] * len(ins.ops))]
        return ins.length, ins.fmt.format(*arg)
    if slag[0] == "w":
        n = int(slag[1:] or 1)
        tips = tips + [""] * n
        return 2 * n, "DEFW " + ",".join(
            tips[i] or "$%04X" % (mem(a + 2 * i) | mem(a + 2 * i + 1) << 8)
            for i in range(n))
    if slag[0] == "m":
        n = int(slag[1:])
        return n, 'DEFM "%s"' % "".join(chr(mem(a + i)) for i in range(n))
    # DEFB
    n, delar, k = 0, [], 1
    while k < len(slag):
        c = slag[k]
        if c == "x":
            delar.append("$%02X" % mem(a + n))
            n += 1
        elif c == "k":
            delar.append("'%s'+$80" % chr(mem(a + n) & 0x7F))
            n += 1
        elif c == "s":
            e = slag.index(".", k)
            m = int(slag[k + 1:e])
            delar.append('"%s"' % "".join(chr(mem(a + n + i)) for i in range(m)))
            n += m
            k = e
        k += 1
    return n, "DEFB " + ",".join(delar)


def byte_tex(rom, org, a, n):
    by = " ".join("%02X" % rom[a - org + i] for i in range(min(n, 4)))
    return tt(by) + (r"\dots" if n > 4 else "")


HÅL = re.compile(r"\\By\{\?\}(.*?)\\Fy\{([^}]*)\}\{([^}]*)\}")
ADR = re.compile(r"^\\CL\{\\T\{a:([0-9A-F]{4})\}")


def fyll_rad(rad, rom, org, links):
    m = ADR.match(rad)
    h = HÅL.search(rad) if m else None
    if not h:
        return rad
    a = int(m.group(1), 16)
    tips = h.group(3).split("|") if h.group(3) else []
    n, text = koda(rom, org, a, h.group(2), tips)
    return (rad[:h.start()] + r"\By{%s}" % byte_tex(rom, org, a, n) + h.group(1)
            + instr_tex(text, links) + rad[h.end():])


def huvud(rader):
    """(sha256, org, länkar) ur mallens huvud."""
    sha, org, links = None, 0, {}
    for r in rader:
        if r.startswith("%% ROM "):
            f = r.split()
            sha, org = f[3], int(f[5], 16)
        elif r.startswith("%% LÄNK "):
            for x in r.split()[2:]:
                t, n = x.split(":", 1)
                links[n] = t
    return sha, org, links


def main():
    if len(sys.argv) != 3:
        sys.exit("fyll.py MALL ROM > LISTNING")
    rader = open(sys.argv[1], encoding="utf-8").read().split("\n")
    rom = open(sys.argv[2], "rb").read()
    sha, org, links = huvud(rader)
    if sha and hashlib.sha256(rom).hexdigest() != sha:
        sys.exit("%s är inte den ROM som mallen gäller (sha256 %s)" % (sys.argv[2], sha))
    ut = [fyll_rad(r, rom, org, links) for r in rader if not r.startswith("%% ")]
    sys.stdout.write("\n".join(ut))


if __name__ == "__main__":
    main()
