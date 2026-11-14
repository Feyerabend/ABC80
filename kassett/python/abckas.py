"""
abckas.py  kärnan för kassettverktygen: block, filer och ABC80-signalen.

Lagren (se FORMAT.md):
  1 signal   FM som ABC80-ROM:en skriver den (CASWBL/CASWBI), till och från WAV
  2 block    256 nollbitar, 3 x SYN, STX, 256 byte, ETX, summa (lo, hi)
  3 fil      namnblock FF FF FF + namn (8 + 3) [+ tidkonstant, ABC 800],
             datablock 00 + blocknummer (lo, hi) + 253 byte
  4 innehåll text som TXTPRT skriver den ($09 n = n blanksteg, $03 = blockslut,
             sist ett block med sex nollor), annars rå data

Mellanformatet .kas är blocken efter varandra, 256 byte styck, i bandets
ordning, utan inledning, synk och summa.

Avkodningen av WAV behöver numpy. Resten är ren Python 3.

S. Lönnert 2023, 2026. Public domain.
"""

import array
import os
import sys
import wave

SYN, STX, ETX = 0x16, 0x02, 0x03
BLOCK = 256
PAYLOAD = 253          # data per datablock
TEXTMAX = 252          # text per block; den 253:e byten är ETX

# ABC80: CPU-klocka och bitcellernas längd i T (CASWBI/CASHLF)
CLOCK = 2995200
T_ZERO = 3993
T_ONE = 4065

# ABC 800-serien: CTC kanal 0 ger dubbla bittakten (CPU / 16 / tk) och en
# vippa delar den till SIO B:s TxC, så en bitcell är 32 x tk T. Filhuvudet
# går alltid med tk 129 (ungefär 700 baud), datablocken med tk ur filhuvudet
# (byte 14; 0 = 129, ABC80:s band). Se FORMAT.md, avsnitt 2.
CLOCK800 = 3000000
TK800 = 129


# ------------------------------------------------------------------
# lager 2: block

def checksum(data):
    """Summan som CASWBL räknar: 3 + summan av de 256 byten, 16 bitar."""
    return (3 + sum(data)) & 0xFFFF


def block_bytes(data, abc800=False):
    """Byten efter inledningen: SYN SYN SYN STX data ETX summa lo hi.
    ABC 800 (CASWR): SYN STX data ETX summa lo hi FF FF."""
    assert len(data) == BLOCK
    s = checksum(data)
    if abc800:
        return bytes([SYN, STX]) + bytes(data) + bytes([ETX, s & 0xFF, s >> 8, 0xFF, 0xFF])
    return bytes([SYN, SYN, SYN, STX]) + bytes(data) + bytes([ETX, s & 0xFF, s >> 8])


def block_tk(b):
    """Tidkonstanten för datablocken efter namnblocket b (ABC 800)."""
    return b[14] or TK800


# ------------------------------------------------------------------
# lager 3: filer

# den svenska 7-bitarskoden
SWEDISH = {0x40: 'É', 0x5B: 'Ä', 0x5C: 'Ö', 0x5D: 'Å', 0x5E: 'Ü',
           0x60: 'é', 0x7B: 'ä', 0x7C: 'ö', 0x7D: 'å', 0x7E: 'ü', 0x24: '¤'}
TO_ABC = {v: k for k, v in SWEDISH.items()}


def abc_name(name):
    """Filnamn -> 11 tecken som på bandet: 8 + 3, versaler, blanksteg efter.
    ÅÄÖ blir ]  [ \\ som i 7-bitarskoden."""
    base = os.path.basename(name).upper()
    base = ''.join(chr(TO_ABC[c]) if c in TO_ABC and c != '¤' else c for c in base)
    stem, _, ext = base.rpartition('.') if '.' in base else (base, '', '')
    stem = stem[:8].ljust(8)
    ext = ext[:3].ljust(3)
    return (stem + ext).encode('latin-1', 'replace')


def show_name(raw11):
    """11 tecken från bandet -> 'NAMN.EXT', med ÅÄÖ."""
    s = ''.join(SWEDISH.get(c, chr(c)) if c != 0x24 else '$' for c in bytes(raw11))
    stem, ext = s[:8].rstrip(), s[8:11].rstrip()
    return stem + ('.' + ext if ext else '')


def name_block(raw11, tk=None):
    """Namnblocket. tk är tidkonstanten för ABC 800 (byte 14), annars 0."""
    b = bytearray(BLOCK)
    b[0:3] = b'\xff\xff\xff'
    b[3:14] = raw11
    if tk is not None:
        b[14] = tk
    return bytes(b)


def data_block(nr, payload):
    assert len(payload) <= PAYLOAD
    b = bytearray(BLOCK)
    b[1] = nr & 0xFF
    b[2] = nr >> 8
    b[3:3 + len(payload)] = payload
    return bytes(b)


def is_name_block(b):
    return b[0:3] == b'\xff\xff\xff'


def block_number(b):
    return b[1] | (b[2] << 8)


def binary_blocks(data):
    """Rå data -> datablock, 253 byte i varje, nollor i det sista."""
    n = max(1, (len(data) + PAYLOAD - 1) // PAYLOAD)
    return [data_block(i, data[i * PAYLOAD:(i + 1) * PAYLOAD]) for i in range(n)]


def text_blocks(text, basic2=False):
    """Text (byte, CR, CRLF eller LF) -> datablock, som TXTPRT och TXTEND gör.

    Radslutet blir CR: CRLF och en ensam LF blir CR, som när ROM:en
    skriver CR LF och tar bort LF. Två eller fler blanksteg blir $09 n
    (högst 255 i en följd). Blanksteg före CR behålls: TXTPRT tar bara
    bort dem när CR kommer i samma anrop, och PRINT # skickar CR för sig.
    Varje block får högst 252 byte och sedan ETX. Filen slutar med ett block som har sex nollor och ETX.

    ROM:en skriver hela bufferten (253 byte) varje gång och nollar den
    inte, så efter ETX står det som var kvar från blocket före; från
    början är bufferten nollor. Så blir blocken (och summan) desamma som
    när ROM:en skriver dem (provat i emulatorn).

    basic2: som BASIC II i ABC 800-serien (BPRINT). Den packar blanksteg
    så länge två platser är lediga och skriver annars ett vanligt
    blanksteg på den sista; ABC80 börjar ett nytt block när två eller
    färre platser är lediga."""
    blocks, cur, buf = [], bytearray(), bytearray(PAYLOAD)

    def put():
        cur.append(ETX)
        buf[:len(cur)] = cur
        blocks.append(data_block(len(blocks), bytes(buf)))
        cur.clear()

    t = bytes(text).replace(b'\r\n', b'\r').replace(b'\n', b'\r')
    i, n = 0, len(t)
    while i < n:
        c = t[i]
        if c == 0x20 and i + 1 < n and t[i + 1] == 0x20:
            j = i
            while j < n and t[j] == 0x20 and j - i < 255:
                j += 1
            if basic2 and len(cur) == TEXTMAX - 1:
                cur.append(0x20)                 # sista platsen: ingen packning
                i += 1
            else:
                if not basic2 and TEXTMAX - len(cur) <= 2:
                    put()
                cur += bytes([0x09, j - i])
                i = j
        else:
            cur.append(c)
            i += 1
        if len(cur) >= TEXTMAX:
            put()
    put()                                        # TXTEND: aktuellt block ...
    cur += bytes(6)                              # ... och sex nollor
    put()
    return blocks


def text_from_blocks(payloads):
    """Datablockens innehåll -> text med CR, som TXTINP läser den.
    Returnerar (text, ok), där ok är falskt om något block saknar ETX."""
    out, ok = bytearray(), True
    for p in payloads:
        i = 0
        while True:
            if i >= len(p):
                ok = False
                break
            c = p[i]
            if c == ETX:
                break
            if c == 0x00:
                return bytes(out), ok
            if c == 0x09 and i + 1 < len(p):
                out += b' ' * p[i + 1]
                i += 2
                continue
            out.append(c)
            i += 1
    return bytes(out), ok


def looks_like_text(payloads):
    """Sant om varje block har ETX och bara tecken under $80 före den.
    ETX letas efter som TXTINP gör: n i $09 n hoppas över (det kan vara 3)."""
    if not payloads:
        return False
    for p in payloads:
        i = 0
        while i < len(p) and p[i] != ETX:
            if p[i] == 0x09:
                i += 2
            elif p[i] >= 0x80:
                return False
            else:
                i += 1
        if i >= len(p):
            return False
    return True


def plain_text(data):
    """Sant om filen bara har skrivbara 7-bitarstecken och radslut."""
    return len(data) > 0 and all(0x20 <= c < 0x7F or c in (0x0A, 0x0D) for c in data)


# ------------------------------------------------------------------
# text till och från dagens filer

# ABC:s 7-bitarskod: [\\]^ och {|}~ är ÄÖÅÜ och äöåü, @ och ` É och é.
# '$' får vara '$' (¤ står bara på tangenten).
ABC_TECKEN = {0x40: 'É', 0x5B: 'Ä', 0x5C: 'Ö', 0x5D: 'Å', 0x5E: 'Ü',
              0x60: 'é', 0x7B: 'ä', 0x7C: 'ö', 0x7D: 'å', 0x7E: 'ü'}
FRAN_TECKEN = {v: k for k, v in ABC_TECKEN.items()}
KODER = ('7', 'dos', 'utf8')
RADER = {'cr': b'\r', 'crlf': b'\r\n', 'lf': b'\n'}


def text_ut(data, kod='utf8', rad='lf'):
    """Text från ABC (7 bitar, radslut CR, kanske LF efter) -> dagens fil.
    kod: '7' som den är, 'dos' teckentabell 850, 'utf8'."""
    if not data:
        return b''
    rader = data.replace(b'\r\n', b'\r').replace(b'\n', b'\r').split(b'\r')
    if rader and rader[-1] == b'':
        rader.pop()
        slut = True
    else:
        slut = False
    if kod == '7':
        t = [r for r in rader]
    else:
        enc = 'cp850' if kod == 'dos' else 'utf-8'
        t = [''.join(ABC_TECKEN.get(c, chr(c)) for c in r).encode(enc, 'replace')
             for r in rader]
    ut = RADER[rad].join(t)
    return ut + RADER[rad] if slut else ut


def text_in(data, kod=None):
    """Dagens fil -> ABC:s 7-bitarskod med radslut CR. kod None: gissa
    (UTF-8 om det går, annars 850 om där finns byte >= $80, annars 7)."""
    if kod is None:
        if all(c < 0x80 for c in data):
            kod = '7'
        else:
            try:
                data.decode('utf-8')
                kod = 'utf8'
            except UnicodeDecodeError:
                kod = 'dos'
    if kod == '7':
        b = bytes(data)
    else:
        s = data.decode('cp850' if kod == 'dos' else 'utf-8', 'replace')
        b = bytes(FRAN_TECKEN.get(c, ord(c) if ord(c) < 0x80 else 0x3F) for c in s)
    return b.replace(b'\r\n', b'\r').replace(b'\n', b'\r')


class TapeFile:
    """En fil på bandet: namnblocket och datablocken i nummerordning."""

    def __init__(self, header=None):
        self.header = header        # namnblocket, eller None om det inte lästes
        self.blocks = {}            # nummer -> 256 byte

    @property
    def name(self):
        return show_name(self.header[3:14]) if self.header else None

    @property
    def tk(self):
        return self.header[14] if self.header else 0

    def missing(self):
        if not self.blocks:
            return []
        return [i for i in range(max(self.blocks) + 1) if i not in self.blocks]

    def payloads(self):
        return [self.blocks[i][3:] for i in sorted(self.blocks)]

    def kind(self):
        p = self.payloads()
        if p and p[0][:1] in (b'\x82', b'\x8f'):
            return 'BAC'
        if looks_like_text(p):
            return 'TEXT'
        return 'BIN'

    def content(self):
        """Innehållet: text med CR för TEXT. BAC får 256-bytes sektorer som
        på skivan (253 byte och tre nollor). Annars datablocken efter varandra."""
        p = self.payloads()
        kind = self.kind()
        if kind == 'TEXT':
            return text_from_blocks(p)[0]
        if kind == 'BAC':
            return b''.join(q + bytes(3) for q in p)
        return b''.join(p)


def make_file(path_or_name, data, kind=None, tk=None):
    """Bygg filens block. kind: 'TEXT', 'BIN' eller None: gissa, text
    för .BAS, .TXT och .ASC och annars text om filen bara har skrivbara
    7-bitarstecken och radslut (som en datafil från PRINT #)."""
    if kind is None:
        ext = os.path.splitext(path_or_name)[1].upper()
        kind = 'TEXT' if ext in ('.BAS', '.TXT', '.ASC') or plain_text(data) else 'BIN'
    if kind == 'TEXT':
        body = text_blocks(data, basic2=tk is not None)
    elif kind == 'BIN' and data[:1] in (b'\x82', b'\x8f') and len(data) % BLOCK == 0:
        # BAC från skivan: 256-bytes sektorer, de 253 första i varje går på bandet
        body = [data_block(i, data[i * BLOCK:i * BLOCK + PAYLOAD])
                for i in range(len(data) // BLOCK)]
    else:
        body = binary_blocks(data)
    return [name_block(abc_name(path_or_name), tk)] + body


def group_files(blocks):
    """Block i bandets ordning -> lista av TapeFile. Ett datablock före
    första namnblocket hamnar i en fil utan namn. Dubbletter tas bort."""
    files, cur = [], None
    for b in blocks:
        if is_name_block(b):
            cur = TapeFile(b)
            files.append(cur)
        elif b[0] == 0:
            nr = block_number(b)
            if cur is None or (nr == 0 and cur.blocks):
                cur = TapeFile()
                files.append(cur)
            cur.blocks.setdefault(nr, b)
    return files


# ------------------------------------------------------------------
# .kas

def read_kas(path):
    data = open(path, 'rb').read()
    if len(data) % BLOCK:
        raise ValueError(f'{path}: {len(data)} byte är inte en multipel av 256')
    return [data[i:i + BLOCK] for i in range(0, len(data), BLOCK)]


def write_kas(path, blocks):
    with open(path, 'wb') as f:
        for b in blocks:
            f.write(b)


# ------------------------------------------------------------------
# lager 1: signalen ut

def block_edges(data, tk=None):
    """Flankernas tider (i T) för ett block, räknat från blockets början.
    Varje bitcell slutar med en flank, och en etta har en till i mitten.
    Utan tk ABC80:s bitceller, med tk ABC 800:s (32 x tk T)."""
    edges, t = [], 0
    zero, one = (T_ZERO, T_ONE) if tk is None else (32 * tk, 32 * tk)

    def bit(b):
        nonlocal t
        if b:
            edges.append(t + one // 2)
            t += one
        else:
            t += zero
        edges.append(t)

    for _ in range(256):
        bit(0)
    for byte in block_bytes(data, tk is not None):
        for k in range(8):
            bit((byte >> k) & 1)
    return edges, t


def write_wav(path, blocks, rate=44100, lead=2.0, gap=0.5, amp=0.7, bits=16,
              abc800=False):
    """Block -> WAV med FM-signalen. lead är tystnaden före varje
    namnblock (ROM:en väntar 6,8 s), gap tystnaden före varje datablock.
    abc800: blocken som ABC 800 skriver dem, namnblocket med tk 129 och
    datablocken med tidkonstanten i namnblocket."""
    full = 127 if bits == 8 else 32767
    hi = int(amp * full)
    typecode = 'b' if bits == 8 else 'h'
    out = array.array(typecode)

    def silence(sec):
        out.extend([0] * int(sec * rate))

    clock, tk = (CLOCK800, TK800) if abc800 else (CLOCK, None)
    for b in blocks:
        name = is_name_block(b)
        silence(lead if name else gap)
        edges, total = block_edges(b, TK800 if abc800 and name else tk)
        if abc800 and name:
            tk = block_tk(b)
        start = len(out)
        level, prev = -hi, 0
        for e in edges:
            s = start + (e * rate + clock // 2) // clock   # heltal, som i C
            out.extend([level] * (s - start - prev))
            prev = s - start
            level = -level
        out.extend([level] * int(0.002 * rate))   # håll sista nivån en stund
    silence(gap)

    if bits == 8:
        raw = bytes((v + 128) & 0xFF for v in out)
    else:
        if sys.byteorder != 'little':
            out.byteswap()
        raw = out.tobytes()
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(bits // 8)
        w.setframerate(rate)
        w.writeframes(raw)


# ------------------------------------------------------------------
# lager 1: signalen in (numpy)

def read_wav(path):
    """(samplingsfrekvens, kanaler som float32). PCM 8, 16, 24 eller 32 bitar."""
    import numpy as np
    with wave.open(path, 'rb') as w:
        nch, width, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if width == 1:
        x = np.frombuffer(raw, np.uint8).astype(np.float32) - 128.0
    elif width == 2:
        x = np.frombuffer(raw, '<i2').astype(np.float32)
    elif width == 3:
        b = np.frombuffer(raw, np.uint8).reshape(-1, 3).astype(np.int32)
        v = b[:, 0] | (b[:, 1] << 8) | (b[:, 2] << 16)
        x = np.where(v >= 1 << 23, v - (1 << 24), v).astype(np.float32)
    else:
        x = np.frombuffer(raw, '<i4').astype(np.float32)
    x = x[:len(x) // nch * nch].reshape(-1, nch)
    return sr, [x[:, c].copy() for c in range(nch)]


def _moving_average(x, k):
    import numpy as np
    c = np.cumsum(np.concatenate(([0.0], x.astype(np.float64))))
    m = (c[k:] - c[:-k]) / k
    pad = k // 2
    return np.concatenate((np.full(pad, m[0]), m, np.full(len(x) - len(m) - pad, m[-1])))


def find_edges(x, sr, hyst):
    """Flankerna (samplenummer), med Schmitt-trigger på hyst gånger den
    lokala amplituden. Brum under ungefär 100 Hz tas bort först."""
    import numpy as np
    x = x - _moving_average(x, max(3, sr // 100))
    env = _moving_average(np.abs(x), max(3, sr // 20))
    h = np.maximum(env * hyst, 1e-3 * (np.abs(x).max() + 1))
    m = np.where(x > h, 1, np.where(x < -h, -1, 0)).astype(np.int8)
    nz = np.nonzero(m)[0]
    flips = np.nonzero(np.diff(m[nz]))[0] + 1
    return nz[flips]


def edges_to_bits(edges, sr, split, tk=None):
    """FM-avkodning med följd bitcellslängd. Bitarna blir 0, 1 eller
    2 (oläsbar). Returnerar också första samplet för varje bit. Utan tk
    utgår bitcellen från ABC80 (som också täcker ABC 800 i 700 baud),
    med tk från ABC 800 (32 x tk T)."""
    import numpy as np
    iv = np.diff(edges).tolist()
    if tk is None:
        nominal = sr * (T_ZERO + T_ONE) / 2 / CLOCK
    else:
        nominal = sr * 32 * tk / CLOCK800
    cell = nominal
    bits, pos = [], []
    i, n = 0, len(iv)
    while i < n:
        t = iv[i]
        if split * cell < t < 1.5 * cell:
            bits.append(0)
            pos.append(edges[i])
            cell += 0.05 * (t - cell)
            i += 1
        elif 0.2 * cell < t <= split * cell and i + 1 < n and \
                0.7 * cell < t + iv[i + 1] < 1.4 * cell:
            bits.append(1)
            pos.append(edges[i])
            cell += 0.05 * (t + iv[i + 1] - cell)
            i += 2
        else:
            bits.append(2)
            pos.append(edges[i])
            i += 1
            if t > 3 * cell:                # tystnad: börja om
                cell = nominal
        cell = min(max(cell, 0.7 * nominal), 1.3 * nominal)
    return np.array(bits, dtype=np.int8), np.array(pos, dtype=np.int64)


def _sync_bits(sb):
    import numpy as np
    return np.array([(b >> k) & 1 for b in sb for k in range(8)], dtype=np.int8)


def find_blocks(bits, pos, sr):
    """Alla block efter en synk, ABC80:s (SYN SYN SYN STX) eller ABC 800:s
    (00 00 SYN STX): lista av (tid, 256 byte, status), där status är 'ok',
    'summa', 'etx' eller 'bitar' (oläsbara bitar)."""
    import numpy as np
    L = 32
    if len(bits) < L:
        return []
    W = np.lib.stride_tricks.sliding_window_view(bits, L)
    hits = np.nonzero((W == _sync_bits((SYN, SYN, SYN, STX))).all(axis=1) |
                      (W == _sync_bits((0, 0, SYN, STX))).all(axis=1))[0]
    weights = (1 << np.arange(8)).astype(np.int64)
    found = []
    for h in hits:
        s = h + L
        seg = bits[s:s + 259 * 8]
        if len(seg) < 259 * 8:
            continue
        t = pos[h] / sr
        if (seg == 2).any():
            found.append((t, None, 'bitar'))
            continue
        by = seg.reshape(-1, 8).astype(np.int64) @ weights
        data = bytes(int(v) for v in by[:256])
        if by[256] != ETX:
            found.append((t, data, 'etx'))
        elif checksum(data) != int(by[257]) | (int(by[258]) << 8):
            found.append((t, data, 'summa'))
        else:
            found.append((t, data, 'ok'))
    return found


PASSES = [(0.40, 0.75), (0.25, 0.75), (0.60, 0.75), (0.40, 0.68), (0.40, 0.82),
          (0.15, 0.75), (0.80, 0.75)]


def decode_wav(path, quick=False, log=None):
    """WAV -> (block, rapport). Blocken är de godkända i tidsordning.
    Rapporten är (tid, status) för varje block som hittades, där
    status för ett block som aldrig gick igenom är det första felet."""
    sr, chans = read_wav(path)
    if log:
        log(f'{path}: {sr} Hz, {len(chans)} kanal(er), {len(chans[0]) / sr:.1f} s')
    good, bad = [], []
    for c, x in enumerate(chans):
        _decode_channel(c, x, sr, None, quick, good, bad, log)
    # ABC 800: datablocken går med tidkonstanten i namnblocket
    for tk in sorted({b[14] for _, b in good if is_name_block(b) and b[14]}):
        if tk != TK800:                   # 129 täcks av första omgången
            for c, x in enumerate(chans):
                _decode_channel(c, x, sr, tk, quick, good, bad, log)
    good.sort()
    report = [(t, 'ok') for t, _ in good]
    report += [(t, st) for t, st in bad if not any(abs(t - u) < 1.0 for u, _ in good)]
    report.sort()
    return [d for _, d in good], report


def _decode_channel(c, x, sr, tk, quick, good, bad, log):
    for hyst, split in (PASSES[:1] if quick else PASSES):
        edges = find_edges(x, sr, hyst)
        bits, pos = edges_to_bits(edges, sr, split, tk)
        new = 0
        for t, data, st in find_blocks(bits, pos, sr):
            if st == 'ok':
                if not any(abs(t - u) < 1.0 for u, _ in good):
                    good.append((t, data))
                    new += 1
            elif not any(abs(t - u) < 1.0 for u, _ in bad):
                bad.append((t, st))
        if log:
            log(f'  kanal {c}, hysteres {hyst}, gräns {split}'
                f'{f", tk {tk}" if tk else ""}: +{new} block ({len(good)})')
