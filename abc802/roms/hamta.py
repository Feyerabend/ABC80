#!/usr/bin/env python3
"""Hämta ROM:arna som ABC 802-emulatorn och dess webbsida byggs med:
    python3 roms/hamta.py        (från abc802/; på Windows: py roms\\hamta.py)

ROM:arna ligger inte i förrådet. De hämtas från abc80.net:s arkiv, sätts
ihop och kontrolleras mot sina SHA-256. En fil som redan finns och stämmer
hämtas inte igen. Samma sak med Dataindustriers L.BAS till nätet, från
ABC-klubbens arkiv; webbsidan har den som L.BAC i UFD:.
"""
import hashlib, os, sys, urllib.request

ARKIV = "https://www.abc80.net/archive/luxor/Prom/fw/ABC802/"
LUXNET = "https://www.devili.iki.fi/pub/Luxor/software/ABC-klubben/abc800/luxnet/"

# fil: (källor efter varandra, SHA-256)
ROMAR = {
    # BASIC II i tre kretsar à 8 KB och DOS v.20 (32-31, 1984-04-03)
    "abc802rom.rom": ([ARKIV + n for n in ("ABC802-basic.02-11.bin",
                                           "ABC802-basic.12-11.bin",
                                           "ABC802-basic.22-11.bin",
                                           "ABC802-dos.32-31.bin")],
                      "5467d443150f2a6546a133293dd37d0d5c29e211722e94e38df129843fc21253"),
    # BASIC II med nätets DOS-PROM i stället för DOS:en (LUXNET, "ABC 32-XX")
    "abc802net.rom": ([ARKIV + n for n in ("ABC802-basic.02-11.bin",
                                           "ABC802-basic.12-11.bin",
                                           "ABC802-basic.22-11.bin",
                                           "ABC802-dos-LUXNET-322n02.bin")],
                      "8938ba4547e6a190dccdc15f6d13a6dd0068a4b6d4a5048a16c9e10f364d3b78"),
    # teckengeneratorn 6490191-01
    "orig/abc802/ABC802-char.6490191-01.bin": ([ARKIV + "ABC802-char.6490191-01.bin"],
                      "360fbd39fc05a13996ea96f385ac366cae1ab9dc631a06a2e00a16f8b43427f1"),
    # L.BAS, "Library listing" X.19 (Dataindustrier AB 1985), som text
    "luxnet/l.bas": ([LUXNET + "l.bas"],
                      "0dd28541a5d662e462e43413a6b77259876358a796f1c8b2b512b99a930a1e58"),
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
        os.makedirs(os.path.dirname(mal), exist_ok=True)
        with open(mal, "wb") as f:
            f.write(data)
        print("%s: %d byte" % (namn, len(data)))
    sys.exit(1 if fel else 0)


if __name__ == "__main__":
    main()
