#!/usr/bin/env python3
"""lappa.py -- utanför boken: bygger lappade kopior av ROM:arna.

  python3 lappa.py a|b namn lapp ...     (a = ABC80, gamla ROM:en; b = BASIC II)

Varje lapp är lapp/<lapp>.asm, assemblerad med z80asm; bytena från dess
ORG läggs in i ROM-kopian (under $4000 eller i BASIC II:s lucka, LUCKA)
eller i kretsen på $5000 (ABC80). Anrop som lappen behöver i ROM:en står
i LAPPAR nedan. Skriver ut/<namn>.rom och för ABC80 ut/<namn>.krets.
"""
import os, re, subprocess, sys

HÄR = os.path.dirname(os.path.abspath(__file__))
Z80ASM = os.path.join(HÄR, "../../dis/build/z80asm")
ROM = {"a": "../../emulator/roms/abc80old.rom", "b": "../../emulator/roms/basicii80.rom"}

# Kretsen (ABC80) ligger på $5000. ROM:en har ett fel som hoppar ut i
# kretsarnas område: när CMPARG går fel i CMPDMI ($1BD5, t.ex. "DIM A"
# utan index eller "DIM A(1,2,3)") hoppar den till CMPERR med AF från
# $1BD1 kvar på stacken; POP BC tar AF och RET hoppar till adressen i BC,
# VARNAM:s resultat: bokstaven ($41-$5D, A-Z och ÄÖÅ) som hög byte och
# typen (0-2) plus siffran (1-9, $F utan siffra) gånger 16 som låg. På en
# maskin utan krets där läses $FF (RST $38), och provet ger fel 4. Bytena
# på de adresserna måste alltså vara $FF även i kretsen; lapparna står i
# luckorna $x93-$xEF och tabellerna på $5E00 och $5F00 (ingen bokstav).
KRETS = 0x5000
DIMFEL = [b << 8 | s << 4 | t for b in range(0x41, 0x5E)
          for s in list(range(1, 10)) + [0xF] for t in range(3)]

# BASIC II: lapparna ligger i nollorna $044A-$06A9, där ABC800 M har
# kassetten CAS: (borttagen i ABC80-versionen, ingenting läser där).
# $7800-$7FFF är $FF i ROM-filen men inte ledigt i maskinen: $7800-$7BFF
# är RAM för skivenheternas DOS (enhetsposter och buffertar, i abc80host
# med -F) och $7C00- är skärmen.
LUCKA = (0x044A, 0x06AA)

# byten i ROM:en som pekar ut till lappens rutiner
LAPPAR = {
    "a_brk": {0x0D92: "3A 07 FE A7 C2 93 50 00"},   # LD A,(BRKFLG) / AND A / JP NZ,BRKSTB / NOP
    "a_tab": {},                                    # hoppet står i a_tab.asm
    "a_var": {0x289F: "93 51 93 51 93 51"},        # EXPTAB1: VAR0 -> VAR0X
    "a_sup": {0x289F: "93 51 93 51 93 51"},        # som a_var, VAR0X med V=V+X
    "a_madd": {0x3585: "4F CD 93 53 79 18 EE"},     # LD C,A / CALL MADDX / LD A,C / JR L357A
    "a_fswp": {0x34E5: "C3 93 54 00"},              # JP FSWP (FMUL efter PUSH BC)
    "a_ftab": {0x351B: "C3 93 57"},                 # JP TABX
    "a_fdiv": {0x3628: "C3 93 55"},                 # JP DSUB
    "b_mul": {0x2369: "CD 50 04"},                  # CALL MULSNB i stället för MUL16
    "b_nxt": {0x2809: "C3 70 04"},                  # NEXT: JP NEXTX
    "b_sup": {0x1D1A: "D4 D0 04"},                  # satsens första kod: CALL NC,STMX
}

def asm(namn):
    """[(adress, bytes)] för varje rad med kod i lappens listning"""
    src = os.path.join(HÄR, "lapp", namn + ".asm")
    ut = os.path.join(HÄR, "ut", namn + ".bin")
    lst = subprocess.run([Z80ASM, "-l", "-o", ut, src], check=True,
                         capture_output=True, text=True).stdout
    delar = []
    for rad in lst.splitlines():
        m = re.match(r"([0-9A-F]{4})  ((?:[0-9A-F]{2} )+)", rad)
        if m:
            delar.append((int(m.group(1), 16), bytes.fromhex(m.group(2))))
    return delar

def main():
    vilken, namn, lappar = sys.argv[1], sys.argv[2], sys.argv[3:]
    rom = bytearray(open(os.path.join(HÄR, ROM[vilken]), "rb").read())
    krets = bytearray(b"\xFF" * 0x1000)
    def lägg(adr, data):
        if vilken == "a" and KRETS <= adr < KRETS + 0x1000:
            krets[adr - KRETS:adr - KRETS + len(data)] = data
        else:
            rom[adr:adr + len(data)] = data
    upptaget = set()
    for l in lappar:
        for org, data in asm(l):
            if vilken == "b":
                fel = [a for a in range(org, org + len(data))
                       if not LUCKA[0] <= a < LUCKA[1] or a in upptaget]
                if fel:
                    sys.exit("lappa.py: %s har kod på $%04X, utanför luckan "
                             "eller på en annan lapp" % (l, fel[0]))
                upptaget.update(range(org, org + len(data)))
            lägg(org, data)
        for adr, hx in LAPPAR[l].items():
            lägg(adr, bytes.fromhex(hx))
    if "a_tab" in lappar:
        # EXPTAB1 som ROM:en läser den, med de andra lapparnas ändringar:
        # index i = koden - $B6 (mod 256), adressen $288B + 2i, för alla i
        for i in range(256):
            a = (0x288B + 2 * i) & 0xFFFF
            rutin = rom[a:a + 2]                  # a < $4000 för alla i
            lägg(0x5E00 + i, rutin[:1])
            lägg(0x5F00 + i, rutin[1:])
    os.makedirs(os.path.join(HÄR, "ut"), exist_ok=True)
    open(os.path.join(HÄR, "ut", namn + ".rom"), "wb").write(rom)
    if vilken == "a":
        fel = [f"${a:04X}" for a in DIMFEL
               if KRETS <= a < KRETS + 0x1000 and krets[a - KRETS] != 0xFF]
        if fel:
            sys.exit("lappa.py: kretsen har kod där DIM-felet hoppar: " + " ".join(fel))
        open(os.path.join(HÄR, "ut", namn + ".krets"), "wb").write(krets)

main()
