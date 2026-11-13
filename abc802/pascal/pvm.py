#!/usr/bin/env python3
"""pvm.py -- p-kodtolken i Python, förlagan till tolken i Z80-assembler.

    python3 pvm.py [-t] [-s] [-c] prog.pk [indata ...]

-t skriver varje instruktion (spåret), -s skriver efteråt hur många
instruktioner som kördes och hur djup stacken blev. -c är den stora
tolken, PVM.COM under CP/M (PASCAL.md, avsnitt 8 och 9): p-koden ligger
lägre och stacken börjar på $E800, och den kan mängderna och högen
(version 2 av p-kodfilen). Tangentbordet är
stdin och skärmen stdout, byte för byte. Filerna som programmet öppnar
med RESET och REWRITE är filer i katalogen där pvm.py körs.

Minnet är 64 KB som på ABC 802, med samma adresser som tolken där ska
ha (PASCAL.md, avsnitt 5): p-koden läses till KOD, de globala ligger
efter den och stacken växer nedåt från TOPP.

REAL: när p-kodfilen säger att programmet använder REAL läses fpp.bin
(sh bygg.sh fpp) till FPPADR som i pvm.asm, och stacken börjar under
den. Flyttalen räknas av FPP i fpp.bin, med Z80-processorn ur emulatorn
(z80kor.c, som översätts med cc första gången), så att talen blir
desamma som på ABC 802.
"""
import ctypes
import os
import struct
import subprocess
import sys

from pkod import (CSP_NAMN, CSPSTOR, FEL, FPPADR, PER_KOD, REALFLAGGA,
                  VERSION, VERSION_STOR, s16, storlek)

KOD = 0x9000         # efter tolken på $8000
TOPP = 0xED00        # uppmätt för Forth med KALKYL
MARGINAL = 64        # bytes för uttrycken ovanför de globala
# PVM.COM (-c): p-koden efter tolken med FPP, stacken under BDOS och en
# större marginal, eftersom en mängd är 32 bytes på stacken.
KOD_C = 0x2800
TOPP_C = 0xE800
MARGINAL_C = 256


class Korfel(Exception):
    pass


HAR = os.path.dirname(os.path.abspath(__file__))
_KARNA = None


def karna():
    """z80kor.c som delat bibliotek, översatt om det saknas eller är äldre."""
    global _KARNA
    if _KARNA is None:
        c = os.path.join(HAR, "z80kor.c")
        so = os.path.join(HAR, "z80kor.so")
        if (not os.path.exists(so)
                or os.path.getmtime(so) < os.path.getmtime(c)):
            subprocess.run(["cc", "-O2", "-shared", "-fPIC", "-o", so, c],
                           check=True)
        _KARNA = ctypes.CDLL(so)
        _KARNA.fpp_anrop.restype = ctypes.c_int
    return _KARNA


# FPP:s operationer (fpp/fpp.asm) för opkoderna
FPPOP = {"ADR": 11, "SBR": 13, "MPR": 10, "DVR": 15,
         "EQR": 9, "NER": 6, "LSR": 8, "LER": 5, "GTR": 12, "GER": 7,
         "ABR": 16, "SQT": 30, "SIN": 29, "COS": 20, "ATN": 19,
         "LNR": 24, "EXR": 22}
VAL, STR, FIX, FLOAT = 36, 37, 38, 39
RBUF = 0xFE00        # texten till och från FPP i pvm.py
RFMT = 0xFE80        # @% till STR
RTKN = b"0123456789+-.E"


class Fil:
    """En textfil med Pascals buffertvariabel: nasta är tecknet som står
    på tur, None vid filens slut; ett radslut läses som 10."""

    def __init__(self, las=None, skriv=None):
        self.las, self.skriv = las, skriv
        self.nasta = None
        self.last = False
        self.ut = 0              # kolumnen vid skrivning (för WRITELN)

    def _hamta(self):
        b = self.las.read(1)
        if not b:
            return None
        if b == b"\r":           # CR, CR LF och LF är alla radslut
            self.cr = True
            return 10
        if b == b"\n" and getattr(self, "cr", False):
            self.cr = False
            return self._hamta()
        self.cr = False
        return b[0]

    def titta(self):
        if not self.last:
            self.nasta = self._hamta()
            self.last = True
        return self.nasta

    def ta(self):
        c = self.titta()
        self.last = False
        return c


class Maskin:
    def __init__(self, pk, spar=False, stor=False):
        self.stor = stor
        KOD, TOPP, MARGINAL = ((KOD_C, TOPP_C, MARGINAL_C) if stor else
                               (globals()["KOD"], globals()["TOPP"],
                                globals()["MARGINAL"]))
        self.marg = MARGINAL
        if pk[:2] != b"PK" or pk[2] not in ((VERSION, VERSION_STOR) if stor
                                            else (VERSION,)):
            raise SystemExit("inte en p-kodfil (version %d)" % VERSION)
        glob, n = struct.unpack_from("<HH", pk, len(pk) - 4)
        self.real = bool(n & REALFLAGGA)
        n &= ~REALFLAGGA
        self.m = bytearray(0x10000)
        self.topp = FPPADR if self.real else TOPP
        if KOD + len(pk) > TOPP:
            raise SystemExit("programmet ryms inte")
        self.m[KOD:KOD + len(pk)] = pk
        tab = len(pk) - 4 - 2 * n
        self.procs = [KOD + 4 + struct.unpack_from("<H", pk, tab + 2 * i)[0]
                      for i in range(n)]
        self.gb = (KOD + len(pk) + 1) & ~1
        self.grans = self.gb + glob + MARGINAL
        # högen: från de globalas slut uppåt, mot stacken
        self.hog0 = self.hog = self.gb + glob
        if self.grans >= self.topp:
            raise SystemExit("programmet ryms inte")
        if self.real:
            try:
                fpp = open(os.path.join(HAR, "fpp.bin"), "rb").read()
            except OSError:
                raise SystemExit("fpp.bin saknas (sh bygg.sh fpp)")
            self.m[FPPADR:FPPADR + len(fpp)] = fpp
            self.minne = (ctypes.c_uint8 * 0x10000).from_buffer(self.m)
        self.sp = self.topp
        self.spar = spar
        self.antal = 0
        self.lagst = self.topp
        self.filer = {0: Fil(sys.stdin.buffer, sys.stdout.buffer)}
        # Märket för huvudprogrammet: statisk länk, dynamisk länk och
        # återhopp 0, som RET tolkar som slutet.
        for _ in range(3):
            self.push(0)
        self.mp = self.sp
        self.pc = self.procs[0]

    # -- minnet och stacken
    def ord(self, a):
        return self.m[a & 0xFFFF] | self.m[(a + 1) & 0xFFFF] << 8

    def satt(self, a, v):
        self.m[a & 0xFFFF] = v & 0xFF
        self.m[(a + 1) & 0xFFFF] = (v >> 8) & 0xFF

    def push(self, v):
        self.sp -= 2
        self.satt(self.sp, v)

    def pop(self):
        v = self.ord(self.sp)
        self.sp += 2
        return v

    def spop(self):
        return s16(self.pop())

    def pushr(self, r):
        self.sp -= 6
        self.m[self.sp:self.sp + 6] = bytes(r[:5]) + b"\0"

    def pushm(self, m):
        self.sp -= 32
        self.m[self.sp:self.sp + 32] = m

    def popm(self):
        m = bytes(self.m[self.sp:self.sp + 32])
        self.sp += 32
        return m

    def mangd_op(self, namn):
        """Mängderna, bara i den stora tolken."""
        if namn == "LDSE":
            self.pushm(bytes(32))
        elif namn in ("SGS", "SRS"):
            hi = self.spop()
            lo = self.spop() if namn == "SRS" else hi
            m = bytearray(32)
            for i in range(lo, hi + 1):
                if not 0 <= i <= 255:
                    raise Korfel(1)
                m[i >> 3] |= 1 << (i & 7)
            self.pushm(m)
        elif namn in ("UNI", "INT", "DIF"):
            b, a = self.popm(), self.popm()
            f = {"UNI": lambda x, y: x | y, "INT": lambda x, y: x & y,
                 "DIF": lambda x, y: x & ~y & 0xFF}[namn]
            self.pushm(bytes(f(x, y) for x, y in zip(a, b)))
        elif namn == "INN":
            m, i = self.popm(), self.spop()
            self.push(int(0 <= i <= 255 and bool(m[i >> 3] >> (i & 7) & 1)))
        elif namn in ("EQS", "NES", "LSS", "GSS"):
            b, a = self.popm(), self.popm()
            if namn == "GSS":
                a, b = b, a
            if namn in ("EQS", "NES"):
                v = (a == b) == (namn == "EQS")
            else:
                v = all(x & ~y & 0xFF == 0 for x, y in zip(a, b))
            self.push(int(v))
        elif namn == "INDS":
            a = self.pop()
            self.pushm(bytes(self.m[a:a + 32]))
        elif namn == "STOS":
            m, a = self.popm(), self.pop()
            self.m[a:a + 32] = m

    def popr(self):
        r = bytes(self.m[self.sp:self.sp + 6])
        self.sp += 6
        return r

    # -- flyttalen, med FPP i fpp.bin
    def fpp(self, op, x=None, y=None, de=0, ix=0):
        """FPP med operationen op, x i HLH'L'C och y i DED'E'B (bytes som
        i minnet); ger resultatet som bytes och DE."""
        r = (ctypes.c_uint8 * 16)()
        r[0] = op
        if x is not None:
            r[15], r[14], r[7], r[6], r[3] = x[:5]
        if y is not None:
            r[13], r[12], r[5], r[4], r[2] = y[:5]
        else:
            r[4], r[5] = de >> 8, de & 0xFF
        rix = ctypes.c_uint(ix)
        if karna().fpp_anrop(self.minne, self.ord(FPPADR + 2), r,
                             ctypes.byref(rix)):
            raise Korfel(8)
        if r[1] & 1:                     # CY: fel, numret i A
            raise Korfel(2 if r[0] == 18 else 8)
        return bytes([r[15], r[14], r[7], r[6], r[3], 0]), r[4] << 8 | r[5]

    def fix(self, x):
        v = int.from_bytes(self.fpp(FIX, x)[0][:4], "little", signed=True)
        if not -32768 <= v <= 32767:
            raise Korfel(8)
        return v

    def flt(self, i):
        i = s16(i) & 0xFFFFFFFF
        return self.fpp(FLOAT, i.to_bytes(4, "little") + b"\0")[0]

    def val(self, text):
        self.m[RBUF:RBUF + len(text) + 1] = text + b"\r"
        return self.fpp(FLOAT, self.fpp(VAL, ix=RBUF)[0])[0]

    def real_op(self, namn):
        P = self.push
        if namn in FPPOP:
            op = FPPOP[namn]
            if op < 16:
                y = self.popr()
                v = self.fpp(op, self.popr(), y)[0]
                if 5 <= op <= 12 and op not in (10, 11):
                    P(v[0] & 1)          # FPP: -1 eller 0
                    return
            else:
                v = self.fpp(op, self.popr())[0]
            self.pushr(v)
        elif namn == "LDCR":
            n = self.b()
            text = bytes(self.m[self.pc:self.pc + n])
            self.pc += n
            self.pushr(self.val(text))
        elif namn == "INDR":
            a = self.pop()
            self.pushr(self.m[a:a + 6])
        elif namn == "STOR":
            r = self.popr()
            a = self.pop()
            self.m[a:a + 6] = r
        elif namn == "FLT":
            self.pushr(self.flt(self.pop()))
        elif namn == "FLO":
            r = self.popr()
            self.pushr(self.flt(self.pop()))
            self.pushr(r)
        elif namn == "TRC":
            P(self.fix(self.popr()))
        elif namn == "RND":
            x = self.popr()
            if x[4]:
                x = self.fpp(11, x, bytes([0, 0, 0, x[3] & 0x80, 0x7F]))[0]
            P(self.fix(x))
        elif namn == "NGR":
            x = bytearray(self.popr())
            if x[4]:
                x[3] ^= 0x80
            self.pushr(x)
        elif namn == "SQRR":
            x = self.popr()
            self.pushr(self.fpp(10, x, x)[0])
        elif namn == "WRR":
            d, b = self.pop(), self.pop()
            x = self.popr()
            if d & 0x8000:
                self.m[RFMT + 1:RFMT + 3] = bytes([9, 0])
            else:
                self.m[RFMT + 1:RFMT + 3] = bytes([min(d, 10), 2])
            slut = self.fpp(STR, x, de=RBUF, ix=RFMT)[1]
            self.skriv(self.pop(), bytes(self.m[RBUF:slut]), s16(b))
        elif namn == "RDR":
            fil = self.fil(self.pop())
            while fil.titta() in (32, 10, 9):
                fil.ta()
            text = b""
            while len(text) < 30 and fil.titta() is not None:
                c = fil.titta()
                c = 69 if c == 101 else c
                if c not in RTKN:
                    break
                text += bytes([c])
                fil.ta()
            if not text:
                raise Korfel(5)
            self.pushr(self.val(text))

    def b(self):
        v = self.m[self.pc]
        self.pc += 1
        return v

    def w(self):
        v = self.ord(self.pc)
        self.pc += 2
        return v

    def lokal(self, i):
        return (self.mp - 2 * (i + 1)) & 0xFFFF

    def ut(self, n):
        p = self.mp
        for _ in range(n):
            p = self.ord(p + 4)
        return p

    # -- filerna
    def fil(self, f):
        if f not in self.filer:
            raise Korfel(5)
        return self.filer[f]

    def skriv(self, f, data, bredd=0):
        fil = self.fil(f)
        if fil.skriv is None:
            raise Korfel(5)
        if bredd > len(data):
            data = b" " * (bredd - len(data)) + data
        fil.skriv.write(data)

    def oppna(self, skriv, steg=1):
        """RESET och REWRITE; steg 2 när namnet står i ett fält med ett
        tecken per ord."""
        n = self.pop()
        a = self.pop()
        namn = bytes(self.m[a:a + steg * n:steg]).decode("latin-1").strip()
        adr = self.pop()
        try:
            h = open(namn, "wb" if skriv else "rb")
        except OSError:
            raise Korfel(5)
        nr = max(self.filer) + 1
        self.filer[nr] = Fil(None, h) if skriv else Fil(h, None)
        self.satt(adr, nr)

    def csp(self, n):
        namn = CSP_NAMN.get(n)
        if n >= CSPSTOR and not self.stor:
            raise Korfel(7)
        if namn == "NEW":
            n, a = self.pop(), self.pop()
            if self.hog + n + self.marg >= self.sp:
                raise Korfel(9)
            self.m[self.hog:self.hog + n] = bytes(n)
            self.satt(a, self.hog)
            self.hog += n
            self.grans = self.hog + self.marg
        elif namn == "MRK":
            self.satt(self.pop(), self.hog)
        elif namn == "REL":
            p = self.pop()
            if not self.hog0 <= p <= self.hog:
                raise Korfel(9)
            self.hog = p
            self.grans = self.hog + self.marg
        elif namn == "WRC":
            b, c, f = self.pop(), self.pop(), self.pop()
            self.skriv(f, bytes([c & 0xFF]), s16(b))
        elif namn == "WRI":
            b, v, f = self.pop(), self.spop(), self.pop()
            self.skriv(f, str(v).encode(), s16(b))
        elif namn == "WRS":
            b, n, a, f = self.pop(), self.pop(), self.pop(), self.pop()
            self.skriv(f, bytes(self.m[a:a + n]), s16(b))
        elif namn == "WRB":
            b, v, f = self.pop(), self.pop(), self.pop()
            self.skriv(f, b"TRUE" if v else b"FALSE", s16(b))
        elif namn == "WLN":
            self.skriv(self.pop(), b"\n")
        elif namn == "RDC":
            c = self.fil(self.pop()).ta()
            self.push(32 if c in (None, 10) else c)
        elif namn == "RDI":
            fil = self.fil(self.pop())
            while fil.titta() in (32, 10, 9):
                fil.ta()
            tecken = 1
            if fil.titta() in (43, 45):
                tecken = -1 if fil.ta() == 45 else 1
            v, siffror = 0, 0
            while fil.titta() is not None and 48 <= fil.titta() <= 57:
                v = v * 10 + fil.ta() - 48
                siffror += 1
            if not siffror:
                raise Korfel(5)
            self.push(tecken * v)
        elif namn == "RLN":
            fil = self.fil(self.pop())
            while fil.titta() not in (None, 10):
                fil.ta()
            fil.ta()
        elif namn == "EOL":
            self.push(int(self.fil(self.pop()).titta() in (None, 10)))
        elif namn == "EOF":
            self.push(int(self.fil(self.pop()).titta() is None))
        elif namn == "RST":
            self.oppna(False)
        elif namn == "RWR":
            self.oppna(True)
        elif namn == "RSV":
            self.oppna(False, 2)
        elif namn == "RWV":
            self.oppna(True, 2)
        elif namn == "CLO":
            f = self.pop()
            fil = self.filer.pop(f, None)
            if fil is None or f == 0:
                raise Korfel(5)
            (fil.las or fil.skriv).close()
        else:
            raise Korfel(7)

    # -- instruktionerna
    def steg(self):
        start = self.pc
        kod = self.b()
        if kod not in PER_KOD:
            raise Korfel(6)
        namn, _ = PER_KOD[kod]
        if self.spar:
            self.spara(start, namn)
        self.antal += 1
        P, Q = self.push, self.pop
        if kod <= 0x0F:
            P(kod)
        elif namn == "LDCN":
            P(-1)
        elif namn == "LDCB":
            P(self.b())
        elif namn == "LDCW":
            P(self.w())
        elif namn == "LSA":
            n = self.b()
            P(self.pc)
            self.pc += n
        elif 0x18 <= kod <= 0x1F:
            P(self.ord(self.lokal(kod - 0x18)))
        elif 0x20 <= kod <= 0x27:
            self.satt(self.lokal(kod - 0x20), Q())
        elif namn == "LDL":
            P(self.ord(self.lokal(self.b())))
        elif namn == "STL":
            self.satt(self.lokal(self.b()), Q())
        elif namn == "LLA":
            P(self.lokal(self.b()))
        elif 0x30 <= kod <= 0x37:
            P(self.ord(self.gb + 2 * (kod - 0x30)))
        elif namn == "LDG":
            P(self.ord(self.gb + 2 * self.w()))
        elif namn == "STG":
            self.satt(self.gb + 2 * self.w(), Q())
        elif namn == "LGA":
            P(self.gb + 2 * self.w())
        elif namn in ("LOD", "STR", "LDA"):
            p = self.ut(self.b())
            a = (p - 2 * (self.b() + 1)) & 0xFFFF
            if namn == "LOD":
                P(self.ord(a))
            elif namn == "STR":
                self.satt(a, Q())
            else:
                P(a)
        elif namn == "IND":
            P(self.ord(Q()))
        elif namn == "STO":
            v = Q()
            self.satt(Q(), v)
        elif namn == "INDB":
            P(self.m[Q()])
        elif namn == "STB":
            v = Q()
            self.m[Q()] = v & 0xFF
        elif namn == "MOV":
            n, fran = self.w(), Q()
            till = Q()
            self.m[till:till + n] = self.m[fran:fran + n]
        elif namn == "IXA":
            w, i = self.w(), self.spop()
            P(Q() + i * w)
        elif namn == "CHK":
            lo, hi = s16(self.w()), s16(self.w())
            if not lo <= s16(self.ord(self.sp)) <= hi:
                raise Korfel(1)
        elif 0x50 <= kod <= 0x54:
            b, a = self.spop(), self.spop()
            if namn == "ADD":
                P(a + b)
            elif namn == "SUB":
                P(a - b)
            elif namn == "MUL":
                P(a * b)
            else:
                if b == 0:
                    raise Korfel(2)
                q = abs(a) // abs(b) * (1 if (a < 0) == (b < 0) else -1)
                P(q if namn == "DIV" else a - q * b)
        elif namn == "NEG":
            P(-self.spop())
        elif namn == "ABS":
            P(abs(self.spop()))
        elif namn == "INC1":
            P(Q() + 1)
        elif namn == "DEC1":
            P(Q() - 1)
        elif namn == "SQRI":
            v = self.spop()
            P(v * v)
        elif 0x60 <= kod <= 0x65:
            b, a = self.spop(), self.spop()
            P(int([a == b, a != b, a < b, a <= b, a > b, a >= b][kod - 0x60]))
        elif namn == "AND":
            P(Q() & Q() & 1)
        elif namn == "OR":
            P((Q() | Q()) & 1)
        elif namn == "NOT":
            P((Q() & 1) ^ 1)
        elif namn == "ODD":
            P(Q() & 1)
        elif namn in ("UJPS", "FJPS"):
            r = struct.unpack("<b", bytes([self.b()]))[0]
            if namn == "UJPS" or Q() == 0:
                self.pc += r
        elif namn in ("UJP", "FJP"):
            r = s16(self.w())
            if namn == "UJP" or Q() == 0:
                self.pc += r
        elif namn == "XJP":
            i = self.spop()
            lo, hi = s16(self.w()), s16(self.w())
            tab = self.pc
            self.pc += 2 * (hi - lo + 1)
            if lo <= i <= hi:
                self.pc += s16(self.ord(tab + 2 * (i - lo)))
        elif namn == "MST":
            P(self.ut(self.b()))     # statisk länk
            P(self.mp)               # dynamisk länk
            P(0)                     # återhoppet, sätts av CUP
        elif namn == "CUP":
            k, p = self.b(), self.b()
            self.mp = (self.sp + k) & 0xFFFF
            self.satt(self.mp, self.pc)
            self.pc = self.procs[p]
        elif namn in ("ENT", "ENTW"):
            n = self.b() if namn == "ENT" else self.w()
            if self.sp - 2 * n < self.grans:
                raise Korfel(3)
            for _ in range(n):
                P(0)
        elif namn == "RET":
            ra, dl = self.ord(self.mp), self.ord(self.mp + 2)
            self.sp = self.mp + 6
            self.mp, self.pc = dl, ra
            if ra == 0:
                return False
        elif namn == "LFA":
            P(self.mp + 6)
        elif namn == "CSP":
            self.csp(self.b())
        elif namn == "DUP":
            P(self.ord(self.sp))
        elif namn == "POP":
            Q()
        elif namn == "ERR":
            raise Korfel(self.b())
        elif namn == "HLT":
            return False
        elif 0x90 <= kod <= 0x9F and self.stor:
            self.mangd_op(namn)
        elif 0xA0 <= kod <= 0xBB and self.real:
            self.real_op(namn)
        elif namn == "CPS":
            n, a2, a1 = self.w(), self.pop(), self.pop()
            s1, s2 = self.m[a1:a1 + n], self.m[a2:a2 + n]
            P(0 if s1 == s2 else 1 if s1 > s2 else -1)
        else:
            raise Korfel(6)          # CXP finns bara i den stora
        self.lagst = min(self.lagst, self.sp)
        return True

    def spara(self, adr, namn):
        n = storlek(self.m[adr], self.m, adr)
        ops = self.m[adr + 1:adr + n].hex()
        stack = [self.ord(a) for a in range(self.sp, min(self.sp + 8, self.topp), 2)]
        sys.stderr.write("%04X %-5s %-10s mp=%04X sp=%04X %s\n" % (
            adr, namn, ops, self.mp, self.sp, " ".join("%04X" % v for v in stack)))

    def kor(self):
        try:
            while self.steg():
                pass
        except Korfel as e:
            sys.stdout.buffer.flush()
            sys.stderr.write("\nKÖRFEL %d: %s, PC %04X\n" % (
                e.args[0], FEL.get(e.args[0], "?"), self.pc))
            return 1
        finally:
            for f, fil in self.filer.items():
                if f:
                    (fil.las or fil.skriv).close()
            sys.stdout.buffer.flush()
        return 0


def main():
    args = sys.argv[1:]
    spar = "-t" in args
    stat = "-s" in args
    stor = "-c" in args
    args = [a for a in args if a not in ("-t", "-s", "-c")]
    if not args:
        sys.exit(__doc__)
    with open(args[0], "rb") as f:
        m = Maskin(f.read(), spar, stor)
    fel = m.kor()
    if stat:
        sys.stderr.write("%d instruktioner, stacken %d bytes, de globala på %04X\n" % (
            m.antal, m.topp - m.lagst, m.gb))
    sys.exit(fel)


if __name__ == "__main__":
    main()
