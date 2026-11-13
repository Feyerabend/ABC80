#!/usr/bin/env python3
"""pc.py -- kompilatorn för delmängden (PASCAL.md, avsnitt 2) i Python.

    python3 pc.py [-l] prog.pas prog.pk

-l skriver en lista över p-koden, procedur för procedur. pc.py är
förlagan till pc.pas och ska ge samma bytes; därför är den skriven som
kompilatorn i Pascal kommer att vara: ett pass, rekursiv nedstigning,
koden direkt utan träd och utan optimering utöver valet av den kortaste
formen av en instruktion.

Källan läses byte för byte (latin-1), som på ABC 802.
"""
import struct
import sys

from pkod import (CSP, CSPSTOR, PER_KOD, PER_NAMN, REALFLAGGA, VERSION,
                  VERSION_STOR, s16, storlek)

# IN, NIL, RECORD, SET och WITH hör till den stora versionen (PASCAL.md,
# avsnitt 9); pc.pas har dem inte, men inget av dess program använder dem.
ORD = """AND ARRAY BEGIN CASE CONST DIV DO DOWNTO ELSE END FOR FORWARD
FUNCTION IF IN MOD NIL NOT OF OR PACKED PROCEDURE PROGRAM RECORD REPEAT SET
THEN TO TYPE UNTIL VAR WHILE WITH""".split()

FELTEXT = {
    1: "okänt tecken",
    2: "strängen tar inte slut på raden",
    3: "kommentaren tar inte slut",
    4: "talet är för stort",
    5: "här ska stå %s",
    6: "namnet finns redan",
    7: "okänt namn",
    8: "fel typ",
    9: "här ska stå en konstant",
    10: "här ska stå en typ",
    11: "fältets gränser",
    12: "här ska stå en variabel",
    13: "fel antal parametrar",
    14: "här ska stå en sats",
    15: "för många lokala variabler",
    16: "fältet är för stort",
    17: "fallet finns redan",
    18: "CASE har för stort omfång",
    19: "proceduren är inte deklarerad (FORWARD)",
    20: "här ska stå PACKED ARRAY OF CHAR",
    21: "fält som värdeparameter kommer senare",   # bara i pc.pas
    22: "funktionens värde kan bara ges i funktionen själv",
    23: "programmet tar inte slut med END.",
    24: "strängen är längre än 80 tecken",
    25: "namnet är längre än 32 tecken",
    26: "kompilatorns tabeller är fulla",   # bara i pc.pas
    27: "talet har fler än 32 tecken",
    28: "mängdens element ska ligga i 0..255",
}

HELTAL, TECKEN, BOOL, TEXT, REELL = "INTEGER", "CHAR", "BOOLEAN", "TEXT", "REAL"


class Fel(Exception):
    pass


class Post:
    """RECORD: fälten i ordning, falt[namn] = (ordet från början, typ).
    Varje fält börjar på ett helt ord."""

    def __init__(self):
        self.falt, self.ord = {}, 0

    @property
    def bytes(self):
        return 2 * self.ord


class Pekare:
    """^mal; mal är None tills typen är deklarerad (i TYPE)."""

    def __init__(self, mal):
        self.mal = mal

    def __eq__(self, o):
        return isinstance(o, Pekare) and o.mal == self.mal


class Mangd:
    """SET OF elem, alltid 16 ord med elementen 0..255. elem är grundtypen;
    None för den tomma mängden [], som passar till alla."""

    def __init__(self, elem):
        self.elem = elem

    def __eq__(self, o):
        return isinstance(o, Mangd) and o.elem == self.elem


class Upprakning:
    """(a, b, c): konstanterna 0, 1, 2. Två uppräkningar är olika typer."""

    def __init__(self, antal):
        self.antal = antal


class Del:
    """lo..hi av grundtypen bas. Värdet prövas med CHK när det tilldelas."""

    def __init__(self, bas, lo, hi):
        self.bas, self.lo, self.hi = bas, lo, hi


NILTYP = "NIL"


def grund(t):
    return t.bas if isinstance(t, Del) else t


def ordinal(t):
    t = grund(t)
    return t in (HELTAL, TECKEN, BOOL) or isinstance(t, Upprakning)


def block_typ(t):
    """Ett fält eller en post: tilldelas med MOV och laddas som adressen."""
    return isinstance(t, (Falt, Post))


def mangd_par(t, u):
    """Två mängder som passar ihop: mängdtypen, annars None."""
    if not (isinstance(t, Mangd) and isinstance(u, Mangd)):
        return None
    if t.elem is None:
        return u
    if u.elem is None or u.elem == t.elem:
        return t
    return None


class Falt:
    """ARRAY [lo..hi] OF elem: ett ord per element, tre för REAL. PACKED
    ARRAY [lo..hi] OF CHAR (en sträng): en byte per element."""

    def __init__(self, lo, hi, elem, packad=False):
        self.lo, self.hi, self.elem, self.packad = lo, hi, elem, packad
        n = hi - lo + 1
        self.bytes = n if packad else 2 * n * ord_i(elem)
        self.ord = (self.bytes + 1) // 2

    def __eq__(self, o):
        return (isinstance(o, Falt) and (self.lo, self.hi, self.elem,
                self.packad) == (o.lo, o.hi, o.elem, o.packad))


def parord(p):
    """Orden som procedurens parametrar tar; ett fält eller en post som
    värdeparameter är adressen, och proceduren kopierar den."""
    return sum(1 if vp or block_typ(t) else ord_i(t) for t, vp in p.params)


def ord_i(typ):
    if isinstance(typ, (Falt, Post)):
        return typ.ord
    if isinstance(typ, Mangd):
        return 16
    return 3 if typ == REELL else 1


def strang(t):
    return isinstance(t, Falt) and t.packad


class Namn:
    """En post i symboltabellen. slag: konst, typ, var, proc, funk, std,
    stdfil. För var: niva, adr (ordets nummer; för ett fält det första
    ordets) och varpar. För proc och funk: nr, niva (kroppens), params
    [(typ, varpar)], typ (funktionens) och forward."""

    def __init__(self, namn, slag, typ=None, **k):
        self.namn, self.slag, self.typ = namn, slag, typ
        self.__dict__.update(k)


class Kompilator:
    def __init__(self, kalla):
        self.k = kalla
        self.i = 0
        self.rad, self.kol = 1, 0
        self.symrad = self.symkol = 0
        self.namn = []
        for n, v in (("MAXINT", 32767), ("FALSE", 0), ("TRUE", 1)):
            self.namn.append(Namn(n, "konst", BOOL if v < 2 else HELTAL,
                                  varde=v))
        self.namn[0].typ = HELTAL
        for t in (HELTAL, TECKEN, BOOL, TEXT, REELL):
            self.namn.append(Namn(t, "typ", t))
        self.namn.append(Namn("INPUT", "stdfil", TEXT))
        self.namn.append(Namn("OUTPUT", "stdfil", TEXT))
        for n in """WRITE WRITELN READ READLN RESET REWRITE CLOSE ABS SQR ODD
                    ORD CHR SUCC PRED EOF EOLN HALT SQRT SIN COS ARCTAN LN
                    EXP TRUNC ROUND NEW MARK RELEASE""".split():
            self.namn.append(Namn(n, "std"))
        self.procs = 0           # nästa procedurnummer
        self.klara = []          # (nr, kod) i den ordning de blir klara
        self.globala = 0
        self.real = False        # REAL i programmet: FPP.BIN behövs
        self.stor = False        # mängder eller högen: version 2
        self.oppna = None        # pekarna i TYPE som väntar på sin typ
        self.nasta()

    # -- läsaren --------------------------------------------------------

    def fel(self, nr, *arg, var=None):
        """Felet vid den aktuella symbolen; var="läsaren" vid tecknet som
        läses, var="förra" vid symbolen före (typfelen)."""
        t = FELTEXT[nr] % arg if arg else FELTEXT[nr]
        rad, kol = {"läsaren": (self.rad, self.kol),
                    "förra": (self.forrad, self.forkol)}.get(
                        var, (self.symrad, self.symkol))
        raise Fel("rad %d, kolumn %d: fel %d: %s" % (rad, kol + 1, nr, t))

    def tecken(self, j=0):
        return self.k[self.i + j] if self.i + j < len(self.k) else ""

    def fram(self):
        if self.tecken() == "\n":
            self.rad, self.kol = self.rad + 1, 0
        else:
            self.kol += 1
        self.i += 1

    def nasta(self):
        """Nästa symbol till self.sym (och self.val)."""
        while True:
            c = self.tecken()
            if c and c in " \t\r\n\f":
                self.fram()
            elif c == "(" and self.tecken(1) == "*":
                self.fram(), self.fram()
                while not (self.tecken() == "*" and self.tecken(1) == ")"):
                    if self.tecken() == "":
                        self.fel(3, var="läsaren")
                    self.fram()
                self.fram(), self.fram()
            else:
                break
        self.forrad, self.forkol = self.symrad, self.symkol
        self.symrad, self.symkol = self.rad, self.kol
        c = self.tecken()
        if c == "":
            self.sym = "slut"
        elif c.isascii() and c.isalpha():
            s = ""
            while self.tecken().isascii() and self.tecken().isalnum():
                if len(s) == 32:
                    self.fel(25)
                s += self.tecken().upper()
                self.fram()
            if s in ORD:
                self.sym = s
            else:
                self.sym, self.val = "namn", s
        elif c.isdigit():
            # Ett REAL har en punkt med en siffra efter, eller E.
            s = self.siffror()
            if self.tecken() == "." and self.tecken(1).isdigit():
                self.fram()
                s += "." + self.siffror()
            if self.tecken() in ("E", "e"):
                s += "E"
                self.fram()
                if self.tecken() in ("+", "-"):
                    s += self.tecken()
                    self.fram()
                if not self.tecken().isdigit():
                    self.fel(5, "en siffra", var="läsaren")
                s += self.siffror()
            if "." in s or "E" in s:
                if len(s) > 32:
                    self.fel(27)
                self.sym, self.val = "reell", s.encode()
            elif int(s) > 32767:
                self.fel(4)
            else:
                self.sym, self.val = "tal", int(s)
        elif c == "'":
            s = bytearray()
            while True:
                self.fram()
                c = self.tecken()
                if c in ("", "\n"):
                    self.fel(2, var="läsaren")
                if c == "'":
                    self.fram()
                    if self.tecken() != "'":
                        break
                if len(s) == 80:
                    self.fel(24)
                s.append(ord(c))
            if len(s) == 1:
                self.sym, self.val = "tecken", s[0]
            else:
                self.sym, self.val = "strang", bytes(s)
        else:
            tva = c + self.tecken(1)
            if tva in (":=", "<=", ">=", "<>", "..", "(.", ".)"):
                self.fram(), self.fram()
                self.sym = {"(.": "[", ".)": "]"}.get(tva, tva)
            elif c and c in "+-*/=<>()[],;:.^":
                self.fram()
                self.sym = c
            else:
                self.fel(1, var="läsaren")

    def siffror(self):
        s = ""
        while self.tecken().isdigit():
            s += self.tecken()
            self.fram()
        return s

    def vant(self, sym):
        if self.sym != sym:
            self.fel(5, sym)
        self.nasta()

    def namnet(self):
        if self.sym != "namn":
            self.fel(5, "ett namn")
        n = self.val
        self.nasta()
        return n

    # -- symboltabellen -------------------------------------------------

    def sok(self, n):
        p = self.leta(n)
        if p is None:
            self.fel(7, var="förra")
        return p

    def leta(self, n):
        for p in reversed(self.namn):
            if p.namn == n:
                return p
        return None

    def ny(self, p):
        for q in reversed(self.namn[self.scope:]):
            if q.namn == p.namn:
                self.fel(6, var="förra")
        self.namn.append(p)
        return p

    # -- koden ----------------------------------------------------------

    def op(self, namn, *operander):
        kod, typ = PER_NAMN[namn]
        if kod >= 0xA0:
            self.real = True
        elif 0x90 <= kod or (namn == "CSP" and operander[0] >= CSPSTOR):
            self.stor = True
        self.kod.append(kod)
        for t, v in zip(typ, operander):
            if t in "bn":
                self.kod.append(v)
            elif t == "s":
                self.kod += struct.pack("<b", v)
            else:
                self.kod += struct.pack("<H", v & 0xFFFF)

    def ldcr(self, text):
        self.op("LDCR", len(text))
        self.kod += text

    def ldc(self, v):
        v = s16(v)
        if 0 <= v <= 15:
            self.op("LDC%d" % v)
        elif v == -1:
            self.op("LDCN")
        elif 0 <= v <= 255:
            self.op("LDCB", v)
        else:
            self.op("LDCW", v)

    def framat(self, namn):
        """Ett hopp framåt med tomt mål; ger platsen att fylla i."""
        self.op(namn, 0)
        return len(self.kod) - 3

    def fyll(self, plats):
        struct.pack_into("<h", self.kod, plats + 1,
                         len(self.kod) - (plats + 3))

    def bakat(self, namn, mal):
        r = mal - (len(self.kod) + 2)
        if -128 <= r <= 127:
            self.op(namn + "S", r)
        else:
            self.op(namn, mal - (len(self.kod) + 3))

    # -- variablerna ----------------------------------------------------
    #
    # Ett ord i i en post ligger på MP-2(i+1); på nivå 0 är orden globala.
    # Ett fält med k ord som börjar på ord i har sin lägsta adress (sitt
    # element lo) i ord i+k-1 lokalt, och i ord i globalt.

    def bas(self, v):
        """Numret som LLA/LDA/LGA ska ha för variabeln v."""
        if v.niva == 0 or v.varpar:
            return v.adr
        return v.adr + ord_i(v.typ) - 1

    def lad_ord(self, v):
        """Ladda variabelns ord (värdet, eller adressen för en VAR)."""
        d = self.niva - v.niva
        if v.niva == 0:
            if v.adr < 8:
                self.op("LDG%d" % v.adr)
            else:
                self.op("LDG", v.adr)
        elif d == 0:
            if v.adr < 8:
                self.op("LDL%d" % v.adr)
            else:
                self.op("LDL", v.adr)
        else:
            self.op("LOD", d, v.adr)

    def lagra_ord(self, v):
        d = self.niva - v.niva
        if v.niva == 0:
            self.op("STG", v.adr)
        elif d == 0:
            if v.adr < 8:
                self.op("STL%d" % v.adr)
            else:
                self.op("STL", v.adr)
        else:
            self.op("STR", d, v.adr)

    def adressen(self, v):
        """Lägg variabelns adress på stacken."""
        if getattr(v, "wtemp", None) is not None:
            self.lad_ord(v.wtemp)
            self.forskjut(v.off)
            return
        if v.varpar:
            self.lad_ord(v)
            return
        d = self.niva - v.niva
        b = self.bas(v)
        if v.niva == 0:
            self.op("LGA", b)
        elif d == 0:
            self.op("LLA", b)
        else:
            self.op("LDA", d, b)

    def forskjut(self, off):
        """Adressen på stacken off ord fram."""
        if off:
            self.ldc(2 * off)
            self.op("ADD")

    def faltnamn(self, v, namn, off, ft):
        """Fältet off (typ ft) i posten v, som en egen variabel: v är en
        hel variabel som inte är en VAR-parameter."""
        if v.niva == 0:
            a = v.adr + off
        else:
            a = v.adr + ord_i(v.typ) - off - ord_i(ft)
        return Namn(namn, "var", ft, niva=v.niva, adr=a, varpar=False)

    def variabel(self, v):
        """v är redan läst. Ger (slag, typ, v): 'enkel' för en hel
        variabel (ingen kod än), 'adr' när adressen ligger på stacken och
        'badr' för ett tecken i en sträng. Efter namnet kan [i, j], .fält
        och ^ följa i vilken ordning som helst."""
        ref = ("enkel", v.typ, v)
        if getattr(v, "wtemp", None) is not None:
            # ett fält i WITH, med postens adress i ett gömt ord
            self.adressen(v)
            ref = ("adr", v.typ, v)
        while self.sym in ("[", ".", "^"):
            slag, t, v = ref
            if self.sym == "[":
                if not isinstance(t, Falt):
                    self.fel(8)
                self.nasta()
                if slag == "enkel":
                    self.adressen(v)
                while True:
                    f = ref[1]
                    t = self.uttryck()
                    if not ordinal(t):
                        self.fel(8)
                    self.op("CHK", f.lo, f.hi)
                    if f.lo == 1:
                        self.op("DEC1")
                    elif f.lo != 0:
                        self.ldc(f.lo)
                        self.op("SUB")
                    self.op("IXA", 1 if f.packad else 2 * ord_i(f.elem))
                    ref = ("badr" if f.packad else "adr", f.elem, v)
                    if self.sym != ",":
                        break
                    self.nasta()
                    if not isinstance(ref[1], Falt):
                        self.fel(8)
                self.vant("]")
            elif self.sym == ".":
                if not isinstance(t, Post):
                    self.fel(8)
                self.nasta()
                n = self.namnet()
                if n not in t.falt:
                    self.fel(7, var="förra")
                off, ft = t.falt[n]
                if slag == "enkel" and not v.varpar:
                    ref = ("enkel", ft, self.faltnamn(v, n, off, ft))
                else:
                    if slag == "enkel":
                        self.adressen(v)
                    self.forskjut(off)
                    ref = ("adr", ft, v)
            else:
                if not isinstance(t, Pekare):
                    self.fel(8)
                self.nasta()
                if slag == "enkel":
                    self.lad(ref)
                else:
                    self.op("IND")
                ref = ("adr", t.mal, v)
        return ref

    # Ett REAL laddas och lagras genom sin adress (INDR och STOR), en mängd
    # med INDS och STOS, och ett tecken i en sträng med INDB och STB.

    def lad(self, ref):
        slag, typ, v = ref
        if typ == REELL:
            self.ref_adress(ref)
            self.op("INDR")
        elif isinstance(typ, Mangd):
            self.ref_adress(ref)
            self.op("INDS")
        elif slag == "badr":
            self.op("INDB")
        elif slag == "adr":
            self.op("IND")
        elif v.varpar:
            self.lad_ord(v)
            self.op("IND")
        else:
            self.lad_ord(v)

    def fore_lagra(self, ref):
        """Det som ska före värdet när ref ska lagras."""
        slag, typ, v = ref
        if slag == "enkel" and (v.varpar or typ == REELL
                                or isinstance(typ, Mangd)):
            self.adressen(v)

    def lagra(self, ref):
        slag, typ, v = ref
        if typ == REELL:
            self.op("STOR")
        elif isinstance(typ, Mangd):
            self.op("STOS")
        elif slag == "badr":
            self.op("STB")
        elif slag == "adr" or v.varpar:
            self.op("STO")
        else:
            self.lagra_ord(v)

    def ref_adress(self, ref):
        slag, typ, v = ref
        if slag == "enkel":
            self.adressen(v)

    def temp_ord(self):
        """Ett gömt ord i posten (FOR:s gräns, WITH:s adress); tempdjup
        räknas ner när satsen är slut, så att orden används igen."""
        if self.tempdjup == len(self.temp):
            self.temp.append(Namn("", "var", HELTAL, niva=self.niva,
                                  adr=self.lokalt_ord(), varpar=False))
        self.tempdjup += 1
        return self.temp[self.tempdjup - 1]

    def lokalt_ord(self, antal=1):
        """Nya ord i den aktuella posten; ger det första ordets nummer."""
        if self.niva == 0:
            a = self.globala
            self.globala += antal
        else:
            a = self.lokala
            self.lokala += antal
        return a

    # -- deklarationerna ------------------------------------------------

    def program(self):
        self.vant("PROGRAM")
        self.namnet()
        if self.sym == "(":
            self.nasta()
            self.namnet()
            while self.sym == ",":
                self.nasta()
                self.namnet()
            self.vant(")")
        self.vant(";")
        self.niva = 0
        self.scope = len(self.namn)
        huvud = Namn("", "proc", nr=self.nytt_nr(), niva=0, params=[])
        self.block(huvud)
        if self.sym != ".":
            self.fel(23)

    def nytt_nr(self):
        self.procs += 1
        return self.procs - 1

    def konstant(self):
        """Ger (typ, värde); värdet av ett REAL är texten."""
        tecken = 1
        if self.sym in ("+", "-"):
            tecken = -1 if self.sym == "-" else 1
            self.nasta()
            if self.sym == "tal":
                v = self.val
                self.nasta()
                return HELTAL, tecken * v
            if self.sym == "reell":
                v = self.val
                self.nasta()
                return REELL, (b"-" if tecken < 0 else b"") + v
            p = self.sok(self.namnet())
            if p.slag != "konst" or p.typ not in (HELTAL, REELL):
                self.fel(9)
            if p.typ == REELL:
                if tecken > 0:
                    return REELL, p.varde
                if p.varde[:1] == b"-":
                    return REELL, p.varde[1:]
                return REELL, b"-" + p.varde
            return HELTAL, tecken * p.varde
        if self.sym == "reell":
            v = self.val
            self.nasta()
            return REELL, v
        if self.sym == "tal":
            v = self.val
            self.nasta()
            return HELTAL, v
        if self.sym == "tecken":
            v = self.val
            self.nasta()
            return TECKEN, v
        if self.sym == "namn":
            p = self.sok(self.namnet())
            if p.slag != "konst":
                self.fel(9)
            return p.typ, p.varde
        self.fel(9)

    def granser(self, t):
        """Den ordinala typens lo och hi (för ett index och en mängd)."""
        t0 = grund(t)
        if isinstance(t, Del):
            return t.lo, t.hi
        if t0 == BOOL:
            return 0, 1
        if t0 == TECKEN:
            return 0, 255
        if isinstance(t0, Upprakning):
            return 0, t0.antal - 1
        self.fel(11, var="förra")

    def omfang(self):
        """lo..hi med konstanter, eller namnet på en ordinal typ: (typ, lo,
        hi), där typ är grundtypen."""
        if self.sym == "namn":
            p = self.leta(self.val)
            if p is not None and p.slag == "typ":
                self.nasta()
                lo, hi = self.granser(p.typ)
                return grund(p.typ), lo, hi
        lt, lo = self.konstant()
        self.vant("..")
        ht, hi = self.konstant()
        if lt != ht or lt in (TEXT, REELL) or hi < lo:
            self.fel(11)
        return lt, lo, hi

    def typ(self):
        packad = self.sym == "PACKED"
        if packad:
            self.nasta()
            if self.sym not in ("ARRAY", "RECORD", "SET"):
                self.fel(5, "ARRAY")
        if self.sym == "ARRAY":
            self.nasta()
            self.vant("[")
            granser = [self.omfang()[1:]]
            while self.sym == ",":
                self.nasta()
                granser.append(self.omfang()[1:])
            self.vant("]")
            self.vant("OF")
            e = self.typ()
            if e == TEXT:
                self.fel(10)
            # ARRAY [a, b] OF T är ARRAY [a] OF ARRAY [b] OF T; PACKED
            # gäller det innersta.
            for i in range(len(granser) - 1, -1, -1):
                lo, hi = granser[i]
                pk = packad and i == len(granser) - 1
                if pk and e != TECKEN:
                    if not strang(e):
                        self.fel(20, var="förra")
                    pk = False
                if hi - lo + 1 > 16000 // ord_i(e):
                    self.fel(16)
                e = Falt(lo, hi, e, pk)
            return e
        if self.sym == "RECORD":
            self.nasta()
            post = Post()
            while self.sym == "namn":
                namn = [self.namnet()]
                while self.sym == ",":
                    self.nasta()
                    namn.append(self.namnet())
                self.vant(":")
                t = self.typ()
                if t == TEXT:
                    self.fel(10, var="förra")
                for n in namn:
                    if n in post.falt:
                        self.fel(6, var="förra")
                    post.falt[n] = (post.ord, t)
                    post.ord += ord_i(t)
                if post.ord > 16000:
                    self.fel(16)
                if self.sym != ";":
                    break
                self.nasta()
            self.vant("END")
            return post
        if self.sym == "SET":
            self.nasta()
            self.vant("OF")
            t = self.typ()
            if not ordinal(t):
                self.fel(10, var="förra")
            if grund(t) == HELTAL and not isinstance(t, Del):
                self.fel(28, var="förra")
            lo, hi = self.granser(t)
            if lo < 0 or hi > 255:
                self.fel(28, var="förra")
            return Mangd(grund(t))
        if self.sym == "^":
            self.nasta()
            n = self.namnet()
            p = self.leta(n)
            if p is not None and p.slag == "typ":
                return Pekare(p.typ)
            if self.oppna is None:
                self.fel(7, var="förra")
            pk = Pekare(None)
            self.oppna.append((pk, n))
            return pk
        if self.sym == "(":
            # (a, b, c)
            u = Upprakning(0)
            while True:
                self.nasta()
                self.ny(Namn(self.namnet(), "konst", u, varde=u.antal))
                u.antal += 1
                if self.sym != ",":
                    break
            self.vant(")")
            return u
        if self.sym == "namn":
            p = self.leta(self.val)
            if p is not None and p.slag == "typ":
                self.nasta()
                return p.typ
        if self.sym in ("namn", "tal", "tecken", "+", "-"):
            # lo..hi
            t, lo, hi = self.omfang()
            return Del(t, lo, hi)
        self.fel(10)

    def block(self, proc):
        # Ett fält eller en post som värdeparameter: adressen kom som
        # parameter, och kroppen kopierar till ett lokalt ord från början.
        proc.kopior = []
        for q in self.namn[self.scope:]:
            if getattr(q, "kopia", False):
                ptr = Namn("", "var", HELTAL, niva=self.niva, adr=q.adr,
                           varpar=False)
                q.adr, q.varpar, q.kopia = self.lokalt_ord(ord_i(q.typ)), False, False
                proc.kopior.append((q, ptr))
        if self.sym == "CONST":
            self.nasta()
            while True:
                n = self.namnet()
                self.vant("=")
                t, v = self.konstant()
                self.ny(Namn(n, "konst", t, varde=v))
                self.vant(";")
                if self.sym != "namn":
                    break
        if self.sym == "TYPE":
            self.nasta()
            self.oppna = []
            while True:
                n = self.namnet()
                self.vant("=")
                self.ny(Namn(n, "typ", self.typ()))
                self.vant(";")
                if self.sym != "namn":
                    break
            # ^T där T kom senare i TYPE
            for pk, n in self.oppna:
                p = self.leta(n)
                if p is None or p.slag != "typ":
                    self.fel(7, var="förra")
                pk.mal = p.typ
            self.oppna = None
        if self.sym == "VAR":
            self.nasta()
            while True:
                namn = [self.namnet()]
                while self.sym == ",":
                    self.nasta()
                    namn.append(self.namnet())
                self.vant(":")
                t = self.typ()
                for n in namn:
                    a = self.lokalt_ord(ord_i(t))
                    self.ny(Namn(n, "var", t, niva=self.niva, adr=a,
                                 varpar=False))
                self.vant(";")
                if self.sym != "namn":
                    break
        while self.sym in ("PROCEDURE", "FUNCTION"):
            self.procedur()
        self.kropp(proc)

    def procedur(self):
        funk = self.sym == "FUNCTION"
        self.nasta()
        n = self.namnet()
        p = None
        for q in reversed(self.namn[self.scope:]):
            if q.namn == n and getattr(q, "forward", False):
                p = q
        yttre_scope, yttre_lokala = self.scope, getattr(self, "lokala", 0)
        if p is None:
            p = self.ny(Namn(n, "funk" if funk else "proc", nr=self.nytt_nr(),
                             niva=self.niva + 1, params=[], forward=False))
            self.niva += 1
            self.scope = len(self.namn)
            self.lokala = 0
            if self.sym == "(":
                self.nasta()
                while True:
                    varpar = self.sym == "VAR"
                    if varpar:
                        self.nasta()
                    namn = [self.namnet()]
                    while self.sym == ",":
                        self.nasta()
                        namn.append(self.namnet())
                    self.vant(":")
                    t = self.sok(self.namnet())
                    if t.slag != "typ":
                        self.fel(10)
                    kopia = block_typ(t.typ) and not varpar
                    for m in namn:
                        p.params.append((t.typ, varpar))
                        self.ny(Namn(m, "var", t.typ, niva=self.niva,
                                     adr=self.lokala, varpar=varpar or kopia,
                                     kopia=kopia))
                        self.lokala += 1 if varpar or kopia else ord_i(t.typ)
                    if self.sym != ";":
                        break
                    self.nasta()
                self.vant(")")
            if funk:
                self.vant(":")
                t = self.sok(self.namnet())
                if t.slag != "typ" or block_typ(t.typ) or t.typ == TEXT:
                    self.fel(10)
                p.typ = t.typ
            self.vant(";")
            if self.sym == "FORWARD":
                self.nasta()
                self.vant(";")
                p.forward = True
                p.fparams = self.namn[self.scope:]
                self.namn = self.namn[:self.scope]
                self.niva -= 1
                self.scope, self.lokala = yttre_scope, yttre_lokala
                return
        else:
            # Den riktiga deklarationen efter FORWARD, utan parametrar.
            if funk:
                self.vant(":") if self.sym == ":" else None
                if self.sym == "namn":
                    self.nasta()
            self.vant(";")
            p.forward = False
            self.niva += 1
            self.scope = len(self.namn)
            self.namn += p.fparams
            self.lokala = parord(p)
        self.block(p)
        self.vant(";")
        self.namn = self.namn[:self.scope]
        self.niva -= 1
        self.scope, self.lokala = yttre_scope, yttre_lokala

    def kropp(self, proc):
        for q in self.namn[self.scope:]:
            if getattr(q, "forward", False):
                self.fel(19)
        self.kod = bytearray()
        self.temp = []               # FOR:s gränser, ord i posten
        self.tempdjup = 0
        self.op("ENT", 0)
        for q, ptr in getattr(proc, "kopior", []):
            self.adressen(q)
            self.lad_ord(ptr)
            self.op("MOV", q.typ.bytes)
        self.sammansatt()
        if self.niva == 0:
            self.op("HLT")
        else:
            lok = self.lokala - parord(proc)
            if lok > 255:
                self.fel(15)
            self.kod[1] = lok
            self.op("RET")
        self.klara.append((proc.nr, self.kod))

    # -- satserna -------------------------------------------------------

    def sammansatt(self):
        self.vant("BEGIN")
        self.sats()
        while self.sym == ";":
            self.nasta()
            self.sats()
        self.vant("END")

    def sats(self):
        s = self.sym
        if s == "namn":
            p = self.sok(self.namnet())
            if p.slag == "var":
                self.tilldelning(p)
            elif p.slag == "funk" and self.sym == ":=":
                if p.niva != self.niva:
                    self.fel(22)
                self.nasta()
                self.op("LFA")
                self.ta_till(self.uttryck(), p.typ)
                self.op("STOR" if p.typ == REELL else
                        "STOS" if isinstance(p.typ, Mangd) else "STO")
            elif p.slag == "proc":
                self.anrop(p)
            elif p.slag == "std":
                self.standard(p.namn, sats=True)
            else:
                self.fel(14)
        elif s == "BEGIN":
            self.sammansatt()
        elif s == "IF":
            self.nasta()
            self.ta(self.uttryck(), BOOL)
            self.vant("THEN")
            f = self.framat("FJP")
            self.sats()
            if self.sym == "ELSE":
                self.nasta()
                u = self.framat("UJP")
                self.fyll(f)
                self.sats()
                self.fyll(u)
            else:
                self.fyll(f)
        elif s == "WHILE":
            self.nasta()
            borjan = len(self.kod)
            self.ta(self.uttryck(), BOOL)
            self.vant("DO")
            f = self.framat("FJP")
            self.sats()
            self.bakat("UJP", borjan)
            self.fyll(f)
        elif s == "REPEAT":
            self.nasta()
            borjan = len(self.kod)
            self.sats()
            while self.sym == ";":
                self.nasta()
                self.sats()
            self.vant("UNTIL")
            self.ta(self.uttryck(), BOOL)
            self.bakat("FJP", borjan)
        elif s == "FOR":
            self.forsats()
        elif s == "CASE":
            self.fallsats()
        elif s == "WITH":
            self.medsats()

    def medsats(self):
        """WITH p, q DO sats: postens fält blir namn under satsen. En hel
        post får fälten som egna variabler; annars ligger postens adress i
        ett gömt ord."""
        self.nasta()
        n0, d0 = len(self.namn), self.tempdjup
        while True:
            if self.sym != "namn":
                self.fel(12)
            v = self.sok(self.namnet())
            if v.slag != "var":
                self.fel(12)
            slag, t, w = self.variabel(v)
            if not isinstance(t, Post):
                self.fel(8, var="förra")
            if slag == "enkel" and not w.varpar:
                for n, (off, ft) in t.falt.items():
                    self.namn.append(self.faltnamn(w, n, off, ft))
            else:
                if slag == "enkel":
                    self.adressen(w)
                g = self.temp_ord()
                self.lagra_ord(g)
                for n, (off, ft) in t.falt.items():
                    self.namn.append(Namn(n, "var", ft, niva=self.niva, adr=0,
                                          varpar=False, wtemp=g, off=off))
            if self.sym != ",":
                break
            self.nasta()
        self.vant("DO")
        self.sats()
        del self.namn[n0:]
        self.tempdjup = d0

    def ta(self, t, onskad):
        """Typen t ska vara onskad; ett INTEGER blir REAL med FLT."""
        if t == HELTAL and onskad == REELL:
            self.op("FLT")
        elif t != onskad and not (strang(t) and strang(onskad)
                                  and t.bytes == onskad.bytes):
            if isinstance(onskad, Del) and t == onskad.bas:
                return
            if t == NILTYP and isinstance(onskad, Pekare):
                return
            if mangd_par(t, onskad) is not None:
                return
            self.fel(8, var="förra")

    def ta_till(self, t, onskad):
        """Som ta, när värdet ska till en variabel av typen onskad: ett
        delomfång prövas med CHK."""
        self.ta(t, onskad)
        if isinstance(onskad, Del):
            self.op("CHK", onskad.lo, onskad.hi)

    def tilldelning(self, v):
        ref = self.variabel(v)
        self.vant(":=")
        if block_typ(ref[1]):
            # Hela fältet, strängen eller posten: MOV till från.
            self.ref_adress(ref)
            self.ta(self.uttryck(), ref[1])
            self.op("MOV", ref[1].bytes)
            return
        self.fore_lagra(ref)
        self.ta_till(self.uttryck(), ref[1])
        self.lagra(ref)

    def forsats(self):
        self.nasta()
        v = self.sok(self.namnet())
        if (v.slag != "var" or not ordinal(v.typ)
                or getattr(v, "wtemp", None) is not None):
            self.fel(12)
        ref = ("enkel", v.typ, v)
        self.vant(":=")
        self.fore_lagra(ref)
        self.ta(self.uttryck(), v.typ)
        self.lagra(ref)
        if self.sym not in ("TO", "DOWNTO"):
            self.fel(5, "TO")
        upp = self.sym == "TO"
        self.nasta()
        # Gränsen i ett gömt ord i posten.
        g = self.temp_ord()
        self.ta(self.uttryck(), v.typ)
        self.lagra_ord(g)
        self.vant("DO")
        borjan = len(self.kod)
        self.lad(ref)
        self.lad_ord(g)
        self.op("LEQ" if upp else "GEQ")
        f = self.framat("FJP")
        self.sats()
        self.fore_lagra(ref)
        self.lad(ref)
        self.op("INC1" if upp else "DEC1")
        self.lagra(ref)
        self.bakat("UJP", borjan)
        self.fyll(f)
        self.tempdjup -= 1

    def fallsats(self):
        self.nasta()
        t = self.uttryck()
        if not ordinal(t):
            self.fel(8)
        self.vant("OF")
        tab = self.framat("UJP")
        fall, ut = {}, []
        while True:
            borjan = len(self.kod)
            while True:
                kt, kv = self.konstant()
                self.ta(kt, t)
                if kv in fall:
                    self.fel(17)
                fall[kv] = borjan
                if self.sym != ",":
                    break
                self.nasta()
            self.vant(":")
            self.sats()
            ut.append(self.framat("UJP"))
            if self.sym != ";":
                break
            self.nasta()
            if self.sym == "END":
                break
        self.vant("END")
        self.fyll(tab)
        lo, hi = min(fall), max(fall)
        if hi - lo > 255:
            self.fel(18)
        slut = len(self.kod) + 5 + 2 * (hi - lo + 1)
        self.op("XJP")
        self.kod += struct.pack("<hh", lo, hi)
        for i in range(lo, hi + 1):
            self.kod += struct.pack("<h", fall.get(i, slut) - slut)
        self.op("ERR", 4)
        for u in ut:
            self.fyll(u)

    def anrop(self, p):
        """Anropet av en procedur eller funktion; namnet är läst."""
        if p.slag == "funk":
            for _ in range(ord_i(p.typ)):
                self.op("LDC0")
        self.op("MST", self.niva - (p.niva - 1))
        n = 0
        if self.sym == "(":
            self.nasta()
            while True:
                if n >= len(p.params):
                    self.fel(13)
                typ, varpar = p.params[n]
                if varpar:
                    if self.sym != "namn":
                        self.fel(12)
                    v = self.sok(self.namnet())
                    if v.slag != "var":
                        self.fel(12)
                    ref = self.variabel(v)
                    if ref[1] != typ:
                        self.fel(8)
                    if ref[0] == "badr":
                        # ett tecken i en sträng är ingen VAR-parameter
                        self.fel(12)
                    self.ref_adress(ref)
                else:
                    self.ta_till(self.uttryck(), typ)
                n += 1
                if self.sym != ",":
                    break
                self.nasta()
            self.vant(")")
        if n != len(p.params):
            self.fel(13)
        self.op("CUP", 2 * parord(p), p.nr)

    # -- standardprocedurerna -------------------------------------------

    def filen(self):
        """Om nästa argument är en fil: ladda den och ge True."""
        if self.sym == "namn":
            for p in reversed(self.namn):
                if p.namn == self.val:
                    break
            else:
                return False
            if p.typ == TEXT and p.slag in ("var", "stdfil"):
                self.nasta()
                if p.slag == "stdfil":
                    self.op("LDC0")
                else:
                    self.lad(("enkel", TEXT, p))
                return True
        return False

    def fil_igen(self):
        """Filen en gång till (den ligger i self.fil_ref)."""
        if self.fil_ref is None:
            self.op("LDC0")
        else:
            self.lad(("enkel", TEXT, self.fil_ref))

    def standard(self, n, sats):
        """WRITE och de andra; ger funktionens typ."""
        if n in ("WRITE", "WRITELN", "READ", "READLN"):
            if not sats:
                self.fel(8)
            self.fil_ref = None
            forsta = True
            if self.sym == "(":
                self.nasta()
                while True:
                    if forsta and self.sym == "namn":
                        p = self.sok(self.val)
                        if p.typ == TEXT and p.slag in ("var", "stdfil"):
                            self.nasta()
                            self.fil_ref = p if p.slag == "var" else None
                            if self.sym != ",":
                                break
                            self.nasta()
                    forsta = False
                    if n[:4] == "READ":
                        self.las_arg()
                    else:
                        self.skriv_arg()
                    if self.sym != ",":
                        break
                    self.nasta()
                self.vant(")")
            if n == "WRITELN":
                self.fil_igen()
                self.op("CSP", CSP["WLN"])
            elif n == "READLN":
                self.fil_igen()
                self.op("CSP", CSP["RLN"])
            return None
        if n in ("RESET", "REWRITE"):
            if not sats:
                self.fel(8)
            self.vant("(")
            v = self.sok(self.namnet())
            if v.slag != "var" or v.typ != TEXT:
                self.fel(12)
            self.adressen(v)
            self.vant(",")
            if self.sym == "namn":
                # Namnet i ett ARRAY OF CHAR eller en sträng.
                q = self.sok(self.namnet())
                if (q.slag != "var" or not isinstance(q.typ, Falt)
                        or q.typ.elem != TECKEN):
                    self.fel(5, "ett filnamn")
                self.adressen(q)
                self.ldc(q.typ.hi - q.typ.lo + 1)
                if q.typ.packad:
                    self.op("CSP", CSP["RST" if n == "RESET" else "RWR"])
                else:
                    self.op("CSP", CSP["RSV" if n == "RESET" else "RWV"])
            else:
                if self.sym != "strang" and self.sym != "tecken":
                    self.fel(5, "ett filnamn")
                s = self.val if self.sym == "strang" else bytes([self.val])
                self.nasta()
                self.op("LSA", len(s))
                self.kod += s
                self.ldc(len(s))
                self.op("CSP", CSP["RST" if n == "RESET" else "RWR"])
            self.vant(")")
            return None
        if n == "CLOSE":
            if not sats:
                self.fel(8)
            self.vant("(")
            if not self.filen():
                self.fel(12)
            self.op("CSP", CSP["CLO"])
            self.vant(")")
            return None
        if n == "HALT":
            if not sats:
                self.fel(8)
            self.op("HLT")
            return None
        if n in ("NEW", "MARK", "RELEASE"):
            # högen, bara i den stora versionen
            if not sats:
                self.fel(8)
            self.vant("(")
            if self.sym != "namn":
                self.fel(12)
            v = self.sok(self.namnet())
            if v.slag != "var":
                self.fel(12)
            ref = self.variabel(v)
            if not isinstance(ref[1], Pekare):
                self.fel(8, var="förra")
            if n == "RELEASE":
                self.lad(ref)
                self.op("CSP", CSP["REL"])
            else:
                self.ref_adress(ref)
                if n == "NEW":
                    self.ldc(2 * ord_i(ref[1].mal))
                    self.op("CSP", CSP["NEW"])
                else:
                    self.op("CSP", CSP["MRK"])
            self.vant(")")
            return None
        if sats:
            self.fel(14)
        if n in ("EOF", "EOLN"):
            if self.sym == "(":
                self.nasta()
                if not self.filen():
                    self.fel(12)
                self.vant(")")
            else:
                self.op("LDC0")
            self.op("CSP", CSP["EOF" if n == "EOF" else "EOL"])
            return BOOL
        self.vant("(")
        t = self.uttryck()
        self.vant(")")
        if n == "ORD":
            if not ordinal(t):
                self.fel(8)
            return HELTAL
        if n == "CHR":
            self.ta(t, HELTAL)
            return TECKEN
        if n in ("SUCC", "PRED"):
            if not ordinal(t):
                self.fel(8)
            self.op("INC1" if n == "SUCC" else "DEC1")
            return t
        if n == "ODD":
            self.ta(t, HELTAL)
            self.op("ODD")
            return BOOL
        if n in ("TRUNC", "ROUND"):
            self.ta(t, REELL)
            self.op("TRC" if n == "TRUNC" else "RND")
            return HELTAL
        if n in ("ABS", "SQR") and t == HELTAL:
            self.op({"ABS": "ABS", "SQR": "SQRI"}[n])
            return HELTAL
        self.ta(t, REELL)
        self.op({"ABS": "ABR", "SQR": "SQRR", "SQRT": "SQT", "SIN": "SIN",
                 "COS": "COS", "ARCTAN": "ATN", "LN": "LNR",
                 "EXP": "EXR"}[n])
        return REELL

    def fil_ladd(self):
        self.fil_igen()

    def skriv_arg(self):
        self.fil_igen()
        t = self.uttryck()
        if strang(t):
            self.ldc(t.hi - t.lo + 1)
            t = "S"
        elif t not in (HELTAL, TECKEN, BOOL, REELL):
            self.fel(8)
        if self.sym == ":":
            self.nasta()
            self.ta(self.uttryck(), HELTAL)
        else:
            self.op("LDC0")
        if t == REELL:
            # :w:d, och utan d i exponentform
            if self.sym == ":":
                self.nasta()
                self.ta(self.uttryck(), HELTAL)
            else:
                self.op("LDCN")
            self.op("WRR")
            return
        self.op("CSP", CSP[{"S": "WRS", HELTAL: "WRI", TECKEN: "WRC",
                            BOOL: "WRB"}[t]])

    def las_arg(self):
        if self.sym != "namn":
            self.fel(12)
        v = self.sok(self.namnet())
        if v.slag != "var":
            self.fel(12)
        ref = self.variabel(v)
        t = grund(ref[1])
        if t not in (HELTAL, TECKEN, REELL):
            self.fel(8)
        self.fore_lagra(ref)
        self.fil_igen()
        if t == REELL:
            self.op("RDR")
        else:
            self.op("CSP", CSP["RDI" if t == HELTAL else "RDC"])
        if isinstance(ref[1], Del):
            self.op("CHK", ref[1].lo, ref[1].hi)
        self.lagra(ref)

    # -- uttrycken ------------------------------------------------------

    def blandat(self, t, u):
        """Två tal där minst ett är REAL: båda blir REAL. Ger True då."""
        if REELL not in (t, u) or {t, u} - {HELTAL, REELL}:
            return False
        if u == HELTAL:
            self.op("FLT")
        if t == HELTAL:
            self.op("FLO")
        return True

    def uttryck(self):
        t = self.enkelt()
        rel = ["=", "<>", "<", "<=", ">", ">="]
        if self.sym == "IN":
            self.nasta()
            u = self.enkelt()
            if (not ordinal(t) or not isinstance(u, Mangd)
                    or u.elem not in (None, t)):
                self.fel(8, var="förra")
            self.op("INN")
            return BOOL
        if self.sym in rel:
            i = rel.index(self.sym)
            self.nasta()
            u = self.enkelt()
            if mangd_par(t, u) is not None:
                # = <> <= >=; <= är "ingår i"
                if i in (2, 4):
                    self.fel(8, var="förra")
                self.op({0: "EQS", 1: "NES", 3: "LSS", 5: "GSS"}[i])
            elif ((isinstance(t, Pekare) or t == NILTYP)
                  and (isinstance(u, Pekare) or u == NILTYP)):
                if i > 1 or (t != u and NILTYP not in (t, u)):
                    self.fel(8, var="förra")
                self.op(["EQU", "NEQ"][i])
            elif self.blandat(t, u):
                self.op(["EQR", "NER", "LSR", "LER", "GTR", "GER"][i])
            elif strang(t) and strang(u) and t.bytes == u.bytes:
                # Strängarna: CPS ger -1, 0 eller 1, som jämförs med 0.
                self.op("CPS", t.bytes)
                self.op("LDC0")
                self.op(["EQU", "NEQ", "LES", "LEQ", "GRT", "GEQ"][i])
            elif t != u or not ordinal(t):
                self.fel(8)
            else:
                self.op(["EQU", "NEQ", "LES", "LEQ", "GRT", "GEQ"][i])
            t = BOOL
        return t

    def enkelt(self):
        tecken = None
        if self.sym in ("+", "-"):
            tecken = self.sym
            self.nasta()
        t = self.term()
        if tecken:
            if t == REELL:
                if tecken == "-":
                    self.op("NGR")
            else:
                self.ta(t, HELTAL)
                if tecken == "-":
                    self.op("NEG")
        while self.sym in ("+", "-", "OR"):
            o = self.sym
            self.nasta()
            u = self.term()
            if o == "OR":
                self.ta(t, BOOL)
                self.ta(u, BOOL)
                self.op("OR")
            elif isinstance(t, Mangd):
                m = mangd_par(t, u)
                if m is None:
                    self.fel(8, var="förra")
                self.op("UNI" if o == "+" else "DIF")
                t = m
            elif self.blandat(t, u):
                self.op("ADR" if o == "+" else "SBR")
                t = REELL
            else:
                self.ta(t, HELTAL)
                self.ta(u, HELTAL)
                self.op("ADD" if o == "+" else "SUB")
        return t

    def term(self):
        t = self.faktor()
        while self.sym in ("*", "/", "DIV", "MOD", "AND"):
            o = self.sym
            self.nasta()
            u = self.faktor()
            if o == "AND":
                self.ta(t, BOOL)
                self.ta(u, BOOL)
                self.op("AND")
            elif o == "*" and isinstance(t, Mangd):
                m = mangd_par(t, u)
                if m is None:
                    self.fel(8, var="förra")
                self.op("INT")
                t = m
            elif o == "/":
                # alltid REAL
                if t == HELTAL and u == HELTAL:
                    self.op("FLT")
                    self.op("FLO")
                elif not self.blandat(t, u):
                    self.ta(t, REELL)
                    self.ta(u, REELL)
                self.op("DVR")
                t = REELL
            elif o == "*" and self.blandat(t, u):
                self.op("MPR")
                t = REELL
            else:
                self.ta(t, HELTAL)
                self.ta(u, HELTAL)
                self.op({"*": "MUL", "DIV": "DIV", "MOD": "MOD"}[o])
        return t

    def faktor(self):
        s = self.sym
        if s == "tal":
            self.ldc(self.val)
            self.nasta()
            return HELTAL
        if s == "tecken":
            self.ldc(self.val)
            self.nasta()
            return TECKEN
        if s == "reell":
            self.ldcr(self.val)
            self.nasta()
            return REELL
        if s == "strang":
            n = len(self.val)
            self.op("LSA", n)
            self.kod += self.val
            self.nasta()
            return Falt(1, n, TECKEN, True)
        if s == "(":
            self.nasta()
            t = self.uttryck()
            self.vant(")")
            return t
        if s == "NOT":
            self.nasta()
            self.ta(self.faktor(), BOOL)
            self.op("NOT")
            return BOOL
        if s == "NIL":
            self.nasta()
            self.op("LDC0")
            return NILTYP
        if s == "[":
            return self.mangd()
        if s == "namn":
            p = self.sok(self.namnet())
            if p.slag == "konst":
                if p.typ == REELL:
                    self.ldcr(p.varde)
                else:
                    self.ldc(p.varde)
                return p.typ
            if p.slag == "var":
                ref = self.variabel(p)
                if ref[1] == TEXT:
                    self.fel(8)
                if block_typ(ref[1]):
                    # hela fältet, strängen eller posten: adressen
                    self.ref_adress(ref)
                else:
                    self.lad(ref)
                return grund(ref[1])
            if p.slag == "funk":
                self.anrop(p)
                return grund(p.typ)
            if p.slag == "std":
                t = self.standard(p.namn, sats=False)
                return t
            self.fel(8)
        self.fel(5, "ett uttryck")

    def mangd(self):
        """[a, b..c]: SGS för ett element, SRS för lo..hi, och UNI."""
        self.nasta()
        if self.sym == "]":
            self.nasta()
            self.op("LDSE")
            return Mangd(None)
        elem, forsta = None, True
        while True:
            t = self.uttryck()
            if not ordinal(t) or elem not in (None, t):
                self.fel(8, var="förra")
            elem = t
            if self.sym == "..":
                self.nasta()
                self.ta(self.uttryck(), t)
                self.op("SRS")
            else:
                self.op("SGS")
            if not forsta:
                self.op("UNI")
            forsta = False
            if self.sym != ",":
                break
            self.nasta()
        self.vant("]")
        return Mangd(elem)

    # -- filen ----------------------------------------------------------

    def fil(self):
        """P-kodfilen: huvudet, koden med procedurerna i den ordning de
        blev klara, och sist tabellen, de globala och antalet."""
        offs, kod = [0] * self.procs, bytearray()
        for nr, k in self.klara:
            offs[nr] = len(kod)
            kod += k
        return (b"PK" + bytes([VERSION_STOR if self.stor else VERSION, 1]) + kod
                + b"".join(struct.pack("<H", o) for o in offs)
                + struct.pack("<HH", 2 * self.globala,
                              self.procs | (REALFLAGGA if self.real else 0)))


def lista(kod, nr, ut):
    """Skriv procedurens p-kod som text."""
    ut.write("PROC %d\n" % nr)
    a = 0
    while a < len(kod):
        namn, typ = PER_KOD[kod[a]]
        n = storlek(kod[a], kod, a)
        ops = []
        if typ == "n":
            ops.append(repr(bytes(kod[a + 2:a + n]).decode("latin-1")))
        elif typ == "x":
            lo, hi = s16(kod[a + 1] | kod[a + 2] << 8), s16(kod[a + 3] | kod[a + 4] << 8)
            ops += [str(lo), str(hi)]
            for i in range(hi - lo + 1):
                r = s16(kod[a + 5 + 2 * i] | kod[a + 6 + 2 * i] << 8)
                ops.append("%04X" % (a + n + r))
        else:
            j = a + 1
            for t in typ:
                if t == "b":
                    ops.append(str(kod[j]))
                    j += 1
                elif t == "s":
                    ops.append("%04X" % (a + n + s16(kod[j] if kod[j] < 128 else kod[j] - 256)))
                    j += 1
                else:
                    v = kod[j] | kod[j + 1] << 8
                    ops.append("%04X" % (a + n + s16(v)) if t == "r" else str(s16(v)))
                    j += 2
        ut.write("  %04X  %-5s %s\n" % (a, namn, " ".join(ops)))
        a += n


def main():
    args = sys.argv[1:]
    visa = "-l" in args
    args = [a for a in args if a != "-l"]
    if len(args) != 2:
        sys.exit(__doc__)
    with open(args[0], encoding="latin-1") as f:
        k = Kompilator(f.read())
    try:
        k.program()
    except Fel as e:
        sys.exit("%s: %s" % (args[0], e))
    with open(args[1], "wb") as f:
        f.write(k.fil())
    if visa:
        for nr, kod in k.klara:
            lista(kod, nr, sys.stdout)


if __name__ == "__main__":
    main()
