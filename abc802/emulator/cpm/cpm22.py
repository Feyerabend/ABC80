"""cpm22.py -- CCP och BDOS till vår assembler, till boken ABC80 inifrån

    python3 cpm22.py dr/CPM22.Z80 cpm22.asm

dr/CPM22.Z80 är Clark A. Calkins återskapade källa till CP/M 2.2 i
Z80-koder (cpm2-asm.zip i den fria utgåvan på cpm.z80.de). Här ändras
bara det som z80asm (verktyg/) läser på ett annat sätt, och minnet:
    - strängar i DEFB står inom "", inte '' (ett tecken: 'x' som förut)
    - ADD A,M och SBC A,M (8080:s namn) blir A,(HL)
    - prompten och CCP:ns sex felmeddelanden som i Digital Researchs
      original (CPM.SYS i cpm22-b.zip, samma utgåva): "A>" och
      versaler, "NO FILE" och inte "No file"; Calkins hade gemener
    - MEM blir 63, så att CCP hamnar på $E000, BDOS på $E806 och
      BIOS:en på $F600 (cpm/bios.asm)
Raderna i övrigt och kommentarerna är som i källan.
"""
import re
import sys


def kodens_slut(rad):
    """Var kommentaren börjar: första ; utanför en sträng."""
    inne = False
    for i, c in enumerate(rad):
        if c == "'":
            inne = not inne
        elif c == ";" and not inne:
            return i
    return len(rad)


# CCP:ns meddelanden, med versaler i originalet.
VERSALER = ("RDERR:", "NOFILE:", "YESNO:", "NOSPACE:", "EXISTS:", "BADLOAD:")

ut = []
for rad in open(sys.argv[1], encoding="latin-1").read().splitlines():
    i = kodens_slut(rad)
    kod, kommentar = rad[:i], rad[i:]
    kod = re.sub(r"'([^']{2,})'", r'"\1"', kod)
    kod = re.sub(r"\bA,M\b", "A,(HL)", kod)
    if kod.startswith(VERSALER):
        kod = re.sub(r'"[^"]*"', lambda m: m.group(0).upper(), kod)
    kod = re.sub(r"\bADD(\s+)A,'a'", r"ADD\1A,'A'", kod)       # prompten
    kod = re.sub(r"^MEM(\s+)EQU(\s+)62\b", r"MEM\1EQU\g<2>63", kod)
    ut.append(kod + kommentar)
open(sys.argv[2], "w", encoding="latin-1").write("\n".join(ut) + "\n")
