#!/usr/bin/env python3
"""Hämta ROM:arna som emulatorn och webbsidan byggs med:
    python3 roms/hamta.py        (eller make roms; på Windows: py roms\\hamta.py)

ROM:arna ligger inte i förrådet. De hämtas från abc80.net:s arkiv och
från abc80sim, sätts ihop och kontrolleras mot sina SHA-256. En fil som
redan finns och stämmer hämtas inte igen.
"""
import hashlib, os, sys, urllib.request

ARKIV = "https://www.abc80.net/archive/luxor/Prom/fw/ABC80/"
ABC80SIM = "https://raw.githubusercontent.com/hpax/abc80sim/master/roms/abc80/"

# fil: (källor efter varandra, SHA-256)
ROMAR = {
    "abc80old.rom": ([ARKIV + "ABC80-ZA350%d-11273.bin" % k for k in (6, 7, 8, 9)],
                     "b1e5e57402c16ea059fc76583e04c72b1ff705ad30cf5382dab240424b534396"),
    "abc80new.rom": ([ARKIV + "ABC80-ZA350%d-9913.bin" % k for k in (6, 7, 8, 9)],
                     "ff1a22a7239b4e394f497a8cb547f218f87a78db3b88607552e37d969a2a1453"),
    "basicii80.rom": ([ABC80SIM + "basicii80.rom"],
                      "101c933117ac32368db70bdd848c7a7da29eb7a54dccfe9a0f6e5e0040742bf6"),
    "abcdos80.rom": ([ARKIV + "ABCDOS80.bin"],
                     "78786d99d80639a9f074c6d28f946d1968526b10f3d7f8b5c7088310d753ea59"),
    "p40.rom": ([ARKIV + "P40.bin"],
                "9f1d79c750668a77880e32891d1c90047663858b54a55e576935dd99273977e9"),
    "tkn80-iii.rom": ([ARKIV + "TKN80-III.bin"],
                      "a5cb443873ee5e08251a809c26539b7a2fc0d3c00e3cfeb76aa3ff8bcac655b8"),
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    har = os.path.dirname(os.path.abspath(__file__))
    fel = 0
    for namn, (kallor, summa) in ROMAR.items():
        mal = os.path.join(har, namn)
        if os.path.exists(mal):
            with open(mal, "rb") as f:
                if sha(f.read()) == summa:
                    continue
        data = b""
        try:
            for url in kallor:
                print("hämtar " + url, flush=True)
                with urllib.request.urlopen(url, timeout=60) as r:
                    data += r.read()
        except OSError as e:
            print("%s: kunde inte hämtas (%s)" % (namn, e), file=sys.stderr)
            fel += 1
            continue
        if sha(data) != summa:
            print("%s: fel kontrollsumma, sparas inte" % namn, file=sys.stderr)
            fel += 1
            continue
        with open(mal, "wb") as f:
            f.write(data)
        print("%s: %d byte" % (namn, len(data)))
    sys.exit(1 if fel else 0)


if __name__ == "__main__":
    main()
