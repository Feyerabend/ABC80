"""pdis800.py FIL.COD: plocka isär segmenten i en COD-fil från Pascal 800 (UCSD-lik p-kod).

Varje DOS-sektor har tre byte huvud; resten är en ström. Se PASCAL800.md."""
import sys
raw = open(sys.argv[1], 'rb').read()
data = b''.join(raw[i+3:i+256] for i in range(0, len(raw), 256))   # 3 byte huvud per sektor
hdr = raw[3:256]
w = lambda b, i: b[i] | b[i+1] << 8
nseg = w(hdr, 0)
segs = [(w(hdr, 4+4*i), w(hdr, 6+4*i)) for i in range(nseg)]
N = {128:'ABI',129:'ABR',130:'ADI',131:'ADR',132:'LAND',133:'DIF',134:'DVI',135:'DVR',136:'CHK',137:'FLO',138:'FLT',139:'INN',140:'INT',141:'LOR',142:'MODI',143:'MPI',144:'MPR',145:'NGI',146:'NGR',147:'LNOT',148:'SRS',149:'SBI',150:'SBR',151:'SGS',152:'SQI',153:'SQR',154:'STO',155:'IXS',156:'UNI',157:'LDE',158:'CSP',159:'LDCN',160:'ADJ',161:'FJP',162:'INC',163:'IND',164:'IXA',165:'LAO',166:'LSA',167:'LAE',168:'MOV',169:'LDO',170:'SAS',171:'SRO',172:'XJP',173:'RNP',174:'CIP',175:'EQU',176:'GEQ',177:'GRT',178:'LDA',179:'LDC',180:'LEQ',181:'LES',182:'LOD',183:'NEQ',184:'STR',185:'UJP',186:'LDP',187:'STP',188:'LDM',189:'STM',190:'LDB',191:'STB',192:'IXP',193:'RBP',194:'CBP',195:'EQUI',196:'GEQI',197:'GRTI',198:'LLA',199:'LDCI',200:'LEQI',201:'LESI',202:'LDL',203:'NEQI',204:'STL',205:'CXP',206:'CLP',207:'CGP',208:'LPA',209:'STE',211:'EFJ',212:'NFJ',213:'BPT',214:'XIT',215:'NOP'}
B = {162,163,164,165,167,169,171,198,202,204,168}  # stort operandord
def dis(c, a, e):
    while a < e:
        op = c[a]; s = '%04X  ' % a; a += 1
        def big():
            nonlocal a
            x = c[a]; a += 1
            if x & 128: x = (x & 127) << 8 | c[a]; a += 1
            return x
        if op < 128: print(s + 'SLDC %d' % op); continue
        if 216 <= op <= 231: print(s + 'SLDL %d' % (op-215)); continue
        if 232 <= op <= 247: print(s + 'SLDO %d' % (op-231)); continue
        if op >= 248: print(s + 'SIND %d' % (op-248)); continue
        n = N.get(op, '?%d' % op)
        if op in (161, 185, 211, 212):
            d = c[a]; a += 1
            print(s + '%s %+d' % (n, d - 256 if d > 127 else d))
        elif op == 166:
            l = c[a]; print(s + 'LSA %r' % c[a+1:a+1+l].decode('latin1')); a += 1 + l
        elif op == 199: print(s + 'LDCI %d' % w(c, a)); a += 2
        elif op in (205,): print(s + 'CXP %d,%d' % (c[a], c[a+1])); a += 2
        elif op in (158, 174, 206, 207, 193, 173, 194, 175,176,177,180,181,183, 170, 208, 160, 188, 189):
            print(s + '%s %d' % (n, c[a])); a += 1
        elif op in (178, 182, 184): l = c[a]; a += 1; print(s + '%s %d,%d' % (n, l, big()))
        elif op in (157, 167, 209): sg = c[a]; a += 1; print(s + '%s %d,%d' % (n, sg, big()))
        elif op in B: print(s + '%s %d' % (n, big()))
        else: print(s + n)
which = int(sys.argv[2]) if len(sys.argv) > 2 else None
for i, (sek, ln) in enumerate(segs):
    if which is not None and i != which: continue
    c = data[sek*256:sek*256+ln]
    nproc, segno = c[-1], c[-2]
    print('== segment %d: sektor %d, %d byte, nr %d, %d procedurer' % (i, sek, ln, segno, nproc))
    nxt = None
    ends = sorted(len(c)-2-2*p - w(c, len(c)-2-2*p) for p in range(1, nproc+1))
    for p in range(1, nproc+1):
        q = len(c) - 2 - 2*p; ptr = q - w(c, q)
        print('-- procedur %d: attribut vid %04X:' % (p, ptr), ' '.join('%04X' % w(c, ptr-2*j) for j in range(0, 5)))
        ent = ptr - 2 - w(c, ptr - 2)
        dis(c, ent, ptr - 8)
