#!/usr/bin/env python3
"""profil.py -- utanför boken: summerar prof:s tabell per rutin.

Varje adress räknas till närmaste etikett före den i listningen.
  python3 profil.py listning.lst profil.txt [antal]
"""
import re, sys, bisect

lst, prof = sys.argv[1], sys.argv[2]
n = int(sys.argv[3]) if len(sys.argv) > 3 else 25
lab = {}
for rad in open(lst, encoding="utf-8", errors="replace"):
    m = re.match(r"([0-9A-F]{4})  (?:[0-9A-F]{2} )+\s*([A-Za-z_][A-Za-z0-9_]*):", rad)
    if m:
        if not re.fullmatch(r"[LSJ][0-9A-F]{4}", m.group(2)):
            lab.setdefault(int(m.group(1), 16), m.group(2))
adr = sorted(lab)
# lapparna: kretsen på $5000 i ABC80, luckan $044A-$06A9 i BASIC II
if "basicii" in lst:
    romslut, lapp = 0x7800, (0x044A, 0x06AA)
else:
    romslut, lapp = 0x4000, (0x5000, 0x6000)
summa, tot = {}, 0
for rad in open(prof):
    a, t, k = rad.split()
    a, t, k = int(a, 16), int(t), int(k)
    if lapp[0] <= a < lapp[1]:
        namn = "(lapp)"
    elif a >= romslut:
        namn = "(RAM/skärm)"
    else:
        i = bisect.bisect_right(adr, a) - 1
        namn = lab[adr[i]] if i >= 0 else "?"
    s = summa.setdefault(namn, [0, 0])
    s[0] += t; s[1] += k; tot += t
for namn, (t, k) in sorted(summa.items(), key=lambda x: -x[1][0])[:n]:
    print("%-10s %6.1f %%  %10d T  %8d instr" % (namn, 100 * t / tot, t, k))
