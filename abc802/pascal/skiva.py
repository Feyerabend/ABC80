#!/usr/bin/env python3
"""skiva.py: filerna för pvm.asm på en MF-skiva (320K, kort 44), i
samma format som pvm.asm läser och skriver (se pvm.asm, Filerna).

    python3 skiva.py put skiva.img värdfil NAMN.EXT [-t]
    python3 skiva.py get skiva.img NAMN.EXT värdfil [-t]

put lägger värdfilen på skivan (som måste ha plats för den och inte
redan ha namnet); get hämtar den. Med -t är filen text: på värden UTF-8
med LF, på skivan ABC 802:s teckensats (å ä ö Å Ä Ö som } { | ] [ \\)
med CR som radslut.

På skivan är filen poster om 253 bytes, en per sektor efter
indexsektorn, som DOS:ens DRDREC läser (som BASICINI.SYS i abs.py):
data på plats 3-254 och ETX (3) på 255. Där datan slutar står 0. Byten
0, 3 och 16 skrivs som 16 och byten + 64. En fil som inte ryms i 32
kluster får flera utsträckningar i indexsektorn.
"""
import sys

TILL_ABC = {'å': '}', 'ä': '{', 'ö': '|', 'Å': ']', 'Ä': '[', 'Ö': '\\',
            'é': '`', 'ü': '~', 'Ü': '^'}
FRAN_ABC = {v: k for k, v in TILL_ABC.items() if k not in 'éüÜ'}


def till_abc(b):
    t = b.decode('utf-8').replace('\r\n', '\n').replace('\n', '\r')
    return ''.join(TILL_ABC.get(c, c) for c in t).encode('latin-1')


def fran_abc(b):
    t = b.decode('latin-1').replace('\r\n', '\n').replace('\r', '\n')
    return ''.join(FRAN_ABC.get(c, c) for c in t).encode('utf-8')


def koda(data):
    ut = bytearray()
    for c in data:
        if c in (0, 3, 16):
            ut += bytes([16, c + 64])
        else:
            ut.append(c)
    ut.append(0)
    return ut


def avkoda(strom):
    ut, i = bytearray(), 0
    while i < len(strom):
        c = strom[i]
        if c == 0:
            return ut
        if c == 16:
            i += 1
            c = strom[i] - 64
        ut.append(c)
        i += 1
    raise SystemExit('skiva.py: filen slutar utan 0')


def namn11(namn):
    n, e = namn.upper().split('.') if '.' in namn else (namn.upper(), '')
    assert len(n) <= 8 and len(e) <= 3, namn
    return (n.ljust(8) + e.ljust(3)).encode('latin-1')


class Skiva:
    def __init__(self, fil):
        self.mf = bytearray(open(fil, 'rb').read())

    def sektor(self, n):
        return self.mf[n * 256:n * 256 + 256]

    def satt(self, n, d):
        self.mf[n * 256:n * 256 + 256] = bytes(d).ljust(256, b'\xff')

    def hitta(self, nm):
        for ks in range(16):
            kat = self.sektor(16 + ks)
            for k in range(16):
                e = kat[k * 16:k * 16 + 16]
                if e[4:15] == nm and e != b'\xff' * 16:
                    return ks, k, e
        return None

    def put(self, data, namn):
        nm = namn11(namn)
        assert self.hitta(nm) is None, 'skiva.py: %s finns redan' % namn
        strom = koda(data)
        poster = [strom[i:i + 252] + b'\x03'
                  for i in range(0, len(strom), 252)]
        for ks in range(16):
            kat = self.sektor(16 + ks)
            k = next((k for k in range(16)
                      if kat[k * 16:k * 16 + 16] == b'\xff' * 16), None)
            if k is not None:
                break
        dslot = k * 16 + ks
        bit = bytearray(self.sektor(14))
        nsek = len(poster) + 1
        kvar = (nsek + 3) // 4

        def ledig(c):
            return not bit[c >> 3] & (0x80 >> (c & 7))

        utstr, c = [], 8
        while kvar:
            while not ledig(c):
                c += 1
                assert c < 0xEF * 8, 'skiva.py: skivan är full'
            n = 0
            while n < kvar and n < 32 and c + n < 0xEF * 8 and ledig(c + n):
                n += 1
            utstr.append((c, n))
            for j in range(n):
                bit[(c + j) >> 3] |= 0x80 >> ((c + j) & 7)
            kvar -= n
            c += n
        assert len(utstr) <= 120
        bit[0xEF + ks] += 1
        index = bytearray([dslot, 0, 0, 0xFF])
        for c, n in utstr:
            u = (c << 5) | (n - 1)
            index += bytes([u >> 8, u & 255])
        index += b'\xff\xff'
        sek = [s for c, n in utstr for s in range(c * 4, (c + n) * 4)]
        self.satt(sek[0], index)
        for j, p in enumerate(poster, 1):
            self.satt(sek[j], bytes([dslot, j & 255, j >> 8]) + p)
        F = utstr[0][0] << 5
        e = bytes([F >> 8, F & 0xE0, nsek & 255, nsek >> 8]) + nm + b'\xff'
        n = (16 + ks) * 256 + k * 16
        self.mf[n:n + 16] = e
        self.mf[14 * 256:15 * 256] = bit
        return len(data), nsek, len(utstr)

    def get(self, namn):
        h = self.hitta(namn11(namn))
        assert h, 'skiva.py: %s finns inte' % namn
        ks, k, e = h
        F = e[0] << 8 | e[1]
        nsek = e[2] | e[3] << 8
        index = self.sektor((F >> 5) * 4)
        sek, i = [], 4
        while index[i] != 0xFF:
            u = index[i] << 8 | index[i + 1]
            c, n = u >> 5, (u & 31) + 1
            sek += range(c * 4, (c + n) * 4)
            i += 2
        strom = bytearray()
        for s in sek[1:nsek]:
            strom += self.sektor(s)[3:255]
        return avkoda(strom)

    def spara(self, fil):
        open(fil, 'wb').write(self.mf)


def main():
    a = [x for x in sys.argv[1:] if x != '-t']
    text = '-t' in sys.argv
    if len(a) != 4 or a[0] not in ('put', 'get'):
        raise SystemExit(__doc__)
    s = Skiva(a[1])
    if a[0] == 'put':
        data = open(a[2], 'rb').read()
        if text:
            data = till_abc(data)
        n, nsek, nu = s.put(data, a[3])
        s.spara(a[1])
        print('skiva.py: %s, %d bytes, %d sektorer, %d utsträckning%s'
              % (a[3].upper(), n, nsek, nu, '' if nu == 1 else 'ar'))
    else:
        data = s.get(a[2])
        if text:
            data = fran_abc(data)
        open(a[3], 'wb').write(data)


if __name__ == '__main__':
    main()
