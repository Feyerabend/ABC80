#!/usr/bin/env python3
"""Laddprogram i BASIC för exempel 7.

    python3 e7bas.py
    python3 e7bas.py -k     skriv e7i.bas som tangenter för abc80host

Läser e7i.bin (tolken på $C000, assemblerad av test.sh) och skriver
e7i.bas: ett ABC80-BASIC-program som lägger bilden i minnet med POKE.
Varje DATA-rad har 30 bytes i hex och summan av dem; en rad som är
felskriven ger "FEL I RAD n" innan något av den har lagts i minnet
(raden läses två gånger). BOFA måste redan ha flyttats upp (POKE
65052,0,208 och NEW), så att programmet självt ligger ovanför $D000;
annars skriver det det i stället för att lägga bilden över sig självt.
"""
import sys

ORG, BIN, OUT, N = 0xC000, "e7i.bin", "e7i.bas", 30

PROG = """\
10 REM FORTH, EXEMPEL 7
15 IF PEEK(65053)<208 THEN 270
20 A=%d
30 L=1000
40 READ H$,S
50 IF H$="*" THEN 250
60 T=0
70 FOR P=0 TO 1
80 FOR I=1 TO LEN(H$) STEP 2
90 H=ASC(MID$(H$,I,1))-48
100 IF H>9 THEN H=H-7
110 K=ASC(MID$(H$,I+1,1))-48
120 IF K>9 THEN K=K-7
130 IF P=1 THEN 160
140 T=T+16*H+K
150 GOTO 180
160 POKE A,16*H+K
170 A=A+1
180 NEXT I
190 IF T<>S THEN 230
200 NEXT P
210 L=L+10
220 GOTO 40
230 PRINT "FEL I RAD";L
240 END
250 PRINT A-%d;" BYTES"
260 END
270 PRINT "SKRIV POKE 65052,0,208 OCH NEW"
280 END
""" % (ORG, ORG)

data = open(BIN, "rb").read()
lines = [PROG]
for k in range(0, len(data), N):
    b = data[k:k + N]
    lines.append('%d DATA "%s",%d\n'
                 % (1000 + k // N * 10, b.hex().upper(), sum(b)))
lines.append('%d DATA "*",0\n' % (1000 + (len(data) + N - 1) // N * 10))
text = "".join(lines)
if sys.argv[1:] == ["-k"]:
    print("".join(l + "\\r\\w20\\." for l in text.split("\n") if l), end="")
else:
    open(OUT, "w").write(text)
