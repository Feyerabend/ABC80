#!/usr/bin/env python3
"""bygg.py -- utanför boken: assemblerar comal.asm och bygger
ut/comal.rom (kopia av abc80old.rom med lapparna under $4000) och
ut/comal.krets (8 KB på $4000-$5FFF). Prövar att kopiorna av tabell 2
i kretsen börjar som ROM:ens.
"""
import os, re, subprocess, sys

HÄR = os.path.dirname(os.path.abspath(__file__))
Z80ASM = os.path.join(HÄR, "../../dis/build/z80asm")
ROM = os.path.join(HÄR, "../../emulator/roms/abc80old.rom")
KRETS = 0x4000

def main():
    os.makedirs(os.path.join(HÄR, "ut"), exist_ok=True)
    r = subprocess.run([Z80ASM, "-l", "-o", os.path.join(HÄR, "ut", "comal.bin"),
                        os.path.join(HÄR, "comal.asm")], capture_output=True, text=True)
    open(os.path.join(HÄR, "ut", "comal.lst"), "w").write(r.stdout)
    if r.returncode:
        sys.exit(r.stdout + r.stderr)
    rom = bytearray(open(ROM, "rb").read())
    orig = bytes(rom)
    krets = bytearray(b"\xFF" * 0x2000)
    sym = {}
    for rad in r.stdout.splitlines():
        m = re.match(r"([0-9A-F]{4})  ((?:[0-9A-F]{2} )+)", rad)
        if m:
            adr, data = int(m.group(1), 16), bytes.fromhex(m.group(2))
            if KRETS <= adr < KRETS + 0x2000:
                pass                        # ur bin-filen nedan
            elif adr + len(data) <= 0x4000:
                rom[adr:adr + len(data)] = data
            else:
                sys.exit("bygg.py: kod på $%04X" % adr)
        m = re.match(r"([0-9A-F]{4})  [0-9A-F ]*\s+([A-Z0-9_]+):", rad)
        if m:
            sym[m.group(2)] = int(m.group(1), 16)
    # listningen visar högst 8 bytes per rad; kretsen tas ur bin-filen,
    # som är minnet från adress 0
    bild = open(os.path.join(HÄR, "ut", "comal.bin"), "rb").read()[KRETS:KRETS + 0x2000]
    krets[:len(bild)] = bild
    k = lambda a: krets[a - KRETS:]
    # tabell 2 ur ROM:en: STKW2 $199C-$1A06, STTAB2 $1A36, STEXE2 $0E42
    if bytes(k(sym["TAB2"])[:44]) != orig[0x1A36:0x1A62] or \
       bytes(k(sym["EXE2"])[:44]) != orig[0x0E42:0x0E6E]:
        sys.exit("bygg.py: TAB2 eller EXE2 skiljer från ROM:en")
    kw = bytes(k(sym["KW2"])[:0x200])
    gamla = orig[0x199C:0x1A07]
    if gamla not in kw:
        sys.exit("bygg.py: KW2 har inte STKW2")
    open(os.path.join(HÄR, "ut", "comal.rom"), "wb").write(rom)
    open(os.path.join(HÄR, "ut", "comal.krets"), "wb").write(krets)
    slut = max(a for a in range(0x2000) if krets[a] != 0xFF) + 1
    ändrat = sum(1 for a, b in zip(rom, orig) if a != b)
    print("kretsen %d bytes ($4000-$%04X), %d bytes ändrade i ROM:en"
          % (slut, KRETS + slut - 1, ändrat))

main()
