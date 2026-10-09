#!/usr/bin/env python3
"""
wav2basic.py  read ABC80 cassette recordings (WAV) and write the files on them.

Replaces the two steps wav2bin (WAV -> "0101" text) and bin2basic.py
(text -> program) with one, and makes the decoding more robust:

  * 8- or 16-bit PCM, mono or stereo (both channels are tried)
  * the edges are found with a Schmitt trigger relative to the local
    amplitude, so noise around zero gives no false edges
  * the bit cell length is tracked, so tape speed variations are followed
  * several passes with different settings; a block is accepted when its
    checksum is right, and the passes fill each other's gaps

The tape format (ABC80 ROM, CASWBL; Markesjo 1978, s. 193-194):
  FM coding, every bit cell starts with an edge, a 1 has one more edge in
  the middle. About 740 bit/s. Bytes LSB first. Each block:
      256 zero bits, 3 x SYN ($16), STX ($02), 256 bytes, ETX ($03),
      checksum 2 bytes (sum of the 256 bytes and ETX, low byte first)
  Name block: $FF $FF $FF, name (8 + 3 characters), zeros.
  Data block: $00, block number (2 bytes, from 0), 253 bytes of data.
  In a text file (BAS) the data are lines ended by CR, $09 n means n spaces,
  and $03 ends the data in the block.

S. Lonnert 2023, 2026. Public domain.
"""

import argparse
import os
import re
import sys

import numpy as np

SR_DEFAULT = 44100
SYN, STX, ETX = 0x16, 0x02, 0x03

# ABC80 7-bit Swedish characters
SWEDISH = {
    0x40: 'É', 0x5B: 'Ä', 0x5C: 'Ö', 0x5D: 'Å', 0x5E: 'Ü',
    0x60: 'é', 0x7B: 'ä', 0x7C: 'ö', 0x7D: 'å', 0x7E: 'ü',
    0x24: '¤',
}


# WAV

def read_wav(path):
    """Return (sample rate, list of channels as float32 arrays)."""
    with open(path, 'rb') as f:
        if f.read(4) != b'RIFF':
            raise ValueError('not a RIFF file')
        f.read(4)
        if f.read(4) != b'WAVE':
            raise ValueError('not a WAVE file')
        fmt = None
        while True:
            hdr = f.read(8)
            if len(hdr) < 8:
                raise ValueError('no data chunk')
            cid, size = hdr[:4], int.from_bytes(hdr[4:], 'little')
            if cid == b'fmt ':
                b = f.read(size)
                fmt = (int.from_bytes(b[0:2], 'little'),    # format
                       int.from_bytes(b[2:4], 'little'),    # channels
                       int.from_bytes(b[4:8], 'little'),    # sample rate
                       int.from_bytes(b[14:16], 'little'))  # bits
            elif cid == b'data':
                offset = f.tell()
                break
            else:
                f.seek(size + (size & 1), 1)
    fmtype, nch, sr, bits = fmt
    if fmtype != 1:
        raise ValueError('only PCM is handled')
    dtype = {8: np.uint8, 16: '<i2', 32: '<i4'}[bits]
    raw = np.memmap(path, dtype=dtype, mode='r', offset=offset)
    raw = raw[:len(raw) // nch * nch].reshape(-1, nch)
    chans = []
    for c in range(nch):
        x = raw[:, c].astype(np.float32)
        if bits == 8:
            x -= 128.0
        chans.append(x)
    return sr, chans


def moving_average(x, k):
    c = np.cumsum(np.concatenate(([0.0], x.astype(np.float64))))
    m = (c[k:] - c[:-k]) / k
    pad = k // 2
    return np.concatenate((np.full(pad, m[0]), m, np.full(len(x) - len(m) - pad, m[-1])))


def find_edges(x, sr, hyst):
    """Edges (sample index) found with a Schmitt trigger at hyst * local amplitude."""
    x = x - moving_average(x, max(3, sr // 100))       # remove DC / hum below ~100 Hz
    env = moving_average(np.abs(x), max(3, sr // 20))  # local amplitude
    h = np.maximum(env * hyst, 1e-3 * (np.abs(x).max() + 1))
    m = np.where(x > h, 1, np.where(x < -h, -1, 0)).astype(np.int8)
    nz = np.nonzero(m)[0]
    flips = np.nonzero(np.diff(m[nz]))[0] + 1
    return nz[flips]


# bits

def edges_to_bits(edges, sr, split):
    """FM decoding. Returns bits (0, 1, or 2 = unreadable) and the start sample of each."""
    iv = np.diff(edges).tolist()
    cell = sr / 740.0
    nominal = cell
    bits, pos = [], []
    i, n = 0, len(iv)
    while i < n:
        t = iv[i]
        if t > split * cell and t < 1.5 * cell:
            bits.append(0)
            pos.append(edges[i])
            cell += 0.05 * (t - cell)
            i += 1
        elif t <= split * cell and t > 0.2 * cell and i + 1 < n and \
                0.7 * cell < t + iv[i + 1] < 1.4 * cell:
            bits.append(1)
            pos.append(edges[i])
            cell += 0.05 * (t + iv[i + 1] - cell)
            i += 2
        else:
            bits.append(2)
            pos.append(edges[i])
            i += 1
            if t > 3 * cell:       # silence: start over at nominal speed
                cell = nominal
        cell = min(max(cell, 0.7 * nominal), 1.3 * nominal)
    return np.array(bits, dtype=np.int8), np.array(pos, dtype=np.int64)


def byte_bits(b):
    return [(b >> k) & 1 for k in range(8)]


SYNC = np.array(byte_bits(SYN) * 3 + byte_bits(STX), dtype=np.int8)
WEIGHTS = (1 << np.arange(8)).astype(np.int64)


def find_blocks(bits, pos, sr):
    """All blocks with a correct checksum: list of (time in s, 256 bytes)."""
    L = len(SYNC)
    W = np.lib.stride_tricks.sliding_window_view(bits, L)
    hits = np.nonzero((W == SYNC).all(axis=1))[0]
    found = []
    for h in hits:
        s = h + L
        seg = bits[s:s + 259 * 8]
        if len(seg) < 259 * 8 or (seg == 2).any():
            continue
        by = seg.reshape(-1, 8).astype(np.int64) @ WEIGHTS
        data = by[:256]
        if by[256] != ETX:
            continue
        if (int(data.sum()) + ETX) & 0xFFFF != int(by[257]) | (int(by[258]) << 8):
            continue
        found.append((pos[h] / sr, bytes(int(v) for v in data)))
    return found


# passes

PASSES = [(0.40, 0.75), (0.25, 0.75), (0.60, 0.75), (0.40, 0.68), (0.40, 0.82),
          (0.15, 0.75), (0.80, 0.75)]


def decode_tape(path, verbose=False, quick=False):
    sr, chans = read_wav(path)
    if verbose:
        print(f'{path}: {sr} Hz, {len(chans)} channel(s), '
              f'{len(chans[0]) / sr / 60:.1f} min', file=sys.stderr)
    blocks = []       # (time, data); one per physical block on the tape
    passes = PASSES[:1] if quick else PASSES
    for c, x in enumerate(chans):
        for hyst, split in passes:
            edges = find_edges(x, sr, hyst)
            bits, pos = edges_to_bits(edges, sr, split)
            new = 0
            for t, data in find_blocks(bits, pos, sr):
                if not any(abs(t - u) < 1.0 for u, _ in blocks):
                    blocks.append((t, data))
                    new += 1
            if verbose:
                print(f'  channel {c}, hysteresis {hyst}, split {split}: '
                      f'+{new} blocks ({len(blocks)})', file=sys.stderr)
    blocks.sort()
    return blocks


#  files

class TapeFile:
    def __init__(self, name, time):
        self.name = name        # None if the name block was not read
        self.time = time
        self.blocks = {}        # number -> data

    def missing(self):
        if not self.blocks:
            return []
        return [n for n in range(max(self.blocks) + 1) if n not in self.blocks]


def tape_name(data):
    raw = data[3:14]
    stem = raw[:8].decode('latin-1').rstrip()
    ext = raw[8:11].decode('latin-1').rstrip()
    return stem, ext


def group_files(blocks):
    """Name block starts a file; data blocks follow with rising numbers."""
    files, cur, last_t = [], None, None
    for t, data in blocks:
        if data[0:3] == b'\xff\xff\xff':
            cur = TapeFile(tape_name(data), t)
            files.append(cur)
        else:
            n = data[1] | (data[2] << 8)
            # a new file if there is no current one, the number goes back,
            # or the gap is far longer than the missing blocks would explain
            if cur is None or n in cur.blocks or \
                    (cur.blocks and n < max(cur.blocks)) or \
                    (last_t is not None and t - last_t > 3.3 * (n - (max(cur.blocks) if cur.blocks else -1)) + 6):
                cur = TapeFile(None, t)
                files.append(cur)
            cur.blocks[n] = data
        last_t = t
    return files


def text_block(data):
    """Bytes of a text block, up to ETX, with $09 n expanded to spaces."""
    out = bytearray()
    i = 3
    while i < 256:
        b = data[i]
        if b == ETX:
            return bytes(out), True
        if b == 0x09 and i + 1 < 256:
            out += b' ' * data[i + 1]
            i += 2
            continue
        if b != 0:
            out.append(b)
        i += 1
    return bytes(out), False


def file_kind(tf):
    """'BAC' (SAVE: compiled lines), 'TEXT' (LIST: lines of text) or 'BIN'."""
    if 0 in tf.blocks and tf.blocks[0][3] == 0x82:
        return 'BAC'
    if all(text_block(d)[1] for d in tf.blocks.values()):
        return 'TEXT'
    return 'BIN'


def to_unicode(raw, swedish):
    out = []
    for b in raw:
        if b == 0x0D:
            out.append('\n')
        elif b == 0x0A:
            continue
        elif swedish and b in SWEDISH:
            out.append(SWEDISH[b])
        elif 32 <= b < 127:
            out.append(chr(b))
        else:
            out.append(f'\\x{b:02x}' if swedish else chr(b))
    return ''.join(out)


def safe_filename(s):
    s = s.translate(str.maketrans({'[': 'AE', ']': 'AA', '\\': 'OE',
                                   '{': 'ae', '}': 'aa', '|': 'oe'}))
    s = re.sub(r'[^A-Za-z0-9_.-]', '_', s)
    return s or 'NONAME'


def write_files(files, outdir, swedish):
    os.makedirs(outdir, exist_ok=True)
    report = []
    used = set()
    for k, tf in enumerate(files, 1):
        stem, ext = tf.name if tf.name else (f'OKAND{k:02d}', '')
        kind = file_kind(tf)
        if not ext:
            ext = {'BAC': 'BAC', 'TEXT': 'BAS', 'BIN': 'BIN'}[kind]
        fname = safe_filename(f'{k:02d}-{stem}') + '.' + safe_filename(ext)
        while fname in used:
            fname = fname.replace('.', '_.', 1)
        used.add(fname)
        path = os.path.join(outdir, fname)
        nblk = max(tf.blocks) + 1 if tf.blocks else 0

        # the file itself: 253 bytes per block, BAC padded to 256-byte
        # sectors as on disk (the same format as the files in FILES), so
        # that bac2bas can read both
        with open(path + ('.raw' if kind == 'TEXT' else ''), 'wb') as f:
            for n in range(nblk):
                data = tf.blocks.get(n, bytes(256))[3:]
                f.write(data + bytes(3) if kind == 'BAC' else data)

        listing = None
        if kind == 'TEXT':
            parts = []
            for n in range(nblk):
                if n in tf.blocks:
                    parts.append(text_block(tf.blocks[n])[0])
                else:
                    parts.append(b'\rREM *** BLOCK %d SAKNAS ***\r' % n)
            listing, lpath = b''.join(parts), path
        if listing is not None:
            if swedish:
                with open(lpath, 'w', encoding='utf-8', newline='\n') as f:
                    f.write(to_unicode(listing, True))
            else:
                with open(lpath, 'wb') as f:
                    f.write(listing)
        report.append((fname, tf, tf.missing(), kind))
    return report


# main

def main():
    ap = argparse.ArgumentParser(description='ABC80 cassette WAV -> BASIC files')
    ap.add_argument('wav', nargs='+')
    ap.add_argument('-o', 'outdir', default='ut')
    ap.add_argument('-a', 'ascii', action='store_true',
                    help='keep ABC80 7-bit ASCII ([\\]{|}$) instead of ÄÖÅäöå¤')
    ap.add_argument('-q', 'quick', action='store_true', help='one pass per channel only')
    ap.add_argument('-v', 'verbose', action='store_true')
    args = ap.parse_args()

    for wav in args.wav:
        tape = os.path.splitext(os.path.basename(wav))[0]
        blocks = decode_tape(wav, args.verbose, args.quick)
        files = group_files(blocks)
        outdir = os.path.join(args.outdir, tape)
        report = write_files(files, outdir, not args.ascii)
        print(f'\n{wav}: {len(blocks)} block, {len(files)} filer -> {outdir}/')
        for fname, tf, missing, kind in report:
            m, s = divmod(int(tf.time), 60)
            state = 'inga datablock' if not tf.blocks else \
                'ok' if not missing and tf.name else \
                ', '.join(filter(None, ['namnblock saknas' if not tf.name else '',
                                        f'saknar block {missing}' if missing else '']))
            print(f'  {m:2d}:{s:02d}  {fname:24s} {len(tf.blocks):3d} block  '
                  f'{kind:4s}  {state}')


if __name__ == '__main__':
    main()
