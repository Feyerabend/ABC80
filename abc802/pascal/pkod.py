"""pkod.py -- p-kodens instruktioner, gemensamma för pasm.py, pvm.py och
kompilatorn (se PASCAL.md, avsnitt 4).

Varje instruktion har ett namn, en opkod och sina operander:
  b  en byte utan tecken (0..255)
  s  en byte med tecken, ett relativt hopp (-128..127)
  w  ett ord, lägsta byte först
  r  ett ord med tecken, ett relativt hopp
  n  en byte som är en längd, följd av så många bytes (LSA)
  x  XJP: lo och hi som ord och sedan hi-lo+1 relativa ord
Ett relativt hopp räknas från adressen efter instruktionen (för XJP
efter hela tabellen).
"""

TABELL = []          # (namn, opkod, operander)


def _def(namn, kod, op=""):
    TABELL.append((namn, kod, op))


for i in range(16):
    _def("LDC%d" % i, 0x00 + i)          # konstanten i
_def("LDCN", 0x10)                       # -1
_def("LDCB", 0x11, "b")                  # konstant 0..255
_def("LDCW", 0x12, "w")                  # konstant
_def("LSA", 0x13, "n")                   # strängens adress (strängen i koden)

for i in range(8):
    _def("LDL%d" % i, 0x18 + i)          # lokalt ord i
for i in range(8):
    _def("STL%d" % i, 0x20 + i)          # lagra lokalt ord i
_def("LDL", 0x28, "b")
_def("STL", 0x29, "b")
_def("LLA", 0x2A, "b")                   # lokal adress

for i in range(8):
    _def("LDG%d" % i, 0x30 + i)          # globalt ord i
_def("LDG", 0x38, "w")
_def("STG", 0x39, "w")
_def("LGA", 0x3A, "w")                   # global adress

_def("LOD", 0x3C, "bb")                  # nivåer ut, ord
_def("STR", 0x3D, "bb")
_def("LDA", 0x3E, "bb")

_def("IND", 0x40)                        # adr -- ord
_def("STO", 0x41)                        # adr ord --
_def("INDB", 0x42)                       # adr -- byte
_def("STB", 0x43)                        # adr byte --
_def("MOV", 0x44, "w")                   # till från -- (w bytes)
_def("CPS", 0x45, "w")                   # a1 a2 -- -1/0/1 (w bytes, strängar)

_def("IXA", 0x48, "w")                   # adr i -- adr+i*w
_def("CHK", 0x49, "ww")                  # i -- i, fel om utanför lo..hi

_def("ADD", 0x50)
_def("SUB", 0x51)
_def("MUL", 0x52)
_def("DIV", 0x53)
_def("MOD", 0x54)
_def("NEG", 0x55)
_def("ABS", 0x56)
_def("INC1", 0x57)
_def("DEC1", 0x58)
_def("SQRI", 0x59)

_def("EQU", 0x60)
_def("NEQ", 0x61)
_def("LES", 0x62)
_def("LEQ", 0x63)
_def("GRT", 0x64)
_def("GEQ", 0x65)

_def("AND", 0x68)
_def("OR", 0x69)
_def("NOT", 0x6A)
_def("ODD", 0x6B)

_def("UJPS", 0x70, "s")
_def("FJPS", 0x71, "s")
_def("UJP", 0x72, "r")
_def("FJP", 0x73, "r")
_def("XJP", 0x74, "x")

_def("MST", 0x78, "b")                   # märket: statisk länk n nivåer ut
_def("CUP", 0x79, "bb")                  # argumentens bytes, procedur
_def("ENT", 0x7A, "b")                   # b lokala ord, nollställda
_def("ENTW", 0x7B, "w")
_def("RET", 0x7C)
_def("LFA", 0x7D)                        # funktionsvärdets adress
_def("CXP", 0x7E, "bb")                  # stora versionen; fel här

_def("CSP", 0x80, "b")                   # standardprocedur

_def("DUP", 0x84)
_def("POP", 0x85)

_def("ERR", 0x8E, "b")
_def("HLT", 0x8F)

# Mängderna (bara i den stora versionen, PASCAL.md avsnitt 9): SET OF
# med elementen 0..255, 16 ord (32 bytes) på stacken och i minnet, där
# element i är bit i&7 i byte i>>3. m är en mängd, i ett ord.
_def("LDSE", 0x90)                       # -- m  (den tomma)
_def("SGS", 0x91)                        # i -- m  ([i]; körfel 1 utanför)
_def("SRS", 0x92)                        # lo hi -- m  ([lo..hi])
_def("UNI", 0x93)                        # m m -- m  (+)
_def("INT", 0x94)                        # m m -- m  (*)
_def("DIF", 0x95)                        # m m -- m  (-)
_def("INN", 0x96)                        # i m -- boolesk  (IN)
_def("EQS", 0x97)                        # m m -- boolesk
_def("NES", 0x98)
_def("LSS", 0x99)                        # m1 <= m2: m1 ingår i m2
_def("GSS", 0x9A)                        # m1 >= m2
_def("INDS", 0x9B)                       # adr -- m
_def("STOS", 0x9C)                       # adr m --

# REAL (avsnitt 4.4): tre ord på stacken och i minnet, bytes L' H' L H C 0
# som i BBC BASIC:s flyttalspaket FPP (mantissan med tecknet i H:s högsta
# bit, exponenten i C). Alla går till FPP.BIN; r är ett REAL och i ett ord.
_def("LDCR", 0xA0, "n")                  # -- r, talet som text (VAL)
_def("INDR", 0xA1)                       # adr -- r
_def("STOR", 0xA2)                       # adr r --
_def("FLT", 0xA3)                        # i -- r
_def("FLO", 0xA4)                        # i r -- r r
_def("TRC", 0xA5)                        # r -- i  (TRUNC)
_def("RND", 0xA6)                        # r -- i  (ROUND)
_def("ADR", 0xA7)                        # r r -- r
_def("SBR", 0xA8)
_def("MPR", 0xA9)
_def("DVR", 0xAA)
_def("NGR", 0xAB)                        # r -- r
_def("ABR", 0xAC)
_def("SQRR", 0xAD)
_def("EQR", 0xAE)                        # r r -- boolesk
_def("NER", 0xAF)
_def("LSR", 0xB0)
_def("LER", 0xB1)
_def("GTR", 0xB2)
_def("GER", 0xB3)
_def("SQT", 0xB4)                        # r -- r  (SQRT)
_def("SIN", 0xB5)
_def("COS", 0xB6)
_def("ATN", 0xB7)                        # ARCTAN
_def("LNR", 0xB8)                        # LN
_def("EXR", 0xB9)                        # EXP
_def("WRR", 0xBA)                        # f r b d --  (d < 0: utan decimaler)
_def("RDR", 0xBB)                        # f -- r

PER_NAMN = {namn: (kod, op) for namn, kod, op in TABELL}
PER_KOD = {kod: (namn, op) for namn, kod, op in TABELL}
assert len(PER_KOD) == len(TABELL)

# Standardprocedurerna (CSP). Stacken före -- efter; f är filen (ett
# kanalnummer, 0 är tangentbordet och skärmen), b är fältbredden.
CSP = {
    "WRC": 0,    # f tecken b --  (byten oförändrad, också 10 och 13)
    "WRI": 1,    # f tal b --
    "WRS": 2,    # f adr längd b --
    "WRB": 3,    # f boolesk b --
    "WLN": 4,    # f --
    "RDC": 5,    # f -- tecken
    "RDI": 6,    # f -- tal
    "RLN": 7,    # f --
    "EOL": 8,    # f -- boolesk
    "EOF": 9,    # f -- boolesk
    "RST": 10,   # filvariabelns adr namnets adr längd --
    "RWR": 11,   # filvariabelns adr namnets adr längd --
    "CLO": 12,   # f --
    "RSV": 13,   # filvariabelns adr fältets adr längd -- (namnet i ett
    "RWV": 14,   #   ARRAY OF CHAR, ett tecken per ord, blanksteg sist)
    # bara i den stora versionen (avsnitt 9): högen
    "NEW": 15,   # pekarens adr bytes --  (nollställda, körfel 9 om fullt)
    "MRK": 16,   # pekarens adr --  (MARK: högens topp till pekaren)
    "REL": 17,   # pekare --  (RELEASE: högen tillbaka dit)
}
CSPSTOR = 15     # CSP från 15 finns bara i den stora versionen
CSP_NAMN = {v: k for k, v in CSP.items()}

# Körfelen (ERR b och tolkens egna).
FEL = {
    1: "index utanför gränserna",
    2: "division med noll",
    3: "stacken är full",
    4: "CASE utan fall",
    5: "filen",
    6: "okänd instruktion",
    7: "okänd standardprocedur",
    8: "flyttalsfel",
    9: "minnet är fullt",
}


def storlek(kod, minne, adr):
    """Instruktionens storlek i bytes, med opkoden på adr."""
    namn, op = PER_KOD[kod]
    if op == "n":
        return 2 + minne[adr + 1]
    if op == "x":
        lo = minne[adr + 1] | minne[adr + 2] << 8
        hi = minne[adr + 3] | minne[adr + 4] << 8
        return 5 + 2 * (s16(hi) - s16(lo) + 1)
    return 1 + sum(2 if t in "wr" else 1 for t in op)


def s16(v):
    v &= 0xFFFF
    return v - 0x10000 if v & 0x8000 else v


# P-kodfilen: "PK", version och antal segment (4 bytes), koden, och sist
# procedurtabellen (ord: offset från kodens början), globalernas bytes
# (ord) och antal procedurer (ord). Tabellen ligger sist eftersom den
# blir känd sist; kompilatorn kan då skriva koden med en gång. Bit 15 i
# antalet procedurer säger att programmet använder REAL, och då laddar
# tolken FPP.BIN till FPPADR och lägger stacken under det.
# Version 2 är ett program som använder mängderna eller högen och bara
# går i den stora tolken (PVM.COM); den lilla vill ha version 1.
VERSION = 1
VERSION_STOR = 2
REALFLAGGA = 0x8000
FPPADR = 0xDD00     # FPP.BIN, som slutar under TOPP = $ED00
