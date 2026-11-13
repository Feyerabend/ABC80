#!/usr/bin/env python3
"""hlp.py: gör VERK.HLP:s data av verk.hlp, introduktionen i VERK.

    python3 hlp.py verk.hlp verkhlp.adr verkhlp.bin

verk.hlp är text i UTF-8. Å Ä Ö É Ü å ä ö é ü blir ABC 802:s koder
(som emulatorns -f), ett radslut 13 och {TS} kod 1, där VERK skriver
hur många tecken texten får vara. Sist 0 och en RET (startadressen när
DRUN laddar filen). verkhlp.adr har adressen till PB, dit filen läses
(kabs.py skriver den); hlp.py skriver adressen och startadressen
tillbaka till verkhlp.adr, för abs.py.
"""
import sys

kalla, adr, ut = sys.argv[1:4]
SV = {'Å': 0x5D, 'Ä': 0x5B, 'Ö': 0x5C, 'É': 0x40, 'Ü': 0x5E,
      'å': 0x7D, 'ä': 0x7B, 'ö': 0x7C, 'é': 0x60, 'ü': 0x7E}
text = open(kalla, encoding='utf-8').read().rstrip('\n')
text = text.replace('{TS}', '\x01')
data = bytearray()
for t in text:
    if t == '\n':
        data.append(13)
    elif t in SV:
        data.append(SV[t])
    elif t == '\x01' or 32 <= ord(t) < 127:
        data.append(ord(t))
    else:
        sys.exit('hlp.py: tecknet %r går inte' % t)
data += b'\x00'
if len(data) > 1900:
    sys.exit('hlp.py: texten är %d bytes, PB tar 1920' % len(data))
pb = int(open(adr).read().split()[0], 16)
open(ut, 'wb').write(data + b'\xc9')
open(adr, 'w').write('%04X %04X\n' % (pb, pb + len(data)))
print('VERK.HLP: introduktionen, %d bytes på $%04X' % (len(data), pb))
