#!/usr/bin/env python3
"""bac2bas.py  BAC-fil (ett sparat, kompilerat BASIC-program) -> text,
som LIST skriver den.

    bac2bas.py [--kod 7|dos|utf8] [--rad cr|crlf|lf] [-o UT] FIL.BAC ...

Läser filerna (eller stdin) och skriver listan till UT (eller stdout).
  --kod  7     ABC:s 7-bitarskod som den är ([\\]{|} och så vidare)
         dos   ÄÖÅäöå... i teckentabell 850 (MS-DOS)
         utf8  ÄÖÅäöå... i UTF-8 (standard)
  --rad  cr    radslut CR, som på ABC
         crlf  CR LF, som i MS-DOS och Windows
         lf    LF (standard)
'$' blir aldrig ¤: i BASIC är det strängarnas tecken.

Formatet ses på första byten:
  $82  ABC80 (BASIC i ABC80, den gamla och den nya ROM:en)
  $8F  BASIC II (ABC 800 C och M, ABC 802, ABC 806)

Programmet kör inte Z80-koden. Det gör LIST rutin för rutin som ROM:en,
med samma namn på rutinerna:

ABC80 (abc80old.asm): DETOK $1195 en rad, SPACE $11EB, DETEXP $1258
uttrycken (omvänd polsk notation), DETOP $1398 en operator, OPTEXT
$13D5, FNTEXT $13EB, DETVAR $1425 ett namn, ROTTXT $1448 byter plats på
två textbitar, PRNUM $1872 ett heltal, FLTASC $1683 ett flyttal (BCD).

BASIC II (ABC 802, abc802rom.lst): DLINE $40D3 en rad, DSTMT $40EA
satserna, DECOMP $3D75 uttrycken, DOPR $3E3D en operator, DMERGE $3FE7
flyttar en textbit, DFUNC $3DED och DFNAME $3FB7 funktionerna, DDIM
$3F15, DNUM $3E7A och FTODEC $5153 ett flyttal (binärt), NAMTX1 $3379
ett namn, S3032 $3032 ord i koden (länkade eller inte), L307D de lokala
namnen i DEF FN.

Tabellerna (nyckelorden, operatorerna och tiopotenserna) är kopierade
ur ROM:en i ABC 802 längst ned, så sökningarna går som i ROM:en.

S. Lönnert 2026. Public domain.
"""

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import abckas  # noqa: E402


class Fel(Exception):
    """Raden kan inte listas."""


# ==================================================================
# ABC80

# STKW1 ($1911) .. KWAS och STKW2 ($199C) direkt efter, som i ROM:en
STKW80 = (b'\x82LET\x80GOTO\x84PRINT\x85;\x88IF'
          b'\x8aINPUTLINE\x89INPUT\x8bFOR\x8cNEXT'
          b'\x8eREAD\x8fRESTORE\x90GOSUB\x91RETURN'
          b'\x94ONERRORGOTO\x92ON\x93DATA'
          b'\x96DEFFN\xff'                                   # KWDEFN
          b'\x81\x83\x95??? '                                # KWLIST
          b'\xbdTHEN\xff\xbcELSE\xff\xbbSTEP\xff'
          b'\xbaTO\xff\xb8ASFILE\xff\xb7AS\xff'
          # STKW2
          b'\x80DIM\x81POKE\x82OUT\x84REM\x85OPEN'
          b'\x86PREPARE\x87CLOSE\x88RANDOMIZE\x89STOP'
          b'\x8aEND\x8bCMD\x8eSETDOT\x8fCLRDOT'
          b'\x90GET\x91CHAIN\x92KILL\x93NAME'
          b'\x94TRACE\x95NOTRACE\xff')
STKW1, KWDEFN, STKW2 = 0, 92, 139

# KWEQV ($23F5), 60 byte, och FNKW ($2431) direkt efter
KWOPFN = (b'\xdcEQV\xff\xddIMP\xff\xdeOR\xdfXOR\xff'
          b'\xe0AND\xff\xe1NOT\xff'
          b'\xf1<=\xe5<>\xe8<\xeb>=\xee>\xe2='
          b'\xff\xf4+\xf7-\xff\xf9*\xfb/\xff'
          b'\xfd^\xfd**\xff'
          # FNKW
          b'\x80FN\x81ABS\x82ATN\x83COS\x84EXP'
          b'\x85FIX\x86INT\x88LOG10\x87LOG\x89PI'
          b'\x8aRND\x8bSGN\x8cSIN\x8dSQR\x8eTAN'
          b'\x8fASC\x90CHR$\x91LEFT$\x92RIGHT$'
          b'\x93MID$\x94LEN\x95INSTR\x96SPACE$'
          b'\x97STRING$\x98NUM$\x99VAL\x9aSWAP%'
          b'\x9bPEEK\x9cINP\x9dCALL\x9eERRCODE'
          b'\x9fIEC$\xa0TAB\xa1CUR\xa3DOT\xb0ADD$'
          b'\xb1SUB$\xb2MUL$\xb3DIV$\xb4COMP%\xff')
KWEQV, KWEQVN, FNKW = 0, 60, 60


def cpir(tab, start, n, a):
    """CPIR från tab[start] (n byte, 0: till slutet): platsen efter koden."""
    end = len(tab) if n == 0 else min(len(tab), start + n)
    i = tab.find(bytes([a]), start, end)
    return -1 if i < 0 else i + 1


class Abc80:
    """En rad [längd][nummer lo][hi][kod ...][$0D] -> text som slutar med CR."""

    def detok(self, line):
        self.code = line
        self.hl = 0
        self.out = bytearray()
        self.stk = []
        self.jump = False
        self.prnum(line[1] | line[2] << 8)
        self.hl = 3
        self.space()
        while True:
            a = self.rd()
            if a == 0x3A:                               # :
                self.space()
                self.wr(a)
                self.hl += 1
                self.space()
            elif a == 0x87:                             # IF/ELSE: hoppet
                self.hl += 2
            elif a == 0xB9:                             # FOR
                self.hl += 3
            elif a in (0xBF, 0xBE):                     # radnummer
                b = 1
                if a == 0xBE:
                    self.hl += 1
                    b = self.rd()
                self.hl += 1
                while True:
                    n = self.rd() | self.rd(1) << 8
                    self.hl += 4
                    self.prnum(n)
                    b = (b - 1) & 0xFF
                    if b == 0:
                        break
                    self.wr(0x2C)
            elif a == 0x0D:                             # radslut
                self.wr(a)
                return bytes(self.out)
            elif a >= 0xC0:
                self.detexp()
                if self.jump:                           # raden slutar här
                    self.wr(0x0D)
                    return bytes(self.out)
            elif a == 0x96:                             # DEF FN
                self.hl += 1
                self.copytext(STKW80, KWDEFN)
                self.detvar()
                self.hl -= 2
                n = self.rd()
                self.hl += 1
                if n:
                    for i in range(n):
                        self.wr(0x2C if i else 0x28)
                        self.detvar()
                        self.hl -= 2
                    self.wr(0x29)
            elif a < 0x80:                              # ett tecken
                self.wr(a)
                self.hl += 1
            else:                                       # ett nyckelord
                t = STKW1
                self.space()
                self.hl += 1
                if a == 0x86:
                    t = STKW2
                    a = self.rd()
                    self.hl += 1
                i = cpir(STKW80, t, 0, a)
                if i < 0:
                    raise Fel('okänt nyckelord $%02X' % a)
                while i < len(STKW80) and not STKW80[i] & 0x80:
                    self.wr(STKW80[i])
                    i += 1
                self.space()

    def rd(self, k=0):                                  # LD A,(HL)
        p = self.hl + k
        if not 0 <= p < len(self.code):
            raise Fel('koden tar slut')
        return self.code[p]

    def wr(self, c):                                    # LD (DE),A : INC DE
        if len(self.out) >= 4096:
            raise Fel('för lång')
        self.out.append(c)

    def push(self, p):
        self.stk.append(p)

    def pop(self, n=1):
        if len(self.stk) < n:
            raise Fel('stacken')
        del self.stk[len(self.stk) - n:]

    def top(self, k=0):
        if len(self.stk) <= k:
            raise Fel('stacken')
        return self.stk[-1 - k]

    def space(self):                                    # SPACE
        if not (self.out and self.out[-1] == 0x20):
            self.wr(0x20)

    def prnum(self, n):                                 # PRNUM
        for c in str(n & 0xFFFF).encode():
            self.wr(c)

    def rottxt(self, p1, p2):                           # ROTTXT
        de = len(self.out)
        if not 0 <= p1 <= p2 <= de:
            raise Fel('ROTTXT')
        o = self.out
        o[p1:de] = o[p2:de] + o[p1:p2]

    def detvar(self):                                   # DETVAR
        """Namnet vid HL (namnbyte, bokstav, 2 byte adress). Svaret är
        antalet index * 4 (0: inget index)."""
        c = self.rd()
        self.hl += 1
        self.wr(self.rd())
        self.hl += 3
        if c & 0xF0 != 0xF0:
            self.wr(((c >> 4) & 0x0F) | 0x30)
        if c & 3:
            self.wr(0x25 if c & 3 == 1 else 0x24)
        return c & 0x0C

    def joina(self, a):                                 # JOINA, JOINC
        p1, p2 = self.top(), len(self.out)
        self.wr(a)
        self.rottxt(p1, p2)
        self.pop()

    def copytext(self, tab, i):                         # L13FA
        while True:
            self.wr(tab[i])
            i += 1
            if i >= len(tab) or tab[i] & 0x80:
                return

    def optext(self, a):                                # OPTEXT
        if a != 0xFF:
            a += 1
        for _ in range(256):
            a = (a - 1) & 0xFF
            i = cpir(KWOPFN, KWEQV, KWEQVN, a)
            if i >= 0:
                self.copytext(KWOPFN, i)
                return
        raise Fel('okänd operator')

    def fnkwtx(self, a):                                # FNKWTX
        i = cpir(KWOPFN, FNKW, 0, a)
        if i < 0:
            raise Fel('okänd funktion')
        self.copytext(KWOPFN, i)

    def fntext(self):                                   # FNTEXT
        a = 0x80 | (self.rd() >> 1)
        self.hl += 1
        self.fnkwtx(a)
        if a == 0x80:
            self.detvar()
            self.hl += 1

    def paren(self):                                    # L12A8
        self.wr(0x28)
        self.rottxt(self.top(1), self.top(0))
        self.wr(0x29)
        self.pop()

    def detop(self, flags):                             # DETOP
        p2, d0 = self.top(), len(self.out)
        if flags & 1 and p2 > 0 and self.out[p2 - 1] != 0x20:
            self.wr(0x20)
        self.optext(self.code[self.hl - 1])
        if flags & 1 and p2 < len(self.out) and self.out[p2] != 0x20:
            self.wr(0x20)
        self.rottxt(p2, d0)
        if not flags & 2:
            self.pop()

    def fltasc(self, e, fmt):
        """FLTASC: talet med exponenten i code[e], formatet fmt (listan
        har $2C: 6 siffror, inget blanksteg före ett positivt tal).
        Mantissan är 6 BCD-siffror i de 3 byten vid e-4, tecknet (bit 0)
        vid e-1. w är arbetsytan vid IX: w[0] formatet, w[1] tecknet, w[2]
        heltalssiffror eller nollor efter punkten, w[5] platsen för en
        minnessiffra, w[6..11] siffrorna, w[13] 1 om x < 1."""
        code = self.code

        def at(i):
            return code[i] if 0 <= i < len(code) else 0

        w = [0] * 16
        w[0] = fmt
        for i in range(3):
            m = at(e - 4 + i)
            w[6 + 2 * i] = (m >> 4) & 0x0F
            w[7 + 2 * i] = m & 0x0F
        w[1] = at(e - 1)
        a = at(e)
        if a == 0:
            a = 1                                       # x = 0
        elif a >= 0x80:
            a -= 0x80
            if a == 0:
                w[13] = 1                               # .1 <= x < 1
        else:
            a = 0x80 - a                                # nollor efter punkten
            w[13] = 1
        w[2] = a
        E = a
        if a >= 6:
            w[0] |= 1                                   # E-form

        def wrdig():
            nonlocal p, c
            self.wr(w[p] + 0x30)
            p += 1
            c -= 1

        def fazero():
            if not w[0] & 0x20:
                self.wr(0x20)
            self.wr(0x30)

        p = 0
        while True:                                     # FAFMT
            n = (w[0] >> 1) & 0x0F
            if w[0] & 1:
                c = min(n, 5) + 1
                break
            if n + E >= 7 and w[0] & 0x40:
                w[0] |= 1
                continue
            if not w[13]:
                c = n + E
                break
            c = n - E                                   # FASMAL
            if c <= 0:                                  # FATINY
                if w[0] & 0x80 and w[0] & 0x0E:
                    E = ((w[0] & 0x0E) >> 1) - 1
                    c, p = 1, 5
                    break
                return fazero()
            break
        tiny = p == 5
        if not tiny:
            c &= 0x1F                                   # FAROUN
            c = min(c, 7)
            pr = p = 6 + c
            if w[p] >= 5:
                while True:                             # FACARY
                    p -= 1
                    w[p] += 1
                    if w[p] != 10:
                        break
                    w[p] = 0
                p = pr
            p -= 1                                      # FASTRP
            while w[p] == 0:
                p -= 1
                c -= 1
                if c < 0:
                    return fazero()
            p = 5                                       # FAOVFL
            if w[5]:
                B = 1
                if w[13]:
                    B = -1
                if w[2] == 0:
                    w[13] = 0
                    B = 1
                w[2] = (w[2] + B) & 0xFF
                E += 1                  # den gamla ROM:ens fel: E+1 också när B = -1
                c += 1
                p = 4
        p += 1                                          # faout
        if c == 7:
            c = 6
        B = E                                           # FASIGN
        if w[1] & 1:
            self.wr(0x2D)
        elif not w[0] & 0x20:
            self.wr(0x20)
        if w[0] & 1:                                    # FAEOUT
            wrdig()
            if c != 0:
                self.wr(0x2E)
                while True:
                    wrdig()
                    if c == 0:
                        break
            self.wr(0x45)
            if w[13]:
                self.wr(0x2D)
                self.prnum((w[2] + 1) & 0xFF)
            else:
                self.wr(0x2B)
                self.prnum((w[2] - 1) & 0xFF)
            return
        if w[13]:                                       # x < 1
            if c == 0:
                self.wr(0x30)
                return
        else:
            while True:                                 # FAINT
                wrdig()
                B -= 1
                if B == 0:
                    break
            if c <= 0:
                return
        self.wr(0x2E)                                   # fadot
        while B > 0:                                    # FALEAD
            self.wr(0x30)
            B -= 1
        while True:                                     # FAFRAC
            wrdig()
            if c == 0:
                break

    def dimelem(self, string):
        """DIM-post ($CC, $D2): "A(i,j)", "A$=n" eller "A$(i,j)=n", sedan
        "," eller $BF, som avslutar DIM-listan. Svaret är sant vid $BF."""
        if string:                                      # L135A
            self.push(len(self.out))
            if self.rd() & 0x0C == 0:                   # L1382: inget index
                carry, a = True, 0x3D
                skip = True
            else:
                self.wr(0x29)
                self.wr(0x3D)
                self.rottxt(self.top(1), self.top(0))
                self.pop(2)
                carry, skip = True, False
        else:
            carry, skip = False, False                  # L133D
        if not skip:
            if self.rd() & 0x08:                        # L133E
                self.joina(0x2C)
            self.push(len(self.out))
            a = 0x28
        self.detvar()                                   # L1347
        self.wr(a)
        self.rottxt(self.top(1), self.top(0))
        if not carry:
            self.wr(0x29)
        self.pop(2)                                     # L1370
        a = self.rd()
        self.hl += 1
        if a == 0xBF:
            return True
        self.hl -= 1
        self.wr(0x2C)
        return False

    def detexp(self):                                   # DETEXP
        while True:
            a = self.rd()
            self.hl += 1
            if 0xB6 <= a <= 0xB8:                       # tilldelning, typ 0-2
                self.joina(0x3D)
                self.pop()
                return
            if a < 0xC0:                                # uttrycket slut
                self.pop()
                # Fler operander kvar: i ROM:en tar RET en operands
                # adress som återhopp och kör den listade texten som
                # Z80-kod. Program använder det som LIST-skydd (en sträng
                # med maskinkod som skriver om listan). Det går inte att
                # följa utan en Z80.
                if self.stk:
                    self.jump = True
                return
            if a == 0xE1:                               # NOT
                self.detop(3)
            elif a >= 0xE2:                             # jämförelse, räknesätt
                self.detop(0)
            elif a >= 0xDC:                             # EQV IMP OR XOR AND
                self.detop(1)
            elif a in (0xD7, 0xD8):                     # L141B: minus
                n = len(self.out)
                self.wr(0x2D)
                self.rottxt(self.top(), n)
            elif a == 0xDB:                             # argument till FN
                self.hl += 4
            elif a == 0xD3:                             # parentes
                self.push(len(self.out))
                self.paren()
            elif a > 0xD3:                              # $D4-$D6, $D9, $DA
                pass
            elif a in (0xC8, 0xC9, 0xCA, 0xC0, 0xC1, 0xC2, 0xC4, 0xC5, 0xC6):
                if a >= 0xC8:                           # två index
                    self.joina(0x2C)
                self.push(len(self.out))                # variabel, ett index
                if self.detvar():
                    self.paren()
            elif a == 0xC3:                             # flyttal
                self.push(len(self.out))
                self.fltasc(self.hl + 4, 0x2C)
                self.hl += 5
            elif a == 0xC7:                             # heltal
                self.push(len(self.out))
                n = self.rd() | self.rd(1) << 8
                self.hl += 2
                self.prnum(n)
                self.wr(0x25)
            elif a == 0xCB:                             # sträng
                self.push(len(self.out))
                q = self.rd()
                n = self.rd(1)
                self.hl += 2
                self.wr(q)
                while True:
                    n = (n - 1) & 0xFF
                    if n & 0x80:
                        break
                    c = self.rd()
                    self.hl += 1
                    self.wr(c)
                    if c == q:
                        self.wr(q)
                self.wr(q)
            elif a == 0xCC:                             # DIM-post
                self.hl += 1
                if self.dimelem(self.rd() & 0x02):
                    return
            elif a == 0xD2:
                self.hl += 2
                if self.dimelem(0):
                    return
            elif a == 0xCD:                             # funktion utan argument
                self.push(len(self.out))
                self.fntext()
            elif a in (0xCE, 0xCF, 0xD0, 0xD1):         # 1-4 argument
                for _ in range(a - 0xCE):
                    self.joina(0x2C)
                self.push(len(self.out))
                self.fntext()
                self.paren()


def list80(data, namn, fel):
    """ABC80: 256-bytes sektorer, den första börjar med $82. Raderna:
    [längd][nummer lo][hi][kod ...][$0D]. Längd $00: resten av sektorn
    är tom, $01: programmet slut."""
    ut, d = bytearray(), Abc80()
    klar = False
    for s in range((len(data) + 255) // 256):
        sec = data[s * 256:s * 256 + 256].ljust(256, b'\0')
        i = 1 if s == 0 else 0
        if sec[i] == 0:
            fel(f'{namn}: sektor {s} är tom (ett block saknas?)')
            continue
        while i < 256:
            L = sec[i]
            if L == 0:
                break
            if L == 1:
                klar = True
                break
            if L < 4 or i + L > 256:
                fel(f'{namn}: sektor {s}: fel radlängd {L}')
                break
            nr = sec[i + 1] | sec[i + 2] << 8
            try:
                t = d.detok(sec[i:i + L])
                if d.jump:
                    fel(f'{namn}: rad {nr}: LIST kör maskinkod här (LIST-skydd); '
                        'raden står som den är sparad fram till dit', varning=True)
            except Fel as e:
                fel(f'{namn}: rad {nr} gick inte att lista ({e})')
                t = b'%d REM *** RADEN KUNDE INTE LISTAS ***\r' % nr
            ut += t
            i += L
        if klar:
            break
    if not klar:
        fel(f'{namn}: inget slutmärke ($01)')
    return bytes(ut)


# ==================================================================
# BASIC II

ROM = {}            # adress -> byte, fylls längst ned


def rom(a):
    try:
        return ROM[a]
    except KeyError:
        raise Fel('ROM-adress $%04X' % a) from None


STKW, OPKW, THENTX = 0x089F, 0x0625, 0x0979
FNKWP, STKWP, P10 = 0x0661, 0xFDD8, 0x4DF2   # STKWP: DOS-noden först, som efter DOSINI


def kwtext(a, tab):
    """DKWTXT: texten efter den första koden a från tab, till nästa kod
    eller $00."""
    p = tab
    while rom(p) != a:
        p += 1
    return kwfrom(p + 1)


def kwfrom(p):                                          # S3FE4
    t = bytearray()
    while 0 < rom(p) < 0x80:
        t.append(rom(p))
        p += 1
    return bytes(t)


def optext(a):
    """DOPTXT: operatorn a. Bara den första av en operators koder står i
    OPKW, så a, a-1, a-2 ... provas."""
    for _ in range(256):
        for p in range(OPKW, OPKW + 60):
            if rom(p) == a:
                return kwfrom(p + 1)
        a = (a - 1) & 0xFF
    raise Fel('okänd operator')


def fndtab(chain, code):
    """FNDTAB: noden i kedjan som har koden, eller None. Noden är
    [nästa][C][B][..][..][tabellen]: koderna C .. C+antal-1 med prefixet
    (bit 7 i B: antal = B & 15 + 1, prefix = B >> 4 & 7)."""
    d, e = code >> 8, code & 0xFF
    node = chain
    while node:
        nxt = rom(node) | rom(node + 1) << 8
        c, b = rom(node + 2), rom(node + 3)
        if b & 0x80:
            n, pre = (b & 15) + 1, (b >> 4) & 7
        else:
            n, pre = b, 0
        if pre == d and (e - c) & 0xFF < n:
            tab = rom(node + 6) | rom(node + 7) << 8
            try:
                return kwtext(((e - c) + 0x80) & 0xFF, tab)
            except Fel:
                # inte i tabellen: ROM:en letar vidare och skriver det
                # den hittar (satsen hör till en annan maskin)
                return None
        node = nxt
    return None


# ------------------------------------------------------------------
# flyttal (DNUM, FTODEC): 8 byte, exponenten först (bias $80, mantissan
# 0.1xxx binärt), tecknet i bit 7 av första mantissabyten

M56 = (1 << 56) - 1


def p10(n):
    """10^n ur ROM:ens tabell vid $4DF2: (exponent, mantissa 56 bitar)."""
    a = P10 + 8 * n
    m = 0
    for i in range(1, 8):
        m = m << 8 | rom(a + i)
    return rom(a), m | 1 << 55


def p10exp(a):
    """P10EXP: den första tiopotensen n >= 1 med exponent >= a + $81."""
    a = (a + 0x81) & 0xFF
    n = 1
    while rom(P10 + 8 * n) < a:
        n += 1
        if n > 38:
            raise Fel('tiopotens')
    return n


def dlen(m):
    """Antalet mantissabyte till den sista som inte är noll."""
    n = 7
    while n > 1 and (m >> (8 * (7 - n))) & 0xFF == 0:
        n -= 1
    return n


def rund(acc, exp):
    """Normalisera 64 bitar och avrunda till 56."""
    if acc == 0:
        return 0, 0
    while not acc >> 63:
        acc <<= 1
        exp -= 1
    if acc & 0x80:
        m = (acc >> 8) + 1
        if m >> 56:
            m, exp = 1 << 55, exp + 1
    else:
        m = acc >> 8
    return exp, m


def dmul(x, p):
    """DMUL: multiplikatorn är den med kortast mantissa (vid lika:
    tiopotensen). Bit för bit, den minsta först, 64 bitar."""
    (e1, m1), (e2, m2) = x, p
    if dlen(m1) < dlen(m2):
        mc, mr = m2, m1
    else:
        mc, mr = m1, m2
    acc = 0
    for i in range(dlen(mr) - 1, -1, -1):
        byte = (mr >> (8 * (6 - i))) & 0xFF
        for _ in range(8):
            if byte & 1:
                acc += mc << 8
            byte >>= 1
            acc >>= 1
    if e1 + e2 - 1 < 0x80:
        return 0, 0
    return rund(acc, e1 + e2 - 0x80)


def ddiv(x, p):
    """DDIV: 64 kvotbitar, sedan som DMUL."""
    (ex, r), (ep, d) = x, p
    q, cy = 0, 0
    for _ in range(64):
        if cy or r >= d:
            r = (r - d) & M56
            q = q << 1 | 1
        else:
            q <<= 1
        r <<= 1
        cy = r >> 56
        r &= M56
    return rund(q, ex - ep + 0x81)


def ftodec(code):
    """Flyttalet (8 byte) -> text, som LIST skriver det."""
    X = code[0]
    neg = code[1] & 0x80
    m = (code[1] | 0x80) << 48
    for i in range(2, 8):
        m |= code[i] << (8 * (7 - i))
    if X == 0:
        return b'0'
    x, ix8 = (X, m), 0
    while True:
        X, m = x
        if X == 0:
            return b'0'
        if 0x81 <= X < 0xB8 and m & ((1 << (56 - (X - 0x80))) - 1) == 0:
            break
        if X <= 0xB8:
            n = p10exp(0xB8 - X)
            ix8 -= n
            x = dmul(x, p10(n))
            continue
        a = X - 0xB8 - 8
        if a >= 0:
            n = p10exp(a)
            ix8 += n
            x = ddiv(x, p10(n))
        break
    X, m = x
    sh = X - 0x80 - 56
    v = m << sh if sh >= 0 else m >> -sh
    s = '%022d' % (v % 10 ** 22)
    C = 22 + ix8
    t = s.lstrip('0')
    C -= len(s) - len(t)
    t = t.rstrip('0')
    if not t:
        return b'0'
    keep = 17 if t[0] < '5' else 16                     # FMRND
    if len(t) > keep:
        up = t[keep] >= '5'
        t = t[:keep]
        if up:
            t = t.rstrip('9')
            if t:
                t = t[:-1] + chr(ord(t[-1]) + 1)
            else:
                t, C = '1', C + 1
        t = t.rstrip('0')
    D = len(t)
    if 0 <= C <= 6 or (C < 0 and C + 6 >= 0 and C + 6 >= D):
        if C <= 0:
            r = '.' + '0' * -C + t
        elif C >= D:
            r = t + '0' * (C - D)
        else:
            r = t[:C] + '.' + t[C:]
    else:
        e = C - 1
        r = t[0] + '.' + t[1:] + 'E' + ('-' if e < 0 else '+') + '%02d' % abs(e)
    return (('-' if neg else '') + r).encode()


# ------------------------------------------------------------------

def bandet(data):
    """Sant om filen är som kas upp skriver den från bandet: sektorer om
    256 byte, där de 3 sista i varje är nollor (blocken har 253 byte).
    En fil från skivan är programmet i ett stycke."""
    return (len(data) % 256 == 0 and
            all(data[i + 253:i + 256] == b'\0\0\0' for i in range(0, len(data), 256)))


class Basic2:
    """Programmet som det står i minnet: sektorernas 253 första byte efter
    varandra. Huvudet: +0 $8F, +2 flaggor (bit 0: länkat), +4 VARBEG,
    +$0A antalet variabler. Raderna från $15: [längd][nr lo][hi] kod $87.
    Vid VARBEG står paren (referens, kod) för variablerna; före VARBEG
    ordet som pekar ut de långa namnen."""

    def __init__(self, data):
        self.m = bytes(data)
        if bandet(data):
            self.m = b''.join(data[i:i + 253] for i in range(0, len(data), 256))
        self.linked = self.m[2] & 1
        self.varbeg = self.w(4)
        self.pairs = {}
        for i in range(self.w(0x0A)):
            p = self.varbeg + 4 * i
            if p + 4 > len(self.m):
                break
            self.pairs.setdefault(self.w(p), self.w(p + 2))
        self.lnkdef = 0
        self.level = 0              # B: nivån för följande rader
        self.asgn = None            # referensen i den senaste tilldelningen
        self.fornext = {}           # NEXT:s ord -> FOR-variabelns referens

    # minnet
    def b(self, a):
        if not 0 <= a < len(self.m):
            raise Fel('utanför programmet')
        return self.m[a]

    def w(self, a):
        return self.b(a) | self.b(a + 1) << 8

    def rd(self):                                       # LD A,(HL) : INC HL
        a = self.b(self.hl)
        self.hl += 1
        return a

    def rdw(self):                                      # LDBCHL
        v = self.w(self.hl)
        self.hl += 2
        return v

    # texten
    def put(self, t):
        if isinstance(t, int):
            self.buf.append(t)
        else:
            self.buf += t
        if len(self.buf) > 4096:
            raise Fel('för lång')

    def dblank(self):                                   # DBLANK
        if not (self.buf and self.buf[-1] == 0x20):
            self.buf.append(0x20)

    def push(self):                                     # PUSH DE
        self.fs.append(len(self.buf))

    def pop(self):                                      # POP BC
        if not self.fs:
            raise Fel('stacken')
        return self.fs.pop()

    def dmerge(self):
        """DMERGE: den senaste textbiten flyttas före den under."""
        s = self.pop()
        f = self.pop()
        if not 0 <= f <= s <= len(self.buf):
            raise Fel('DMERGE')
        b = self.buf
        b[f:] = b[s:] + b[f:s]
        self.fs += [f, s]

    def insert(self, at, c):                            # INSCHR
        if not 0 <= at <= len(self.buf):
            raise Fel('INSCHR')
        self.buf.insert(at, c)

    def dcomma(self, n):                                # DCOMMA
        for _ in range(n - 1):
            self.insert(self.pop(), 0x2C)

    # namn
    def ref(self, kind):
        """S3032: ordet i koden. Länkat program: radnumret, FN-namnets kod
        eller variabelns kod ur adressen; annars ordet som det är."""
        bc = self.rdw()
        if not self.linked:
            return bc
        after = self.hl
        if kind == 9:                                   # radnummer
            return self.w((after + bc + 1) & 0xFFFF)
        if kind == 0x0B:                                # FN-namn
            return self.w((after + bc - 5) & 0xFFFF)
        if kind == 0x0A:
            if bc >> 8 == 0xFF:                         # lokal
                return self.local(bc & 0xFF)
            if bc in self.pairs:
                return self.pairs[bc]
            raise Fel('variabeln $%04X saknas' % bc)
        if kind == 7:
            # NEXT: ordet är FOR-blockets referens - 2, och blocket finns
            # bara när programmet körs. FOR-satsens länk ($BC ord) pekar
            # på det här ordet, så variabeln är den som FOR satte.
            r = self.fornext.get(after - 2)
            if r is not None and r in self.pairs:
                return self.pairs[r]
            for d in (2, 4, 8):
                c = self.pairs.get((bc - d) & 0xFFFF)
                if c is not None and (d == 2) == bool(c & 1) and not c & 2:
                    return c
            if bc == 0:
                return 0
            raise Fel('variabeln i NEXT')
        raise Fel('S3032')

    def local(self, r):
        """L307D: namnet på plats r i DEF FN:s parametrar och LOCAL efter
        dem. Strängarna i LOCAL har ett ord till (storleken)."""
        hl = self.lnkdef
        if not hl:
            raise Fel('lokal variabel utanför DEF')
        a = (self.b(hl) + 1) & 0xFF
        c = ((r >> 1) + 1) & 0xFF
        for _ in range(1000):
            a = (a - 1) & 0xFF
            if a != 0:
                hl += 1
                c = (c - 1) & 0xFF
                if c == 0:
                    return self.w(hl)
                s = self.b(hl) & 2
                hl += 1
                if not s:
                    continue
                a = (a + 1) & 0xFF
            a = (a - 1) & 0xFF                          # L3094
            if a < 0x80:
                continue
            hl += 2
        raise Fel('L307D')

    def namtx1(self, code):
        """NAMTX1: variabelns namn ur koden. (kod >> 3) - $3FF >= 0: ett
        långt namn i listan före VARBEG; annars bokstav, siffra (15:
        ingen) och $ (bit 1) eller % (bit 0)."""
        idx = (code >> 3) - 0x3FF
        if idx >= 0:
            v = self.varbeg - 2
            o = self.w(v)
            chunk = (v + (o - 0x10000 if o & 0x8000 else o)) & 0xFFFF
            p = chunk + 4 + idx
            t = bytearray()
            while True:
                c = self.b(p)
                if c == 0 or c >= 0x80:
                    break
                t.append(c)
                p += 1
            if not t:
                raise Fel('långt namn')
            self.put(t)
        else:
            self.put((code >> 8) | 0x40)
            d = (code >> 3) & 15
            if d != 15:
                self.put(d | 0x30)
        if code & 2:
            self.put(0x24)
        elif code & 1:
            self.put(0x25)

    def namtxt(self):                                   # NAMTXT
        self.namtx1(self.rdw())

    def s3de4(self, kind):                              # S3DE4, DVNAME
        c = self.ref(kind)
        if c:
            self.namtx1(c)

    def fname(self, chain, xname):
        """S3FBB: funktionens eller satsens namn ur kedjan."""
        a = self.rd()
        if a >= 0xF9:
            code = (a - 0xF8) << 8 | self.rd()
        else:
            code = a
        t = fndtab(chain, code)
        if t is None:
            t = xname + str(code).encode()
        self.put(t)

    # uttrycken
    def decomp(self):
        """DECOMP: uttryckets kod (omvänd polsk notation) -> text. Varje
        operand är en textbit; fs har deras början."""
        while True:
            a = self.rd()
            if a >= 0xDD:                               # DSMALL: 0-34
                self.push()
                self.put(b'%d%%' % (a - 0xDD))
            elif a >= 0xD5:                             # DNUM
                self.push()
                n = a - 0xD4
                code = bytes(self.b(self.hl + i) for i in range(n))
                self.hl += n
                self.put(ftodec(code.ljust(8, b'\0')))
            elif a >= 0xD0:                             # DFUNC
                n = a - 0xD0
                if n >= 4:
                    n = (self.rd() - 0xD0) & 0xFF
                if n:
                    self.dcomma(n)
                    self.push()
                    self.fname(FNKWP, b'XFN')
                    self.paren()
                else:
                    self.push()
                    self.fname(FNKWP, b'XFN')
            elif a >= 0xA7:
                if self.dectab(a):
                    return
            elif a >= 0x89:                             # DOP
                self.dopr(a, 0)
            elif a >= 0x84:                             # DOPSP
                self.dopr(a, 1)

    def paren(self):                                    # L3DB5
        self.put(0x28)
        self.dmerge()
        self.pop()
        self.put(0x29)

    def dopr(self, op, flags):
        """DOPR: operatorn sist och flyttad före den högra operanden.
        flags bit 0: blanksteg runt, bit 1: en operand (NOT)."""
        bc = self.fs[-1] if self.fs else 0
        self.push()
        if flags & 1 and bc > 0 and self.buf[bc - 1] != 0x20:
            self.put(0x20)
        self.put(optext(op))
        if flags & 1 and bc < len(self.buf) and self.buf[bc] != 0x20:
            self.put(0x20)
        self.dmerge()
        self.pop()
        if not flags & 2:
            self.pop()

    def dectab(self, a):
        """DECTAB, $A7-$CF. Svaret är sant när uttrycket är slut."""
        if a == 0xA7:                                   # DPAREN
            self.push()
            self.paren()
        elif a in (0xA8, 0xA9):                         # DNEG
            self.push()
            self.put(0x2D)
            self.dmerge()
            self.pop()
        elif 0xAA <= a <= 0xAE:                         # DSKIP
            self.hl += 1
        elif a == 0xAF:                                 # DNOT
            self.dopr(a, 3)
        elif a in (0xB0, 0xC3):                         # DEMPTY
            self.fs.append(0)
        elif 0xB1 <= a <= 0xB3:                         # DASGN1
            self.insert(self.pop(), 0x3D)
            self.pop()
            return True
        elif 0xB4 <= a <= 0xB6:                         # DASGN
            self.asgn = self.w(self.hl)
            self.push()
            self.s3de4(0x0A)
            self.put(0x3D)
            self.dmerge()
            self.pop()
            self.pop()
            return True
        elif 0xB7 <= a <= 0xBC or a == 0xBF:            # DDROP
            self.pop()
            return True
        elif a == 0xBD:                                 # DTHEN
            self.dblank()
            self.put(kwfrom(THENTX))
            self.hl += 1
            self.pop()
            return True
        elif a == 0xBE:
            self.hl += 1
            self.pop()
            return True
        elif a in (0xC0, 0xC1, 0xC2, 0xC4, 0xC5, 0xC6):  # DVAR
            self.push()
            self.s3de4(0x0A)
        elif a == 0xC7:                                 # DINT
            self.push()
            self.put(b'%d%%' % self.rdw())
        elif a in (0xC8, 0xC9, 0xCA):                   # DARR
            self.dcomma(self.rd())
            self.push()
            self.s3de4(0x0A)
            self.paren()
        elif a in (0xCB, 0xCC):                         # DSTR, DSTR2
            q = 0x22 if a == 0xCB else 0x27
            self.push()
            n = self.rd()
            self.put(q)
            for _ in range(n):
                c = self.rd()
                self.put(c)
                if c == q:
                    self.put(q)
            self.put(q)
        elif a in (0xCD, 0xCE):                         # DDIM, DCOMMN
            return self.ddim(a == 0xCE)
        elif a == 0xCF:                                 # DFN
            n = self.rd()
            self.hl += 1
            if n:
                self.dcomma(n)
            self.push()
            self.put(b'FN')
            self.s3de4(0x0B)
            if n:
                self.paren()
        return False

    def ddim(self, common):
        """DDIM, DCOMMN: en post i DIM eller COMMON. Svaret är sant när
        listan är slut."""
        while True:
            typ = self.rd()
            if not typ & 2:
                self.fs.append(0)                       # ingen storlek
            self.push()
            self.put(0x29)
            arr = typ & 8
            if not arr:
                self.buf.pop()
                self.s3de4(0x0A)
            ix = self.pop()
            bc = self.pop()
            if bc:                                      # =storleken
                self.fs += [bc, ix]
                self.put(0x3D)
                self.dmerge()
                self.pop()
                self.pop()
            if arr:
                n = self.rd()
                while True:
                    bc = self.pop()
                    if bc:
                        self.insert(bc, 0x3A)           # undre:övre
                    n -= 1
                    if n <= 0:
                        break
                    self.insert(self.pop(), 0x2C)
                self.push()
                self.s3de4(0x0A)
                self.put(0x28)
                self.dmerge()
                self.pop()
                self.pop()
            if common:
                self.hl += 2
            if self.b(self.hl) < 0xC0:
                return True
            self.put(0x2C)
            # nästa post: DECOMP läser vidare och kommer tillbaka hit
            return False

    # satserna
    def s422e(self, n, c):
        """DNAMES: n namn med komma emellan; c bit 1: strängarna med
        =storleken (LOCAL)."""
        for _ in range(n):
            a = self.b(self.hl)
            self.namtxt()
            if a & 2 and c & 2:
                self.put(b'=%d' % self.rdw())
            self.put(0x2C)
        if n:
            self.buf.pop()

    def dtext(self):                                    # DTEXT
        n = self.rd()
        t = bytes(self.b(self.hl + i) for i in range(n))
        self.hl += n
        self.put(t[:-1])

    def dline(self, p):
        """DLINE: raden vid p -> text som slutar med CR."""
        self.hl = p + 1
        self.buf = bytearray()
        self.fs = []
        B = self.level
        C = B
        self.put(b'%d' % self.rdw())
        start = len(self.buf)
        self.dblank()
        while True:                                     # L40EE
            a = self.b(self.hl)
            if a == 0xBC:                               # FOR: länken till NEXT
                ln = self.w(self.hl + 1)
                if ln and self.asgn is not None:
                    self.fornext[(self.hl + 3 + ln - 1) & 0xFFFF] = self.asgn
                self.hl += 3
                continue
            if a == 0x87:                               # radslut
                self.hl += 1
                break
            if a >= 0xC0:
                self.decomp()
                if self.b(self.hl) >= 0xC0:
                    self.dblank()
                continue
            if a < 0x80:
                self.put(a)
                self.hl += 1
                continue
            if a == 0xB7:                               # DVARS
                self.hl += 1
                n = 1
                while not self.rd() & 4:
                    n += 1
                while True:
                    self.decomp()
                    n -= 1
                    if n or self.b(self.hl) == 0xB7:
                        self.put(0x2C)
                    if not n:
                        break
                continue
            # ett nyckelord
            self.dblank()
            a = self.rd()
            b = 1
            if a == 0xBF:                               # ON: antal och kod
                b = self.rd()
                a = self.rd()
            c = a
            if 0x9D <= a < 0xA0:
                a = (a + 0xE6) & 0xFF
            if a == 0x86:
                self.fname(STKWP, b'XSTM')
            else:
                self.put(kwtext(a, STKW))
            self.dblank()
            if c < 0x86:                                # DLNUMS
                for i in range(b):
                    if i:
                        self.put(0x2C)
                    self.put(b'%d' % self.ref(9))
                continue
            if c == 0x8C:                               # DEF FN
                self.buf.pop()
                self.hl += 2
                self.namtxt()
                self.hl += 2
                self.lnkdef = self.hl
                n = self.rd()
                if n:
                    self.put(0x28)
                    self.s422e(n, 0)
                    self.put(0x29)
                B += 1
                if self.b(self.hl) < 0xC0:
                    continue
                self.put(0x3D)
                self.decomp()
                self.lnkdef = 0
                B, C = self.dedent(B, C)
            elif c == 0x8A:                             # ELSE
                self.hl += 1
            elif c == 0xB6:                             # LOCAL
                self.s422e(self.rd(), 2)
            elif c in (0x8F, 0x90):                     # REM, !
                self.dtext()
                break
            elif c == 0x8E:                             # DATA
                self.hl += 2
                self.dtext()
                break
            elif c == 0x9A:                             # WHILE
                self.hl += 2
                B += 1
                self.dblank()
            elif c == 0x9B:                             # WEND
                self.hl += 2
                B, C = self.dedent(B, C)
            elif c == 0x8D:                             # FNEND
                self.lnkdef = 0
                B, C = self.dedent(B, C)
            elif c == 0x98:                             # FOR
                B += 1
                self.dblank()
            elif c == 0x99:                             # NEXT
                self.s3de4(7)
                B, C = self.dedent(B, C)
            elif c == 0xA2:                             # MID$
                self.buf.pop()
                self.decomp()
                if self.b(self.hl) >= 0xC0:
                    self.dblank()
            elif c == 0x8B:                             # ?
                del self.buf[-2]
                self.dtext()
                break
        self.level = B
        if C:
            self.buf[start:start] = bytes([self.buf[start]]) * (2 * C)
        return bytes(self.buf) + b'\r'

    def dedent(self, B, C):                             # DDEDNT
        if B:
            B -= 1
            C = min(C, B)
        self.dblank()
        return B, C


def list2(data, namn, fel):
    """BASIC II: raderna från $15 till längden 1."""
    try:
        p = Basic2(data)
    except IndexError:
        fel(f'{namn}: för kort')
        return b''
    ut = bytearray()
    i = 0x15
    while True:
        if i >= len(p.m):
            fel(f'{namn}: inget slutmärke (längden 1)')
            break
        L = p.m[i]
        if L == 1:
            break
        if L < 4:
            fel(f'{namn}: fel radlängd {L} vid ${i:04X}')
            break
        nr = p.w(i + 1)
        try:
            ut += p.dline(i)
        except Fel as e:
            fel(f'{namn}: rad {nr} gick inte att lista ({e})')
            ut += b'%d REM *** RADEN KUNDE INTE LISTAS ***\r' % nr
        i += L
    return bytes(ut)


# ==================================================================

def bac2bas(data, namn='BAC', fel=None):
    """BAC-filen -> text med CR (7-bitarskod), som LIST skriver den."""
    if fel is None:
        def fel(t, varning=False):
            print('bac2bas: ' + t, file=sys.stderr)
    if not data:
        fel(f'{namn}: tom')
        return b''
    if data[0] == 0x8F:
        return list2(data, namn, fel)
    if data[0] != 0x82:
        fel(f'{namn}: börjar inte med $82 eller $8F, ingen BAC-fil?', varning=True)
    return list80(data, namn, fel)


def main():
    ap = argparse.ArgumentParser(
        prog='bac2bas',
        description='BAC-fil -> BASIC-text, som LIST skriver den (ABC80 och BASIC II).')
    ap.add_argument('--kod', choices=abckas.KODER, default='utf8',
                    help='teckenkod: 7 (ABC), dos (850) eller utf8 (standard)')
    ap.add_argument('--rad', choices=sorted(abckas.RADER), default='lf',
                    help='radslut: cr, crlf eller lf (standard)')
    ap.add_argument('-o', metavar='UT', help='skriv hit i stället för stdout')
    ap.add_argument('fil', nargs='*', help='BAC-filer (inga: stdin)')
    a = ap.parse_args()

    fels = 0

    def fel(t, varning=False):
        nonlocal fels
        print('bac2bas: ' + t, file=sys.stderr)
        if not varning:
            fels += 1

    ut = bytearray()
    for f in a.fil or ['-']:
        if f == '-':
            data, namn = sys.stdin.buffer.read(), 'stdin'
        else:
            try:
                data = open(f, 'rb').read()
            except OSError as e:
                fel(f'{f}: {e.strerror}')
                continue
            namn = f
        ut += abckas.text_ut(bac2bas(data, namn, fel), a.kod, a.rad)
    if a.o:
        open(a.o, 'wb').write(ut)
    else:
        sys.stdout.buffer.write(ut)
    return 1 if fels else 0


# ROM:en i ABC 802: $0600-$0FFF (nyckelord, operatorer, funktioner; en
# okänd kod letas som i ROM:en vidare i koden efter tabellerna),
# $4BF7-$4C08 (satsernas kedja), $4DF2-$4F29 (tiopotenserna), $6F87-$6F99
# (DOS-satserna BYE KILL NAME). DOSINI lägger en nod för dem vid $FDD8
# (DOSSTH) först i STKWP-kedjan; nästa nod är den gamla, $4BF7.
for _a, _h in (
    (0xFDD8, ('f74ba004816f876f',)),
    (0x6F87, ('80425945814b494c4c824e414d45ff834153ff',)),
    (0x0600, (
        '000000000000000000002506032a06032f06033706034106825106815606015b',
        '060400000084455156ff85494d50ff864f5287584f52ff88414e44ffaf4e4f54',
        'ff983c3d8c3c3e8f3c923e3d953e893dff9b2b9e2dffa02aa22fffa45ea42a2a',
        'ff6b06802000001408800000000036000075069e07fe58464e009a4142538641',
        '544e84434f53a34558508146495882494e54a54c4f473130a44c4f4788504989',
        '524e448a53474e8353494e875351528554414e8b41534349498b4153438c4348',
        '52248d4c454654248d4c4546548e5249474854248e52494748548f4d4944248f',
        '4d4944904c454e91494e5354529253504143452493535452494e47249c4e554d',
        '24a756414c945357415025ab564152505452ac5641524f4f54ad5045454b32ae',
        '4d4f44af4356542524b04356542425b14356544624b24356542446b348455824',
        'b44f435424955045454b96494e509743414c4c98455252434f44459d5441429f',
        '435552a15458504f494e549b414444249e53554224a04d554c24a244495624a6',
        '434f4d502599535953a954494d4524aa504f534954b5444f5480464e00ff9a86',
        '84a38182a4a5838785002088208920208a01208b90b001228c02298d8e020221',
        '8f0202012191010102229d92afb3b402219f930201219cb10220a7b2002295ad',
        '9496990121ae0101219821a1b5010121a980229b9ea0a202060221a605062297',
        '012121aa0021abac0170a8a802120121ff292d6c815245448247524e8359454c',
        '84424c55854d4147864359418757485488464c534889535444598c4e524d4c8d',
        '44424c459147524544924747524e934759454c9447424c5595474d4147964743',
        '5941974757485498484944459947434f4e9a475345509c424c42479d4e574247',
        '9e47484f4c9f4752454cff9508001900007e096c0a0000002500009f08000086',
        '5853544d00914c455492454e4480474f544f935052494e54943b9549468f5245',
        '4d902197494e505554204c494e4596494e50555498464f52994e4558549a5748',
        '494c459b57454e449c5245414483524553544f524584524553554d4582474f53',
        '5542a052455455524e854f4e204552524f5220474f544fa14f4e8e444154418c',
        '44454620464e008d464e454e44a24d494424a24d4944a353494e474c45a4444f',
        '55424c45ff818b3f883a8a454c5345ffbe53544550ffbd544fffbb4153204649',
        '4c45ffb9434f554e54ffb85553494e47ffb64c4f43414cffbd5448454eff8044',
        '494d81434f4d4d4f4e82504f4b45834f5554844f50454e855052455041524586',
        '434c4f53458752414e444f4d495a458853544f50895458504f494e548a474554',
        '8b434841494e8c54524143458d4e4f2054524143458e5055548f494e54454745',
        '5290464c4f415491455854454e44924e4f20455854454e449344494749545394',
        '4f5054494f4e204241534595504f5349549653455420444f5497434c5220444f',
        '54ff401940194019ff19ff19ff19081b000000000000000000007e1a4f19f219',
        'f419f4191b16a419401b401b57199c1b041c0a1a3e1a471a411aed1b00000000',
        '00005019b619f615363a393a4a164d16601a6a1a421c421c5d1c4f194f19771a',
        '111c1b002d1c4f190a1c1c3a203a2c3a303a08160816261c4c1a4c1a251d251d',
        'e52b7728353cc42bf71cea1c4f191201151daf1d211d952b131d151d151d311d',
        'f11cb11db11d311db11de0383227b8275828251d9f286828263cd92bf72b162c',
        '9d2b4f194f19311d311df039023a73387538be38ec348e3b95041939442c9b2c',
        'b21f41394f194f194f194f196b3a713a5d397d048304a6ff9c225922d5221121',
        '1f2199446d21792185219d218f21c820a721b925c820b221b925c820be21b925',
        'c820ca21b925c820da21b925c820d621b9252721de2124252721e3212721ed21',
        '2721ec21b22d122d5c2d311daa206721bb2abb2abb2a10211e219921b8248422',
        '5122c5225f224722a322072b352b4a2b77227d227a221a1d1a1d4f1994225522',
        'd122dc224222422242225f21e122e122e122bf24bf245b245c24cc29b32cb72c',
        '3421342134213421342198226634b120d2249125eb240325e124cb241c268326',
        '8826f7215521cf39db3951212c3ba5200a443421661f0a44c4260a4489040a44',
        '34213421342113441344e124e23aa939bc36c9365922e9210737e2360337de36',
        '123715377504f34cc04c0000805684568856505b895ae820e820e820f020f020',
        'f0200621062106210b210b210b21f820f820f820ff20ff20ff20414fd35c5044',
        '394fcb5cea4feb5d5f508a5e37592858605aa4443b2100000000000000000000',
        'f34cc04c2a08ff462b4edd216c0dcdc50cdd21ec0c012c00fdcb269ecdc20ccd',
        'ad0ced530affc911ffffcd9d0e014000dd21560dfd362000cdc20ccda210d0c3',
        '1200fde5e1094e234678b1c8eb2bed420923eb300b78fdbe0f300536002b3600',
        '6069e5d5cd5d35d1e130dbc9d823237e4f2323e60620170602cb412007fdcb26',
        'defd461ecd6d0dc8af772310fcc9e5f5cd330df1e1ee06c04e234679b0c8237e',
        '23666f09e5ed42c1a7ed4209d0c5cd390d18f4afcd6d0d2019e523234e234623',
        '232378b1280bebed4209eb38042802f1c9e1060618b2e5dde3380bcd6d0d2809',
        'ddcb034e2003cd7610dde1a7c9fdcb1d5ec8fdcb1d66c0d5eb2a30ffed52ebd1',
        '9fa7c9cd3f00cde926fdcb1da6cd8b0ecdc50eebcd0e0efdcb2646c2840ccd95',
        '0efdcb266e206fd5ed5b08ffd513cd840ced5b06ffe1a7ed52444d0319ed5b0e',
        'ff1bcd860eedb813ed5306ffd1fd7e26e644f620cd543aed530affed532effc5',
        'd5e52a0affcda60eeb2a06ff01060009cdf531ebcdb50eebcd9e30cdf5312004',
        'ed5332ffed5b32ffcdf531e1d1c1affd7727fd7722c9cd840c18b2cd910ccdcc',
        '3318b4cd9d0ecd660ffdcb264ec4330fcd8b0ecd910cfdcb238ed521910f011d',
        '00c3bd0dfdcb2646c4b82ecd950e2a08ffeb13fdcb266e28c2ebcd910cd5cd2a',
        '34d97aa33c280bcd3b0019ebcd730ecdcc33d1d5cd660fd1fdcb26aeed5306ff',
        'edb0cdd70d1bed5308ffc9ed5b30fffdcb1d66280ced5b0cffed5330fffdcb1d',
        'e6fdcb1ddec9ed4b2effa7ed42d009cdb80ecbfcc92a30ffed4b0cffa7ed42c9',
        'ebcda030eb2a2eff09cb78c8c5cbb82a0cff09c10420082a18ff097e23666f05',
        'c9c52a0affcda60ec13ecd37c0cd100fd8e5d5192230ffebcdd70dd12a32ff29',
        'ed5a3001232232ffafe1c9cde10e18233ec9cd2c0f19d8d511640019d1d8ed72',
        '3fd8cd2c0fe519220affe1c92a0affc9cd100fd87ab3c8c5d5e5424b545d0b13',
        '360078b12802edb0e1d1c1c9e5210400391801e5d5119cff197dfd960a7cfd9e',
        '0bd1e13ec9c9ed5b06ff2a08ff23a7ed52444debc98fff00001d000000000000',
        '000000000000000000870100000000fffbfff57e23fe222807fe2728032b3e2c',
        '470effe57e230cb82805fe0d20f62b06002bdfebe1f1d5cdbc0fe1c9eb21f2ff',
        '39f9ebf5d5cd8c4ada6010ed62dd75031e03cd68492bcd7e4acd854ad12a7bff',
        '7cb5287ccda030c5d5eb010b000906031aa72803be2008231310f5ebcd2b27d1',
    )),
    (0x4BF7, (
        '8b0800900a4c014c084c805749445448ff0c',
    )),
    (0x4DF2, (
        '8100000000000000842000000000000087480000000000008a7a000000000000',
        '8e1c400000000000914350000000000094742400000000009818968000000000',
        '9b3ebc20000000009e6e6b2800000000a21502f900000000a53a43b740000000',
        'a868d4a510000000ac1184e72a000000af35e620f4800000b2635fa931a00000',
        'b60e1bc9bf040000b931a2bc2ec50000bc5e0b6b3a764000c00ac7230489e800',
        'c32d78ebc5ac6200c658d726b7177a80ca078678326eac90cd2968163f0a57b4',
        'd053c21bcecceda1d404595161401485d7256fa5b99019a6da4ecb8f27f4200f',
        'de013f3978f8940ae1218f07d736b90ce449f2c9cd04674fe77c6f7c40458123',
        'eb1dc5ada82b70b6ee45371912364ce3f17684df56c3e01cf51a130b963a6c11',
        'f84097ce7bc90716fb70bdc21abb48dbff16769950b50d89',
    )),
):
    for _i, _c in enumerate(bytes.fromhex(''.join(_h))):
        ROM[_a + _i] = _c

if __name__ == '__main__':
    sys.exit(main())
