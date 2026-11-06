#!/usr/bin/env python3
"""skiva.py -- CP/M-skivorna till ABC 802, till ABC 802 inifrån

    python3 skiva.py start.bin cpm22.bin bios.bin a.img [fil ...]
    python3 skiva.py - - - b.img [fil ...]          (utan system)

En avbild till kort 44 om 320 KB (1 280 sektorer à 256 bytes), som DOS
v.20 i ROM:en startar och cpm/bios.asm läser. CP/M ser 80 spår om 16
sektorer (32 sektorer om 128 bytes):

  sektor 0-39    DOS:ens del, ur ../../prov/skivor/tom.img: etiketten,
                 bitkartan (sektor 14), katalogen (sektor 16-23),
                 SYSDIR.SYS, och BASICINI.SYS med laddaren (start.bin)
                 som post 0. Bitkartan säger att alla kluster är
                 upptagna, så att DOS:en aldrig skriver i CP/M:s del.
  sektor 40-71   CCP och BDOS ($1600 bytes ur cpm22.bin, från $E000) och
                 BIOS:en (bios.bin, från $F600), som laddaren läser
  sektor 80-     spår 5-79, CP/M:s del: 150 block om 2 KB; katalogen
                 (128 poster) i block 0-1, filerna från block 2

BASICINI.SYS skrivs av DOS:en själv: emulatorn kör ett BASIC-program som
skriver posten med PUT, som gor.py i prov/skivor gör med Luxors
systemprogram (dess kor(), koder() och SKRIVARE används här). Utan
system blir hela skivan $E5, med samma CP/M-del.

Filerna läggs i katalogen för användare 0, med namnet i versaler. Med
block om 2 KB och blocknumren i en byte rymmer en katalogpost 16 block,
32 KB, alltså två logiska extent (EXM = 1): EX är numret på postens
sista extent och RC antalet poster om 128 bytes i den.
"""
import os
import shutil
import sys
import tempfile

HAR = os.path.dirname(os.path.abspath(__file__))
SKIVOR = os.path.join(HAR, "..", "..", "prov", "skivor")
sys.path.insert(0, SKIVOR)
import gor  # noqa: E402

SEKTOR, SEKTORER = 256, 1280
SYSSEKTOR, SYSANTAL = 40, 32                  # som start.asm
SYSTEMSPAR, SPARSEKTORER = 5, 16
BLOCK, BLOCK_ANTAL, KATALOGPOSTER = 2048, 150, 128
KATALOGBLOCK = KATALOGPOSTER * 32 // BLOCK    # 2
BITKARTA = 14


def basicini(avbild, laddare):
    """BASICINI.SYS med laddaren som post 0, skriven av DOS:en."""
    post = laddare.ljust(253, b"\0")
    rader = list(gor.SKRIVARE) + ['1000 DATA "BASICINI.SYS"']
    nr, rad = 1010, ""
    for kod in gor.koder(post) + [None]:
        if kod is None or len(rad) + len(kod) > 60:
            rader.append('%d DATA "%s"' % (nr, rad))
            nr += 10
            rad = ""
        if kod:
            rad += kod
    rader += ['%d DATA "."' % nr, '%d DATA "."' % (nr + 10)]
    with tempfile.NamedTemporaryFile("w", suffix=".BAS") as program:
        program.write("".join(r + "\n" for r in rader))
        program.flush()
        gor.kor(avbild, "RUN\\r\\w20000", program.name)
    data = open(avbild, "rb").read()
    if gor.filens_data(data, post[:32])[:253] != post:
        sys.exit("skiva.py: BASICINI.SYS blev fel på %s" % avbild)
    if gor.filer(data) != ["SYSDIR.SYS", "BASICINI.SYS"]:
        sys.exit("skiva.py: katalogen på %s: %s" % (avbild, gor.filer(data)))


def main():
    start, cpm22, bios, ut = sys.argv[1:5]
    if start != "-":
        shutil.copyfile(os.path.join(SKIVOR, "tom.img"), ut)
        basicini(ut, open(start, "rb").read())
        skiva = bytearray(open(ut, "rb").read())
        assert len(skiva) == SEKTORER * SEKTOR
        omrade = skiva[SYSSEKTOR * SEKTOR:SYSTEMSPAR * SPARSEKTORER * SEKTOR]
        if omrade != b"\xE5" * len(omrade):
            sys.exit("skiva.py: DOS:en har skrivit i sektor %d-%d" %
                     (SYSSEKTOR, SYSTEMSPAR * SPARSEKTORER - 1))
        karta = SEKTORER // 4 // 8            # 320 kluster, 40 bytes
        skiva[BITKARTA * SEKTOR:BITKARTA * SEKTOR + karta] = b"\xFF" * karta
        system = open(cpm22, "rb").read()[:0x1600] + open(bios, "rb").read()
        assert len(open(cpm22, "rb").read()) >= 0x1600, "cpm22.bin ska räcka till $F600"
        assert len(system) <= SYSANTAL * SEKTOR, "systemet får inte plats i sektor 40-71"
        skiva[SYSSEKTOR * SEKTOR:SYSSEKTOR * SEKTOR + len(system)] = system
    else:
        skiva = bytearray(b"\xE5" * (SEKTORER * SEKTOR))

    data = SYSTEMSPAR * SPARSEKTORER * SEKTOR  # block 0
    skiva[data:] = b"\xE5" * (len(skiva) - data)
    post, block = 0, KATALOGBLOCK
    for fil in sys.argv[5:]:
        innehall = open(fil, "rb").read()
        if len(innehall) % 128:
            innehall += b"\x1A" * (128 - len(innehall) % 128)
        namn, _, typ = os.path.basename(fil).upper().partition(".")
        namn = namn[:8].ljust(8).encode("ascii")
        typ = typ[:3].ljust(3).encode("ascii")
        delar = [innehall[k:k + 16 * BLOCK] for k in range(0, len(innehall), 16 * BLOCK)] or [b""]
        for nummer, del_ in enumerate(delar):
            if post == KATALOGPOSTER:
                sys.exit("skiva.py: katalogen är full")
            blocken = []
            for k in range(0, len(del_), BLOCK):
                if block == BLOCK_ANTAL:
                    sys.exit("skiva.py: skivan är full")
                bit = del_[k:k + BLOCK]
                skiva[data + block * BLOCK:data + block * BLOCK + len(bit)] = bit
                blocken.append(block)
                block += 1
            rader = len(del_) // 128          # 0-256
            sista = max(rader - 1, 0) // 128  # postens sista extent, 0 eller 1
            ex = 2 * nummer + sista
            rc = rader - 128 * sista
            katalogpost = (bytes([0]) + namn + typ + bytes([ex, 0, 0, rc])
                           + bytes(blocken).ljust(16, b"\0"))
            skiva[data + post * 32:data + post * 32 + 32] = katalogpost
            post += 1

    open(ut, "wb").write(skiva)


if __name__ == "__main__":
    main()
