#!/usr/bin/env python3
"""handelser.py -- utanför boken: räknar händelser i prof:s tabeller.

Antalet instruktioner på en rutins första adress är antalet gånger den
körts. Skriver per prov: tiden, rader, satser och uttryckskoder (och
T-cykler per sats), för ABC80 (gamla ROM:en) och BASIC II.
  python3 handelser.py prov ...
"""
import sys

ABC80 = {"rad": 0x0D71, "sats": 0x0D9E, "kod": 0x2842, "brk": 0x033E}
B2 = {"rad": None, "sats": 0x1D5B, "kod": 0x1DB3, "brk": 0x1D1D}

def las(fil):
    t, n = {}, {}
    for rad in open(fil):
        a, tt, k = rad.split()
        t[int(a, 16)] = int(tt); n[int(a, 16)] = int(k)
    return sum(t.values()), n

print("%-7s | %8s %6s %7s %8s %5s | %8s %7s %8s %5s"
      % ("prov", "ABC80 T", "rader", "satser", "koder", "T/k",
         "BII T", "satser", "koder", "T/k"))
for p in sys.argv[1:]:
    ta, na = las("ut/%s.gamla.txt" % p)
    tb, nb = las("ut/%s.b2.txt" % p)
    ka, kb = na.get(ABC80["kod"], 0), nb.get(B2["kod"], 0)
    print("%-7s | %8.2fM %6d %7d %8d %5d | %8.2fM %7d %8d %5d"
          % (p, ta / 1e6, na.get(ABC80["rad"], 0), na.get(ABC80["sats"], 0), ka,
             ta / max(ka, 1), tb / 1e6, nb.get(B2["sats"], 0), kb, tb / max(kb, 1)))
