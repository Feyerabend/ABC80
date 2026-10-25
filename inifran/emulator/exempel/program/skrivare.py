#!/usr/bin/env python3
"""Skriv ut ett BASIC II-program som lägger filer på MO0: med PUT.

    python3 skrivare.py NAMN=fil ... > skrivare.bas

Varje fil står i DATA-rader som hex, med Gnn för nn bytes $FF i rad.
Programmet bygger varje post på 253 bytes i en sträng och skriver den med
PUT (PRINT # skriver bara 252 bytes per sektor och sätter 03 sist), så
filen på skivan får samma data som arkivfilen, 253 bytes per post.
gor.sh lägger på det sättet ABC 800:s CMDINT.SYS och LIB.ABS på BASIC
II-skivan.
"""
import sys

SKRIVARE = [
    '5 DIM A$=80%,N$=20%,P$=253%',
    '10 READ N$ : IF N$="." THEN 999',
    '20 PREPARE "MO0:"+N$ AS FILE 1 : P$=""',
    '30 READ A$ : IF A$="." THEN 200',
    '40 I%=1%',
    '50 IF I%>LEN(A$) THEN 30',
    '60 V%=ASC(MID$(A$,I%,1%))-48% : IF V%>9% THEN V%=V%-7%',
    '70 W%=ASC(MID$(A$,I%+1%,1%))-48% : IF W%>9% THEN W%=W%-7%',
    '80 IF V%=16% THEN 120',
    '90 P$=P$+CHR$(16%*V%+W%) : GOSUB 300 : I%=I%+2% : GOTO 50',
    '120 B%=ASC(MID$(A$,I%+2%,1%))-48% : IF B%>9% THEN B%=B%-7%',
    '130 FOR J%=1% TO 16%*W%+B% : P$=P$+CHR$(255%) : GOSUB 300 : NEXT J%',
    '140 I%=I%+3% : GOTO 50',
    '200 IF LEN(P$)>0% THEN PUT #1,P$',
    '210 CLOSE : GOTO 10',
    '300 IF LEN(P$)=253% THEN PUT #1,P$ : P$=""',
    '310 RETURN',
    '999 END',
]


def koder(data):
    """Bytes som hex, med Gnn för nn bytes $FF i rad."""
    ut, i = [], 0
    while i < len(data):
        n = 1
        while data[i] == 0xFF and i + n < len(data) and data[i + n] == 0xFF and n < 255:
            n += 1
        if n > 2:
            ut.append("G%02X" % n)
        else:
            ut.append("%02X" % data[i])
            n = 1
        i += n
    return ut


def main():
    rader, nr = list(SKRIVARE), 1000
    for arg in sys.argv[1:]:
        namn, fil = arg.split("=", 1)
        with open(fil, "rb") as f:
            data = f.read()
        rader.append('%d DATA "%s"' % (nr, namn))
        nr += 10
        rad = ''
        for kod in koder(data) + [None]:
            if kod is None or len(rad) + len(kod) > 60:
                rader.append('%d DATA "%s"' % (nr, rad))
                nr += 10
                rad = ''
            if kod:
                rad += kod
        rader.append('%d DATA "."' % nr)
        nr += 10
    rader.append('%d DATA "."' % nr)
    sys.stdout.write(''.join(r + '\n' for r in rader))


if __name__ == "__main__":
    main()
