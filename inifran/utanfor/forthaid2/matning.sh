#!/bin/sh
# Mäter FIND, VAR, REN och CHANGE i Forthaid II, utan TKN80 och med TKN80
# i 80-teckensläget, och jämför med Forthaid (../forthaid) och, om den
# finns i arkivet, Smartaid 3 (dis/roms/smartaid3_dec.bin), på program om
# ungefär 2, 8 och 12 KB. Räknar också anropen över bankgränsen (DOFAR)
# per kommando och skriver storleken. Utanför boken, se LASMIG.md.
#
# Tiderna mäts som i ../forthaid/matning.sh (se huvudet där): den
# emulerade ABC80:s tid, med allt längre -t tills kommandot är klart,
# utöver samma kommando på ett program med två rader. Anropen räknas
# med emulatorns -P på DOFAR:s adress (ur forthaid2.lst), medan
# kommandot körs, utan det anrop av EDIT (bank 0) som läser in
# kommandoraden.
#
#     ./matning.sh          (9913; ./matning.sh 11273 för den gamla)
cd "$(dirname "$0")" || exit 1
EMU=../../emulator
[ -x $EMU/abc80 ] || make -C $EMU abc80 > /dev/null || exit 1
[ -f forthaid2_b0.bin ] || ./gor.sh > /dev/null || exit 1
[ -f ../forthaid/forthaid.bin ] || ../forthaid/gor.sh > /dev/null || exit 1
ROM=$EMU/roms/abc80new.rom
[ "$1" = 11273 ] && ROM=$EMU/roms/abc80old.rom
T80=$EMU/roms/tkn80-iii.rom
[ -f $T80 ] || T80=
SA3=../../dis/roms/smartaid3_dec.bin
[ -f $SA3 ] || SA3=
DOFAR=$(grep '^[0-9A-F]\{4\} .* DOFAR:' forthaid2.lst | cut -c1-4)
[ -n "$DOFAR" ] || { echo "matning.sh: DOFAR finns inte i forthaid2.lst"; exit 1; }
python3 - "$EMU/abc80" "$ROM" "$T80" "$SA3" "$DOFAR" <<'PY' || exit 1
import glob, os, re, subprocess, sys, tempfile
emu, rom, t80, sa3, dofar = sys.argv[1:6]
tmp = tempfile.mkdtemp()

def program(n):
    """n rader med tilldelningar, PRINT, IF, GOTO och GOSUB, och sist
    Z9, som FIND letar efter (samma program som i Forthaids mätning)."""
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

banker = ','.join(sorted(glob.glob('forthaid2_b[0-9].bin')))
fa2 = ['-l', 'forthaid2.bin@4000', '-b', banker + '@5000']

def kor(kit, prog, kommando, t, extra=()):
    args, start = kit
    p = subprocess.run([emu, '-r', rom, '-K', '20,20', *args,
                        '-k', start + r'\r\w300\.', '-f', prog,
                        '-k', kommando, '-t', str(t), *extra],
                       capture_output=True, text=True)
    return [r.rstrip() for r in p.stdout.split('\n') if r.strip()], p.stderr

def klar(kit, prog, kommando, svar, t):
    rader, _ = kor(kit, prog, kommando + r'\r', t)
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

def anrop(kit, prog, kommando, t):
    """Antalet DOFAR medan kommandot körs: körningen med kommandot minus
    körningen utan, och minus ett för EDIT (bank 0), som läser in varje
    rad, också programmets."""
    def n(k):
        _, fel = kor(kit, prog, k, t, ['-P', dofar])
        return fel.count('PC=' + dofar.upper())
    return n(kommando + r'\r') - n(r'\.') - 1

def storlek(kit, prog):
    rader, _ = kor(kit, prog, r'PRINT PEEK(65054)+256*PEEK(65055)-PEEK(65052)-256*PEEK(65053)+1\r', 300)
    return int(rader[-3])

kits = [('Forthaid II', (fa2, 'Z=CALL(16384,2)'))]
if t80:
    kits.append(('II, TKN80', (['-tkn80', t80] + fa2, 'Z=CALL(16384,2)')))
kits.append(('Forthaid', (['-l', '../forthaid/forthaid.bin@4000'], 'Z=CALL(16384,2)')))
if sa3:
    kits.append(('Smartaid 3', (['-l', sa3 + '@4000'], 'Z=CALL(16384)')))
# Rubriken, kommandot i Forthaid (II) och i Smartaid 3 (None: finns
# inte), och vad som visar att kommandot är klart.
kommandon = [('FIND Z9', 'FIND Z9', 'FIND Z9', r'\d+ Z9=1'),
             ('VAR', 'VAR', 'VAR', 'ABC80$'),
             ('VAR Z9', 'VAR Z9', None, 'ABC80$'),
             ('REN 10,10', 'REN 10,10', 'REN 10,10', 'ABC80$'),
             ('CHANGE B1', 'CHANGE B1,Q1', 'CHANGE B1 TO Q1', 'ABC80$')]
langder = [53, 213, 320]          # omkring 2, 8 och 12 KB i minnet

print('ROM', os.path.basename(rom), '- tid i ms utöver ett program på två rader')
print(f'{"":12}{"byte":>7}' + ''.join(f'{k[0]:>12}' for k in kommandon))
for namn, kit in kits:
    i = 1 if namn == 'Smartaid 3' else 0
    egna = [(k[1 + i], k[3]) for k in kommandon]
    noll = [k and tid(kit, program(0), k, s) for k, s in egna]
    for n in langder:
        p = program(n)
        rad = f'{namn:12}{storlek(kit, p):7}'
        for (k, s), t0 in zip(egna, noll):
            t = k and tid(kit, p, k, s)
            rad += f'{"-" if k is None else "?" if t is None else t - t0:>12}'
        print(rad, flush=True)

print()
print('Anrop över bankgränsen (DOFAR) medan kommandot körs, Forthaid II')
print(f'{"":12}{"byte":>7}' + ''.join(f'{k[0]:>12}' for k in kommandon))
kit = kits[0][1]
for n in [0] + langder:
    p = program(n)
    rad = f'{"":12}{storlek(kit, p):7}'
    for k in kommandon:
        rad += f'{anrop(kit, p, k[1], 20000):>12}'
    print(rad, flush=True)
PY

# Storleken: den fasta delen och bankerna mot Forthaid.
fast=$(wc -c < forthaid2.bin | tr -d ' ')
fa=$(wc -c < ../forthaid/forthaid.bin | tr -d ' ')
echo
echo "Storleken i byte (utan fyllnaden före KTAB):"
summa=0
for f in forthaid2.bin forthaid2_b[0-9].bin; do
  n=$(wc -c < $f | tr -d ' ')
  case $f in
    forthaid2.bin) l=forthaid2.lst; k=4F34;;
    *) l=; k=;;
  esac
  fyll=0
  if [ -n "$l" ]; then
    d=$(grep "DEFS \$$k-\\$" $l | cut -c1-4)
    fyll=$((0x$k - 0x$d))
  fi
  printf '  %-18s %5d\n' "$f" $((n - fyll))
  summa=$((summa + n - fyll))
done
d=$(grep 'DEFS \$5F34-\$' ../forthaid/forthaid.lst | cut -c1-4)
fyll=$((0x5F34 - 0x$d))
printf '  %-18s %5d\n' "summa" $summa
printf '  %-18s %5d\n' "Forthaid" $((fa - fyll))
nf=$(grep -c 'CALL DOFAR$' forthaid2.lst)
echo "  fjärrord: $nf à 5 byte i den fasta delen och 2 i bankens tabell"
