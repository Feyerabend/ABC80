#!/usr/bin/env python3
"""kabs.py: gör KALKYL.ABS av minnet efter att kalkyl.fs är inskrivet.

    python3 kabs.py kalkyl.mem D800 kalkyl.fs forth.lst kalkyl.bin
    python3 kabs.py verk.mem ED00 verkin.fs forth.lst verk.bin VERK

kalkyl.mem är emulatorns RAM (abc802 -M). Bilden är $8000 till HERE
(variabeln DP), med orden, cellerna och variablerna som de är efter
inskrivningen. LADDAD får den nya gränsen mot BASIC (TIB:s tal är redan
ändrat i Forth, före inskrivningen). Alla namn som kalkyl.fs definierar
ska finnas i ordlistan: ett fel i en definition (ett okänt ord) gör att
den aldrig blir synlig. Orden i överläggen har egna kedjor: överlägg 1
från OVLST (det ligger i området på OVB) och överlägg 2 från OV2LST (för
KALKYL på RAM-skivan, som skrivs till kalkylovl.bin (KALKYL.OVL) med
adresserna i kalkylovl.adr, och för VERK i bitarna i OVBIT); VERK har också
överlägg 3 från OV3LST, i bitarna efter överlägg 2, och sitt eget
överlägg 2 i stället för KALKYL:s, som därför inte provas, och
överlägg 4 från OV4LST, på RAM-skivan (bytes 0-$7FFF i minnet), som
skrivs till verkovl.bin (VERK.OVL) med adresserna i verkovl.adr.
Med HLPN (VERK) skrivs PB:s adress i verkhlp.adr, för hlp.py.
Ingång 1 (QUIT) får OVSTART, som lägger överläggen på
RAM-skivan första gången. Med ett sjätte argument får ingång 1 det
ordet i stället (VERK), och utan OVB i ordlistan finns det inga överlägg.
"""
import os
import re
import sys

mem, upp, kalla, lst, ut = sys.argv[1:6]
start = sys.argv[6] if len(sys.argv) > 6 else 'OVSTART'
prog = os.path.basename(ut).split('.')[0].upper()
ram = bytearray(open(mem, 'rb').read())
upp = int(upp, 16)
etiketter = {}
for rad in open(lst, encoding='latin-1'):
    m = re.match(r'([0-9A-F]{4}) .*?\b(\w+):', rad)
    if m:
        etiketter.setdefault(m.group(2), int(m.group(1), 16))
    m = re.match(r'[0-9A-F]{4}  21 (..) (..) .*LD +HL,ENTRY', rad)
    if m:                                 # ENTRY: står på en egen rad
        etiketter['ENTRY'] = int(m.group(2) + m.group(1), 16)


def ord16(a):
    return ram[a] | ram[a + 1] << 8


here = ord16(etiketter['V_DP'])
sist = ord16(etiketter['V_LAST'])
namn = {}


def kedja(h, bild=None, lo=0, hi=0):
    """Huvudena från h; adresser i lo-hi läses i bild (en kopia)."""
    while h:
        if bild is not None and not lo <= h < hi:
            return
        a = h - lo + bild if bild is not None else h
        n = ram[a + 2] & 31
        namn.setdefault(ram[a + 3:a + 3 + n].decode('latin-1'), h + 3 + n)
        h = ram[a] | ram[a + 1] << 8


kedja(sist)


def var(n):
    return ord16(namn[n] + 3)


ov = 'OVB' in namn
if ov:
    ovb, ovimg, ovt = var('OVB'), var('OVIMG'), namn['OVT'] + 3
    kedja(var('OVLST'))
    n2 = ord16(ovt + 8)
    n3 = 0
    if 'OVBIT' in namn:                   # VERK: 2 och 3 i OVSN bitar
        b = namn['OVBIT'] + 3
        ovimg = len(ram)
        for i in range(var('OVSN')):
            a, n = ord16(b + 4 * i), ord16(b + 4 * i + 2)
            ram += ram[a:a + n]
        n3 = ord16(ovt + 14)
        if 'OV3R' in namn and n3 > var('OV3R'):
            sys.exit('kabs.py: överlägg 3 är %d bytes, OV3R %d'
                     % (n3, var('OV3R')))
        kedja(var('OV3LST'), ovimg + n2, ovb, ovb + n3)
    elif 'KOVLN' in namn:                 # KALKYL: 2 på RAM-skivan
        a2, s2 = ord16(ovt + 6), ord16(ovt + 10)
        ov2 = bytes(ram[a2:a2 + n2])
        if sum(ov2) & 0xFFFF != s2:
            sys.exit('kabs.py: överlägg 2 på RAM-skivan har fel summa')
        if ov2[-1] != 0xC9:
            sys.exit('kabs.py: överlägg 2 slutar inte med RET')
        ovimg = len(ram)
        ram += ov2
        if ord16(ovt) < 16448:
            sys.exit('kabs.py: överlägg 1 på RAM-skivan på $%04X når '
                     'omgångarna' % ord16(ovt))
    kedja(var('OV2LST'), ovimg, ovb, ovb + n2)
n4 = 0
if ov and 'OV4LST' in namn:               # VERK: 4 på RAM-skivan
    ts, n4, s4 = ord16(ovt + 18), ord16(ovt + 20), ord16(ovt + 22)
    if n4 + 1 > var('OV4R'):
        sys.exit('kabs.py: överlägg 4 är %d bytes, OV4R %d'
                 % (n4, var('OV4R')))
    ov4 = bytes(ram[ts:ts + n4])
    if sum(ov4) & 0xFFFF != s4:
        sys.exit('kabs.py: överlägg 4 på RAM-skivan har fel summa')
    ram += ov4
    kedja(var('OV4LST'), len(ram) - n4, ovb, ovb + n4)
    if not var('OVOK'):
        sys.exit('kabs.py: överlägg 2 och 3 får inte plats i bitarna')

text = open(kalla, encoding='utf-8').read()
if ov and 'OVBIT' in namn:                # VERK kompilerar om överlägg 2
    text = re.sub(r'---- Överlägg 2: VISA.*?2 OVSPAR', ' ', text, count=1,
                  flags=re.S)
text = re.sub(r'(^|\s)\(\s[^)\n]*\)', ' ', text)
text = re.sub(r'(^|\s)\.\"\s[^"\n]*"', ' ', text)       # ." text"
ord_ = text.split()
saknas = [ord_[i + 1] for i, w in enumerate(ord_[:-1])
          if w in (':', 'CONSTANT', 'VARIABLE', 'CREATE')
          and ord_[i + 1] not in namn]
kbas = var('KBAS') if ov else here
if saknas:
    sys.exit('kabs.py: ordlistan saknar ' + ' '.join(saknas))
if not 0x8000 < here < upp - 80:
    sys.exit('kabs.py: HERE = $%04X' % here)
entry = etiketter['ENTRY']
ram[entry + 2:entry + 4] = bytes([namn[start] & 255, namn[start] >> 8])
laddad = etiketter['LADDAD']
assert ram[laddad] == 0x21            # LD HL,UPP
ram[laddad + 1:laddad + 3] = bytes([upp & 255, upp >> 8])
open(ut, 'wb').write(ram[0x8000:here])
if not ov:
    print('%s.ABS: $8000-$%04X, %d bytes, %d bytes fria till $%04X'
          % (prog, here - 1, here - 0x8000, upp - 80 - here, upp - 80))
    sys.exit()
n1 = ord16(ovt + 2)
print('%s.ABS: $8000-$%04X, %d bytes; överläggen %s bytes på '
      '$%04X; i gång $8000-$%04X, %d bytes fria till $%04X'
      % (prog, here - 1, here - 0x8000,
         ', '.join(str(n) for n in (n1, n2, n3, n4) if n), ovb, kbas - 1,
         upp - 80 - kbas, upp - 80))
if 'HLPN' in namn:                        # VERK.HLP läses till PB
    open(os.path.splitext(ut)[0] + 'hlp.adr', 'w').write(
        '%04X\n' % (namn['PB'] + 3))
if ov and 'KOVLN' in namn:
    bas = os.path.splitext(ut)[0]
    open(bas + 'ovl.bin', 'wb').write(ov2)
    open(bas + 'ovl.adr', 'w').write('%04X %04X\n' % (ovb, ovb + n2 - 1))
    print('%s.OVL: överlägg 2, %d bytes på $%04X, på RAM-skivan $%04X'
          % (prog, n2, ovb, a2))
if n4:
    bas = os.path.splitext(ut)[0]
    open(bas + 'ovl.bin', 'wb').write(ov4 + b'\xc9')     # RET
    open(bas + 'ovl.adr', 'w').write('%04X %04X\n' % (ovb, ovb + n4))
    print('%s.OVL: överlägg 4, %d bytes på $%04X, på RAM-skivan $%04X'
          % (prog, n4, ovb, ts))
