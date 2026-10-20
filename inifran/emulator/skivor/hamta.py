#!/usr/bin/env python3
"""Hämta Luxors skivor till FD2:
    python3 skivor/hamta.py      (eller make skivor; på Windows: py skivor\\hamta.py)

Skivorna, Systemskiva ver. 1.0 för ABC80 och demoskivan Demo
flexskivminne ABC (art.nr 68 00021-31), ligger inte i förrådet. De
hämtas som Teledisk-avbilder (system10.td0, demo.td0) från ABC-klubbens
samling på devili.iki.fi, görs om till FD2-avbilder om 160 KB
(skivor/systemskiva.dsk, skivor/demoskiva.dsk) och kontrolleras mot sina
SHA-256. En fil som redan finns och stämmer hämtas inte igen.

Avbilden har sektorerna i logisk följd, som emulatorns kort 45 vill ha
dem: 40 spår à 16 sektorer à 256 byte. På skivan har sektorerna numren
1-16, och styrprogrammet lägger logisk sektor L på sektor (7L mod 16) + 1
(band 2, kapitlet Skivminnet).

    ./abc80 -r roms/abc80new.rom -l roms/abcdos80.rom@6000 -D skivor/systemskiva.dsk@45
    ./abc80 -r roms/abc80new.rom -l roms/abcdos80.rom@6000 -D skivor/demoskiva.dsk@45 \\
            -k '\\w1500\\.RUN DR0:START\\r'
"""
import hashlib, os, struct, sys, urllib.request

ARKIV = "https://www.devili.iki.fi/pub/Luxor/software/ABC80/"

# fil: (Teledisk-avbilden, dess SHA-256, FD2-avbildens SHA-256)
SKIVOR = {
    "systemskiva.dsk": ("system10.td0",
                        "38d83782c41abcbf1815413ec71ca3e26ef2449bd1a80b5e512dff46a534f397",
                        "64ad961a0f3adf3e68b8421e12078fd95302462cef4a5882da6b4d89bc21ee9d"),
    "demoskiva.dsk": ("demo.td0",
                      "0c818c59056701ecdde0f569eec04a8ab537a6df49d399cf62942893a5527bb2",
                      "980f3d6c0df21cc7940bd46ba3d840ddad62c1669bf53ba4fcc66ffcc63830ef"),
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def sektorer(d):
    """Sektorerna i en okomprimerad Teledisk-avbild: {(spår, sida, nummer): data}.
    Spåret och sidan är spårhuvudets, där sektorn låg; sektorns eget
    huvud kan ha bit 7 satt i sidan (så i demo.td0)."""
    if d[:2] != b"TD":
        raise ValueError("inte en okomprimerad Teledisk-avbild")
    p = 12
    if d[7] & 0x80:                       # kommentarsblocket
        p += 10 + struct.unpack_from("<H", d, p + 2)[0]
    sekt = {}
    while d[p] != 0xFF:
        antal, spar, sida = d[p:p + 3]
        p += 4
        for _ in range(antal):
            _spar, _sida, nr, _storlek, flaggor = d[p:p + 5]
            p += 6
            data = b""
            if not flaggor & 0x30:
                langd = struct.unpack_from("<H", d, p)[0]
                kod, blk = d[p + 2], d[p + 3:p + 2 + langd]
                p += 2 + langd
                if kod == 0:              # som den är
                    data = blk
                elif kod == 1:            # ett par upprepat
                    data = blk[2:4] * struct.unpack_from("<H", blk)[0]
                else:                     # block, rakt av eller upprepade
                    q = 0
                    while q < len(blk):
                        if blk[q] == 0:
                            n = blk[q + 1]
                            data += blk[q + 2:q + 2 + n]
                        else:
                            n = 1 << blk[q]
                            data += blk[q + 2:q + 2 + n] * blk[q + 1]
                        q += 2 + n
            sekt[(spar, sida, nr)] = data
    return sekt


def fd2(td0):
    sekt = sektorer(td0)
    return b"".join(sekt[(spar, 0, (7 * l) % 16 + 1)]
                    for spar in range(40) for l in range(16))


def hamta(mal, td0_namn, td0_summa, dsk_summa):
    """Hämta en skiva; sant om den finns och stämmer efteråt."""
    namn = os.path.basename(mal)
    if os.path.exists(mal):
        with open(mal, "rb") as f:
            if sha(f.read()) == dsk_summa:
                return True
    try:
        print("hämtar " + ARKIV + td0_namn, flush=True)
        with urllib.request.urlopen(ARKIV + td0_namn, timeout=60) as r:
            td0 = r.read()
    except OSError as e:
        print("%s: kunde inte hämtas (%s)" % (namn, e), file=sys.stderr)
        return False
    if sha(td0) != td0_summa:
        print("%s: fel kontrollsumma, sparas inte" % td0_namn, file=sys.stderr)
        return False
    dsk = fd2(td0)
    if sha(dsk) != dsk_summa:
        print("%s: fel kontrollsumma efter omvandlingen, sparas inte" % namn,
              file=sys.stderr)
        return False
    with open(mal, "wb") as f:
        f.write(dsk)
    print("%s: %d byte" % (namn, len(dsk)))
    return True


def main():
    har = os.path.dirname(os.path.abspath(__file__))
    fel = 0
    for namn, (td0_namn, td0_summa, dsk_summa) in SKIVOR.items():
        if not hamta(os.path.join(har, namn), td0_namn, td0_summa, dsk_summa):
            fel += 1
    sys.exit(1 if fel else 0)


if __name__ == "__main__":
    main()
