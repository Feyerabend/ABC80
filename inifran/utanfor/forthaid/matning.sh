#!/bin/sh
# Mäter FIND, VAR, REN och CHANGE i Forthaid och, om den finns i arkivet, i
# Smartaid 3 (dis/roms/smartaid3_dec.bin, med bitarna rättade), på
# program om ungefär 2, 8 och 12 KB, och skriver Forthaids storlek per
# kommando ur listningen. Utanför boken, se LASMIG.md.
#
# Tiden är den emulerade ABC80:s, inte emulatorns: körningen görs om med
# allt längre -t (tiden efter den sista tangenten, kommandots RETURN)
# tills kommandot syns klart på skärmen, och sedan halveras intervallet
# ned till 10 ms. Klart är för FIND raden med träffen (Smartaid 3 väntar
# därefter på mellanslag), för de andra ABC80 efter kommandot. VAR Z9
# finns bara i Forthaid, och CHANGE skrivs CHANGE B1 TO Q1 i Smartaid 3
# (OWOCO 1983); B1 står på var åttonde rad. Samma
# mätning på ett program med två rader dras ifrån, så att det som står
# är tiden för programmets längd. Programmen skrivs in med -K 20,20,
# och programmets storlek läses ur EOFA efteråt.
#
#     ./matning.sh          (9913; ./matning.sh 11273 för den gamla)
cd "$(dirname "$0")" || exit 1
EMU=../../emulator
[ -d $EMU/karna ] || EMU=../emulator
[ -x $EMU/abc80 ] || make -C $EMU abc80 > /dev/null || exit 1
[ -f forthaid.bin ] || ./gor.sh > /dev/null || exit 1
ROM=$EMU/roms/abc80new.rom
[ "$1" = 11273 ] && ROM=$EMU/roms/abc80old.rom
SA3=../../dis/roms/smartaid3_dec.bin
[ -f $SA3 ] || SA3=
python3 - "$EMU/abc80" "$ROM" "$SA3" <<'PY' || exit 1
import os, re, subprocess, sys, tempfile
emu, rom, sa3 = sys.argv[1:4]
tmp = tempfile.mkdtemp()

def program(n):
    """n rader med tilldelningar, PRINT, IF, GOTO och GOSUB, och sist
    Z9, som FIND letar efter."""
    rader = []
    for i in range(1, n + 1):
        r = 10 * i
        v = "ABCDEFGH"[i % 8] + str(i % 10)
        s = [f'{v}={v}+{i}:IF {v}>{i} THEN {r + 10}',
             f'PRINT "RAD";{v};" OCH ";{v}*2',
             f'IF {v}<0 THEN GOSUB {r + 20}',
             f'{v}={v}-1:GOTO {r + 10}'][i % 4]
        rader.append(f'{r} {s}')
    rader += [f'{10 * n + 10} Z9=1:RETURN', f'{10 * n + 20} END']
    p = os.path.join(tmp, f'p{n}.bas')
    open(p, 'w').write('\n'.join(rader) + '\n')
    return p

def kor(kit, prog, kommando, t):
    fil, start = kit
    ut = subprocess.run([emu, '-r', rom, '-K', '20,20', '-l', fil + '@4000',
                         '-k', start + r'\r\w300\.', '-f', prog,
                         '-k', kommando, '-t', str(t)],
                        capture_output=True, text=True).stdout
    return [r.rstrip() for r in ut.split('\n') if r.strip()]

def klar(kit, prog, kommando, svar, t):
    rader = kor(kit, prog, kommando + r'\r', t)
    if kommando not in rader:
        return False
    efter = rader[len(rader) - 1 - rader[::-1].index(kommando) + 1:]
    return any(re.match(svar, r) for r in efter)

def tid(kit, prog, kommando, svar):
    lo, hi = 0, 160
    while not klar(kit, prog, kommando, svar, hi):
        lo, hi = hi, hi * 2
        if hi > 600000:
            return None
    while hi - lo > 10:
        mitt = (lo + hi) // 2
        if klar(kit, prog, kommando, svar, mitt):
            hi = mitt
        else:
            lo = mitt
    return hi

def storlek(kit, prog):
    rader = kor(kit, prog, r'PRINT PEEK(65054)+256*PEEK(65055)-PEEK(65052)-256*PEEK(65053)+1\r', 300)
    return int(rader[-3])

kits = [('Forthaid', ('forthaid.bin', 'Z=CALL(16384,2)'))]
if sa3:
    kits.append(('Smartaid 3', (sa3, 'Z=CALL(16384)')))
# Rubriken, kommandot i Forthaid och i Smartaid 3 (None: finns inte),
# och vad som visar att kommandot är klart.
kommandon = [('FIND Z9', 'FIND Z9', 'FIND Z9', r'\d+ Z9=1'),
             ('VAR', 'VAR', 'VAR', 'ABC80$'),
             ('VAR Z9', 'VAR Z9', None, 'ABC80$'),
             ('REN 10,10', 'REN 10,10', 'REN 10,10', 'ABC80$'),
             ('CHANGE B1', 'CHANGE B1,Q1', 'CHANGE B1 TO Q1', 'ABC80$')]
langder = [53, 213, 320]          # omkring 2, 8 och 12 KB i minnet

print('ROM', os.path.basename(rom), '- tid i ms utöver ett program på två rader')
print(f'{"":12}{"byte":>7}' + ''.join(f'{k[0]:>12}' for k in kommandon))
for i, (namn, kit) in enumerate(kits):
    egna = [(k[1 + i], k[3]) for k in kommandon]
    noll = [k and tid(kit, program(0), k, s) for k, s in egna]
    for n in langder:
        p = program(n)
        rad = f'{namn:12}{storlek(kit, p):7}'
        for (k, s), t0 in zip(egna, noll):
            t = k and tid(kit, p, k, s)
            rad += f'{"-" if k is None else "?" if t is None else t - t0:>12}'
        print(rad, flush=True)
PY

# Storleken per kommando: varje ord i fa.fs, fak.inc och fai.asm räknas
# till det kommando som ensamt använder det (direkt eller genom andra
# ord); ord som flera kommandon använder står som delade. Tolkens egna
# ord ur boken står för sig.
python3 - forthaid.lst fa.fs fak.inc forthaid.bin <<'PY'
import re, sys
import os
lst, fs, inc, binf = sys.argv[1:5]
src = open(fs, encoding='utf-8').read()
egna = set(re.findall(r'(?m)^: (\S+)', src))
egna |= set(re.findall(r'(?m)^;W \S+ (\S+)', open(inc, encoding='utf-8').read()))   # etiketterna, som i listningen
def namn(etikett):
    e = etikett[2:] if etikett.startswith('F_') else etikett
    return re.sub(r'_([0-9A-F]{2})', lambda m: chr(int(m.group(1), 16)), e)
# Orden i listningen: H_x: huvudet, sedan koden, till nästa H_ eller
# ABORTT (ingångstabellens sista ord; sedan kommer fai.asm).
ord_, ordning = {}, []
for rad in open(lst, encoding='utf-8', errors='replace'):
    m = re.match(r'([0-9A-F]{4})  .{14}\s*(H_(\S+)|ABORTT):', rad)
    if m:
        adr = int(m.group(1), 16)
        o = namn(m.group(3)) if m.group(3) else m.group(2)
        ordning.append((adr, o)); ord_[o] = set()
        aktuellt = o
        continue
    if ordning:
        for d in re.findall(r'DEFW ([^;]+)', rad):
            for e in d.split(','):
                ord_[aktuellt].add(namn(e.strip()))
slut = 0x4000 + os.path.getsize(binf)
stl = {}
for (a, o), (b, _) in zip(ordning, ordning[1:] + [(slut, None)]):
    stl[o] = b - a
kommandon = ['SYS', 'LIST', 'PEEK', 'FIND', 'VAR', 'DEL', 'AUTO', 'OLD',
             'CHANGE', 'REN', 'ED', 'START', 'HELP', 'EX', 'BYE']
inkoppling = ['PA', 'KBD']
def nas(o, sett):
    for r in ord_.get(o, ()):
        if r in egna and r not in sett and r not in kommandon:
            sett.add(r); nas(r, sett)
    return sett
anv = {}
for k in kommandon + inkoppling:
    for o in nas(k, {k}):
        anv.setdefault(o, set()).add(k)
summa = {k: 0 for k in kommandon + inkoppling}
delat = 0
for o in egna:
    if o not in stl:
        continue
    a = anv.get(o, set())
    if len(a) == 1:
        summa[next(iter(a))] += stl[o]
    else:
        delat += stl[o]
abortt = dict((o, a) for a, o in ordning)['ABORTT'] + 2   # ingångstabellens slut
# Fyllnaden före KTAB ($5F34) är ledig plats, inte fai.asm.
fyll = 0
for rad in open(lst, encoding='utf-8', errors='replace'):
    m = re.match(r'([0-9A-F]{4}) .*DEFS \$5F34-\$', rad)
    if m:
        fyll = 0x5F34 - int(m.group(1), 16)
fai = slut - abortt - fyll
print()
print('Forthaid i ROM (byte, ord som bara kommandot använder):')
for k in kommandon:
    print(f'  {k:8}{summa[k]:6}')
print(f'  {"PA, KBD":8}{summa["PA"] + summa["KBD"]:6}  (inkopplingen och editorn)')
print(f'  {"fai.asm":8}{fai:6}  (avbrottet och stegningen, med vektortabellen)')
print(f'  {"delat":8}{delat:6}  (ord som flera kommandon använder)')
print(f'  {"tolken":8}{abortt - 0x4000 - sum(stl[o] for o in egna if o in stl):6}  (bokens kärna och yttre tolk, ingångarna)')
print(f'  {"fritt":8}{fyll:6}  (fyllnaden före KTAB på $5F34)')
print(f'  {"summa":8}{slut - 0x4000:6}')
PY
