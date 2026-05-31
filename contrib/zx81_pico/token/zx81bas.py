#!/usr/bin/env python3
"""
zx81bas.py — ZX81 BASIC tokenizer / detokenizer

Usage:
  zx81bas.py list  <file.p>         detokenize; print BASIC listing to stdout
  zx81bas.py build <file.bas>       tokenize text BASIC; write <file.p>
  zx81bas.py test  [dir|file.p]     round-trip test(s)
  zx81bas.py info  <file.p>         dump system variable info

Options (for 'list'):
  --raw    show raw [XX] for unrecognised bytes instead of substitutes
  --ascii  use ASCII fallbacks for block-graphic characters
"""

import struct
import sys
import os
import math
import re
from typing import List, Tuple, Optional

# ---------------------------------------------------------------------------
# ZX81 character set (codes 0x00–0x3F)
# ---------------------------------------------------------------------------
# Each entry is the printable representation of that char code.
# Graphics blocks use Unicode block-element chars where available.
_CHARS = [
    # 0x00–0x07
    ' ', '▘', '▝', '▀', '▖', '▌', '▞', '▛',
    # 0x08–0x0F
    '▗', '▚', '▐', '"', '£', '$', ':', '?',
    # 0x10–0x17
    '(', ')', '>', '<', '=', '+', '-', '*',
    # 0x18–0x1F
    '/', ';', ',', '.', '0', '1', '2', '3',
    # 0x20–0x27
    '4', '5', '6', '7', '8', '9', 'A', 'B',
    # 0x28–0x2F
    'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J',
    # 0x30–0x37
    'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R',
    # 0x38–0x3F
    'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
]

# ASCII fallbacks for block graphics (0x01–0x0A)
_CHARS_ASCII = list(_CHARS)
for _i, _c in enumerate(['#', '#', '#', '#', '#', '#', '#', '#', '#', '#']):
    _CHARS_ASCII[_i + 1] = _c

# Reverse map: ASCII character → ZX81 char code
# Only unambiguous single-char mappings used by the tokenizer.
_ASCII_TO_ZX: dict[str, int] = {}
for _i, _c in enumerate(_CHARS):
    if _c not in _ASCII_TO_ZX:
        _ASCII_TO_ZX[_c] = _i
# Also map lowercase letters to uppercase ZX codes
for _c in 'abcdefghijklmnopqrstuvwxyz':
    _ASCII_TO_ZX[_c] = _ASCII_TO_ZX[_c.upper()]

# ---------------------------------------------------------------------------
# ZX81 BASIC token table (0xC0–0xFF) — verified from ROM + reference
# Source: Sinclair ZX81 ROM token table at address 0x0111
# ---------------------------------------------------------------------------
TOKENS: dict[int, str] = {
    0xC0: '""',    # used for empty string / opening-quote context
    0xC1: 'AT',
    0xC2: 'TAB',
    0xC3: '?',     # unused placeholder
    0xC4: 'CODE',
    0xC5: 'VAL',
    0xC6: 'LEN',
    0xC7: 'SIN',
    0xC8: 'COS',
    0xC9: 'TAN',
    0xCA: 'ASN',
    0xCB: 'ACS',
    0xCC: 'ATN',
    0xCD: 'LN',
    0xCE: 'EXP',
    0xCF: 'INT',
    0xD0: 'SQR',
    0xD1: 'SGN',
    0xD2: 'ABS',
    0xD3: 'PEEK',
    0xD4: 'USR',
    0xD5: 'STR$',
    0xD6: 'CHR$',
    0xD7: 'NOT',
    0xD8: '**',
    0xD9: 'OR',
    0xDA: 'AND',
    0xDB: '<=',
    0xDC: '>=',
    0xDD: '<>',
    0xDE: 'THEN',
    0xDF: 'TO',
    0xE0: 'STEP',
    0xE1: 'LPRINT',
    0xE2: 'LLIST',
    0xE3: 'STOP',
    0xE4: 'SLOW',
    0xE5: 'FAST',
    0xE6: 'NEW',
    0xE7: 'SCROLL',
    0xE8: 'CONT',
    0xE9: 'DIM',
    0xEA: 'REM',
    0xEB: 'FOR',
    0xEC: 'GOTO',
    0xED: 'GOSUB',
    0xEE: 'INPUT',
    0xEF: 'LOAD',
    0xF0: 'LIST',
    0xF1: 'LET',
    0xF2: 'PAUSE',
    0xF3: 'NEXT',
    0xF4: 'POKE',
    0xF5: 'PRINT',
    0xF6: 'PLOT',
    0xF7: 'RUN',
    0xF8: 'SAVE',
    0xF9: 'RAND',
    0xFA: 'IF',
    0xFB: 'CLS',
    0xFC: 'UNPLOT',
    0xFD: 'CLEAR',
    0xFE: 'RETURN',
    0xFF: 'COPY',
    # Special function / constant tokens (< 0x80, bit 6 set)
    0x40: 'RND',
    0x41: 'INKEY$',
    0x42: 'PI',
}

# Reverse: keyword string → token byte.
# For the tokenizer we need longest-match ordering.
_KEYWORD_TO_TOKEN: dict[str, int] = {kw: code for code, kw in TOKENS.items()
                                      if kw not in ('?', '""')}
# Sort by decreasing length for longest-match scanning
_KEYWORDS_BY_LEN: List[Tuple[str, int]] = sorted(
    _KEYWORD_TO_TOKEN.items(), key=lambda x: -len(x[0])
)

# Keywords that are statement openers (get a leading space when listed)
_STATEMENT_TOKENS = {
    0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA,  # LPRINT..DIM
    0xEB, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF2, 0xF3, 0xF4,   # REM..POKE
    0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,  # PRINT..COPY
}

LOAD_ADDR   = 0x4009
SYS_VARS_SZ = 0x74       # 116 bytes
BASIC_START = LOAD_ADDR + SYS_VARS_SZ  # = 0x407D

# ---------------------------------------------------------------------------
# ZX81 floating-point codec
# ---------------------------------------------------------------------------

def float_to_zx(value: float) -> bytes:
    """Encode a Python float as a 5-byte ZX81 floating-point number."""
    if value == 0.0:
        return b'\x00\x00\x00\x00\x00'
    sign = 1 if value < 0 else 0
    v = abs(value)
    e = math.floor(math.log2(v)) + 1      # unbiased exponent; 2^(e-1) <= v < 2^e
    biased_exp = e + 128
    if biased_exp < 1 or biased_exp > 255:
        return b'\x00\x00\x00\x00\x00'    # underflow/overflow → 0
    frac = v / (2 ** (e - 1)) - 1.0       # fractional part of normalised mantissa
    frac = max(0.0, min(frac, 1.0 - 2**-31))
    m31 = round(frac * (2**31))
    b1 = (sign << 7) | ((m31 >> 24) & 0x7F)
    b2 = (m31 >> 16) & 0xFF
    b3 = (m31 >>  8) & 0xFF
    b4 =  m31        & 0xFF
    return bytes([biased_exp, b1, b2, b3, b4])


def zx_to_float(b: bytes) -> float:
    """Decode a 5-byte ZX81 floating-point number."""
    if len(b) != 5 or b[0] == 0:
        return 0.0
    exp = b[0]
    sign = (b[1] >> 7) & 1
    m_bytes = bytes([(b[1] & 0x7F)] + list(b[2:5]))
    stored_m = struct.unpack('>I', m_bytes)[0]
    full_m = (1 << 31) | stored_m
    exp_shift = exp - 160
    if exp_shift >= 0:
        value = float(full_m << exp_shift)
    else:
        value = full_m / float(1 << (-exp_shift))
    return -value if sign else value


def _fmt_number(v: float) -> str:
    """Format a ZX81 number as a clean string."""
    if v == round(v) and abs(v) < 1e15:
        return str(int(round(v)))
    return f'{v:.8g}'

# ---------------------------------------------------------------------------
# Detokenizer
# ---------------------------------------------------------------------------

def _decode_char(byte: int, use_ascii: bool = False, raw: bool = False) -> str:
    chars = _CHARS_ASCII if use_ascii else _CHARS
    if byte < len(chars):
        return chars[byte]
    if raw:
        return f'[{byte:02X}]'
    return f'[{byte:02X}]'


def detokenize_file(filepath: str, use_ascii: bool = False,
                    raw: bool = False) -> List[str]:
    """Load a .p file and return a list of BASIC listing lines."""
    with open(filepath, 'rb') as f:
        data = f.read()
    return detokenize_bytes(data, use_ascii=use_ascii, raw=raw)


def detokenize_bytes(data: bytes, use_ascii: bool = False,
                     raw: bool = False) -> List[str]:
    """Detokenize raw .p file bytes. Returns list of listing lines."""
    if len(data) < SYS_VARS_SZ + 4:
        return ['(file too short)']

    # Determine BASIC program bounds from system variables.
    # Memory layout in saved .p files is always:
    #   BASIC [NXTLIN, D_FILE)  →  Display file [D_FILE, ...)  →  Variables [VARS, ...)
    # So the true end of BASIC is min(VARS, D_FILE).
    nxtlin_ofs = 0x4029 - LOAD_ADDR    # 0x20
    vars_ofs   = 0x4010 - LOAD_ADDR    # 0x07
    dfile_ofs  = 0x400C - LOAD_ADDR    # 0x03

    if nxtlin_ofs + 2 <= len(data):
        nxtlin = struct.unpack('<H', data[nxtlin_ofs:nxtlin_ofs+2])[0]
        basic_start = nxtlin - LOAD_ADDR
    else:
        basic_start = SYS_VARS_SZ

    vars_addr = LOAD_ADDR + len(data)
    dfile_addr = LOAD_ADDR + len(data)
    if vars_ofs + 2 <= len(data):
        vars_addr = struct.unpack('<H', data[vars_ofs:vars_ofs+2])[0]
    if dfile_ofs + 2 <= len(data):
        dfile_addr = struct.unpack('<H', data[dfile_ofs:dfile_ofs+2])[0]
    basic_end = min(vars_addr, dfile_addr) - LOAD_ADDR

    # No BASIC: machine-code-only file (NXTLIN points at / past D_FILE)
    if basic_start >= basic_end:
        return []

    # Clamp to file bounds
    if not (0 <= basic_start < len(data)):
        basic_start = SYS_VARS_SZ
    if not (basic_start < basic_end <= len(data)):
        basic_end = len(data)

    lines = []
    pos = basic_start

    while pos + 4 <= basic_end:
        # Line header: 2-byte line number (big-endian) + 2-byte length (little-endian)
        line_hi = data[pos]
        if line_hi >= 0x80:
            break                   # end-of-BASIC marker or VARS
        line_num = (line_hi << 8) | data[pos + 1]
        line_len = data[pos + 2] | (data[pos + 3] << 8)
        pos += 4

        if pos + line_len > len(data):
            break

        line_end = pos + line_len
        text = _decode_line(data, pos, line_end, use_ascii=use_ascii, raw=raw)
        lines.append(f'{line_num:5d} {text}')
        pos = line_end

    return lines


def _decode_line(data: bytes, start: int, end: int,
                 use_ascii: bool = False, raw: bool = False) -> str:
    """Decode a single BASIC line body (from after the 4-byte header to end)."""
    parts = []
    pos = start
    in_rem = False   # after REM: all bytes are literal characters
    in_str = False   # inside a string literal "..."

    while pos < end:
        byte = data[pos]

        # Line terminator
        if byte == 0x76:
            break

        # Number marker: visible digit chars already precede this.
        # Skip marker + 5 float bytes (total 6 bytes).
        if byte == 0x7E and not in_rem:
            pos += 6
            continue

        # After REM: everything is a literal character
        if in_rem:
            parts.append(_decode_char(byte, use_ascii, raw))
            pos += 1
            continue

        # Inverse video characters (0x80–0xBF)
        if 0x80 <= byte <= 0xBF:
            base = _decode_char(byte & 0x3F, use_ascii, raw)
            parts.append(f'[INV:{base}]')
            pos += 1
            continue

        # Keyword tokens: 0xC0–0xFF and special 0x40–0x42
        is_token = (byte >= 0xC0) or (0x40 <= byte <= 0x42)
        if is_token and byte in TOKENS:
            if in_str:
                # Inside a string literal, token bytes are raw data values.
                # Use [XX] hex notation so the tokenizer can reconstruct them.
                parts.append(f'[{byte:02X}]')
            else:
                kw = TOKENS[byte]
                # The ZX81 ROM prepends a space before each token when listing.
                parts.append(' ' + kw)
                if byte == 0xEA:   # REM
                    in_rem = True
            pos += 1
            continue

        # Regular character (0x00–0x3F; also 0x43–0x75, 0x77–0x7D)
        c = _decode_char(byte, use_ascii, raw)
        # Track string literal for display (open/close on 0x0B = '"')
        if byte == 0x0B:
            in_str = not in_str
        parts.append(c)
        pos += 1

    result = ''.join(parts).strip()
    return result

# ---------------------------------------------------------------------------
# Tokenizer
# ---------------------------------------------------------------------------

def tokenize_text(bas_text: str) -> bytes:
    """
    Tokenize a human-readable BASIC listing and return a complete .p file.

    Each text line must start with a line number followed by BASIC code.
    Lines starting with '#' or empty lines are ignored.
    """
    program_lines: List[Tuple[int, bytes]] = []   # (line_num, body_bytes)

    for raw_line in bas_text.splitlines():
        line = raw_line.strip()
        if not line or line.startswith('#'):
            continue
        result = _tokenize_line(line)
        if result is not None:
            program_lines.append(result)

    program_lines.sort(key=lambda x: x[0])
    return _build_p_file(program_lines)


def _tokenize_line(line: str) -> Optional[Tuple[int, bytes]]:
    """Parse one text line ('10 PRINT "HELLO"') → (line_num, body_bytes)."""
    # Split off line number
    parts = line.split(None, 1)
    if not parts:
        return None
    try:
        line_num = int(parts[0])
    except ValueError:
        return None

    body_text = parts[1] if len(parts) > 1 else ''
    body = _encode_body(body_text)
    return (line_num, body)


def _encode_body(text: str) -> bytes:
    """
    Encode the body of a BASIC line (after the line number) to token bytes.
    Returns bytes NOT including the header or the final 0x76 newline.
    The caller appends 0x76.
    """
    out = bytearray()
    pos = 0
    in_rem = False
    in_str = False
    # In the listing, keyword tokens always have a space prepended.  We only
    # try keyword matching immediately after a space (or at the start of the
    # body), so sequences like SCORE or FLOOR are not mis-tokenised as
    # S+C+OR+E or F+L+OR+R even without an explicit word-boundary check.
    can_kw = True

    while pos < len(text):
        # Inside a REM comment: everything is literal; handle escape notation
        if in_rem:
            nb, pos = _decode_escape(text, pos)
            out.extend(nb)
            continue

        # Inside a string literal: no keyword scanning; handle escape notation
        if in_str:
            c = text[pos]
            if c == '"':
                out.append(0x0B)    # closing "
                in_str = False
                pos += 1
            else:
                nb, pos = _decode_escape(text, pos)
                out.extend(nb)
            continue

        # Space handling: in the listing, each keyword token is preceded by
        # exactly one space (its prefix).  Any additional spaces are explicit
        # ZX81 0x00 bytes stored in the program by the programmer.
        # Strategy: count the run of spaces; if a keyword follows, the last
        # space is the keyword prefix (skip it, set can_kw=True) and the
        # rest are encoded as 0x00.  Otherwise all spaces are encoded as 0x00.
        if text[pos] == ' ':
            n = 0
            j = pos
            while j < len(text) and text[j] == ' ':
                n += 1
                j += 1
            rest = text[j:].upper() if j < len(text) else ''
            next_is_kw = any(rest.startswith(kw) for kw, _ in _KEYWORDS_BY_LEN)
            if next_is_kw:
                for _ in range(n - 1):
                    out.append(0x00)
                can_kw = True
            else:
                for _ in range(n):
                    out.append(0x00)
            pos = j
            continue

        # Try to match a keyword — only valid after a space (or at start)
        if can_kw:
            matched = False
            upper = text[pos:].upper()
            for kw, token_byte in _KEYWORDS_BY_LEN:
                if upper.startswith(kw):
                    out.append(token_byte)
                    pos += len(kw)
                    if token_byte == 0xEA:   # REM
                        in_rem = True
                    matched = True
                    break
            if matched:
                can_kw = False
                continue
            # No keyword at this position; fall through as regular content
            can_kw = False

        c = text[pos]

        # String literal opening
        if c == '"':
            out.append(0x0B)        # opening "
            in_str = True
            pos += 1
            continue

        # Number literal: collect all digits/decimal point/E notation
        if c.isdigit() or (c == '.' and pos + 1 < len(text) and text[pos + 1].isdigit()):
            num_str, pos = _collect_number(text, pos)
            num_bytes, float_bytes = _encode_number(num_str)
            out.extend(num_bytes)
            out.append(0x7E)
            out.extend(float_bytes)
            continue

        # Regular character or [XX]/[INV:X] escape sequence
        nb, pos = _decode_escape(text, pos)
        out.extend(nb)

    return bytes(out)


def _collect_number(text: str, pos: int) -> Tuple[str, int]:
    """Consume a numeric literal from text[pos:] and return (str, new_pos)."""
    start = pos
    # Integer or float part
    while pos < len(text) and text[pos].isdigit():
        pos += 1
    if pos < len(text) and text[pos] == '.':
        pos += 1
        while pos < len(text) and text[pos].isdigit():
            pos += 1
    # Exponent
    if pos < len(text) and text[pos] in 'eE':
        pos += 1
        if pos < len(text) and text[pos] in '+-':
            pos += 1
        while pos < len(text) and text[pos].isdigit():
            pos += 1
    return text[start:pos], pos


def _encode_number(num_str: str) -> Tuple[bytes, bytes]:
    """
    Return (digit_bytes, float_bytes) for a number string.
    digit_bytes: ZX81 char codes for each digit character.
    float_bytes: 5-byte ZX81 float encoding.
    """
    # Emit each digit as its ZX81 character code
    digit_bytes = bytearray()
    for ch in num_str:
        if ch in _ASCII_TO_ZX:
            digit_bytes.append(_ASCII_TO_ZX[ch])
        # else skip (shouldn't happen for valid number strings)

    try:
        value = float(num_str)
    except ValueError:
        value = 0.0

    return bytes(digit_bytes), float_to_zx(value)


def _ascii_to_zx_char(c: str) -> int:
    """Map a single ASCII/Unicode character to its ZX81 character code."""
    if c in _ASCII_TO_ZX:
        return _ASCII_TO_ZX[c]
    # Try some common substitutions
    substitutions = {
        '£': 0x0C,
        '▘': 0x01, '▝': 0x02, '▀': 0x03,
        '▖': 0x04, '▌': 0x05, '▞': 0x06, '▛': 0x07,
        '▗': 0x08, '▚': 0x09, '▐': 0x0A,
    }
    if c in substitutions:
        return substitutions[c]
    return 0x0F   # fallback: '?'


def _decode_escape(text: str, pos: int) -> Tuple[bytes, int]:
    """
    Decode one character or escape sequence from text[pos:].
    Returns (encoded_bytes, new_pos).

    Handled escapes (used inside strings and REM):
      [XX]      → single raw byte with hex value XX
      [INV:X]   → inverse-video byte: 0x80 | charcode(X)
    Anything else → _ascii_to_zx_char for the next single character.
    """
    rest = text[pos:]
    if rest.startswith('[INV:') and ']' in rest:
        end = rest.index(']')
        inner = rest[5:end]          # the base-char string, e.g. ' ', 'W', '▘'
        if inner:
            base = _ascii_to_zx_char(inner[0])
        else:
            base = 0
        return bytes([0x80 | (base & 0x3F)]), pos + end + 1
    m = re.match(r'\[([0-9A-Fa-f]{2})\]', rest)
    if m:
        return bytes([int(m.group(1), 16)]), pos + len(m.group(0))
    return bytes([_ascii_to_zx_char(text[pos])]), pos + 1

# ---------------------------------------------------------------------------
# .p file builder
# ---------------------------------------------------------------------------

def _build_p_file(program_lines: List[Tuple[int, bytes]]) -> bytes:
    """
    Build a complete, valid ZX81 .p file from tokenized BASIC lines.
    Each tuple is (line_number, body_bytes_without_newline).
    """
    # Build the BASIC program area (each line = 4-byte header + body + 0x76)
    program = bytearray()
    for line_num, body in program_lines:
        body_with_nl = body + b'\x76'
        line_len = len(body_with_nl)
        program.append((line_num >> 8) & 0xFF)   # line number high byte
        program.append(line_num & 0xFF)           # line number low byte
        program.append(line_len & 0xFF)           # length low byte
        program.append((line_len >> 8) & 0xFF)    # length high byte
        program.extend(body_with_nl)

    # Minimal display file: 1 leading HALT + 24 rows of (32 spaces + HALT)
    dfile = bytearray()
    dfile.append(0x76)                            # leading HALT
    for _ in range(24):
        dfile.extend(b'\x00' * 32)               # 32 spaces
        dfile.append(0x76)                        # row HALT

    # Compute addresses (all relative to LOAD_ADDR = 0x4009)
    basic_addr  = BASIC_START                     # 0x407D
    vars_addr   = basic_addr + len(program)
    dfile_addr  = vars_addr + 1                   # after the 0x80 VARS marker
    eline_addr  = dfile_addr + len(dfile)
    stkbot_addr = eline_addr + 2

    first_line_num = program_lines[0][0] if program_lines else 0

    # Build system variables (116 bytes = 0x74, base offset from LOAD_ADDR)
    sv = bytearray(SYS_VARS_SZ)

    def wr(offset, val):
        sv[offset] = val & 0xFF

    def wr16(offset, val):
        sv[offset]     = val & 0xFF
        sv[offset + 1] = (val >> 8) & 0xFF

    def wr16be(offset, val):   # big-endian (for E_PPC line number)
        sv[offset]     = (val >> 8) & 0xFF
        sv[offset + 1] = val & 0xFF

    ofs = lambda addr: addr - LOAD_ADDR

    wr(ofs(0x4009), 0)                           # VERSN = 0
    wr16be(ofs(0x400A), first_line_num)          # E_PPC: current line number (big-endian!)
    wr16(ofs(0x400C), dfile_addr)                # D_FILE
    wr16(ofs(0x400E), dfile_addr + 1)            # DF_CC = D_FILE + 1
    wr16(ofs(0x4010), vars_addr)                 # VARS
    wr16(ofs(0x4014), eline_addr)                # E_LINE
    wr16(ofs(0x4016), eline_addr)                # CH_ADD
    wr16(ofs(0x401A), stkbot_addr)               # STKBOT
    wr16(ofs(0x401C), stkbot_addr)               # STKEND
    wr(ofs(0x401E), 0)                           # BERG
    wr16(ofs(0x401F), 0x4000)                    # MEM
    wr(ofs(0x4022), 2)                           # DF_SZ
    wr16be(ofs(0x4023), first_line_num)          # S_TOP
    wr(ofs(0x4028), 55)                          # MARGIN (PAL)
    wr16(ofs(0x4029), basic_addr)                # NXTLIN
    wr(ofs(0x403B), 0x40)                        # CDFLAG: SLOW mode

    buf = bytearray()
    buf.extend(sv)
    buf.extend(program)
    buf.append(0x80)            # VARS end marker
    buf.extend(dfile)

    return bytes(buf)

# ---------------------------------------------------------------------------
# Info command
# ---------------------------------------------------------------------------

def show_info(filepath: str) -> None:
    with open(filepath, 'rb') as f:
        data = f.read()

    print(f'File:   {filepath}')
    print(f'Size:   {len(data)} bytes')

    if len(data) < SYS_VARS_SZ:
        print('(file too short to parse system variables)')
        return

    def rd16(ofs_abs):
        ofs = ofs_abs - LOAD_ADDR
        if ofs + 2 <= len(data):
            return struct.unpack('<H', data[ofs:ofs+2])[0]
        return 0

    def rd16be(ofs_abs):
        ofs = ofs_abs - LOAD_ADDR
        if ofs + 2 <= len(data):
            return (data[ofs] << 8) | data[ofs+1]
        return 0

    versn   = data[0]
    e_ppc   = rd16be(0x400A)
    d_file  = rd16(0x400C)
    df_cc   = rd16(0x400E)
    vars_a  = rd16(0x4010)
    e_line  = rd16(0x4014)
    nxtlin  = rd16(0x4029)
    cdflag  = data[0x403B - LOAD_ADDR] if (0x403B - LOAD_ADDR) < len(data) else 0

    basic_start = nxtlin - LOAD_ADDR
    basic_end   = vars_a  - LOAD_ADDR
    basic_size  = basic_end - basic_start

    print(f'VERSN:  {versn}')
    print(f'E_PPC:  {e_ppc} (last edited line)')
    print(f'NXTLIN: {nxtlin:#06x}  → BASIC at file offset {basic_start:#06x}')
    print(f'VARS:   {vars_a:#06x}  → BASIC size {basic_size} bytes')
    print(f'D_FILE: {d_file:#06x}')
    print(f'DF_CC:  {df_cc:#06x}')
    print(f'E_LINE: {e_line:#06x}')
    print(f'CDFLAG: {cdflag:#04x}  (SLOW mode: {bool(cdflag & 0x40)})')

# ---------------------------------------------------------------------------
# Round-trip test
# ---------------------------------------------------------------------------

def run_test(path: str) -> Tuple[int, int]:
    """
    Test all .p files under path (or a single .p file).
    Returns (passed, failed) counts.
    """
    import difflib

    if os.path.isfile(path):
        files = [path]
    else:
        files = []
        for root, dirs, names in os.walk(path):
            for name in names:
                if name.lower().endswith('.p'):
                    files.append(os.path.join(root, name))
        files.sort()

    passed = failed = 0
    for fpath in files:
        try:
            with open(fpath, 'rb') as f:
                original_data = f.read()

            # Step 1: detokenize
            listing_a = detokenize_bytes(original_data)
            if not listing_a:
                continue

            # Step 2: retokenize
            bas_text = '\n'.join(listing_a)
            rebuilt_data = tokenize_text(bas_text)

            # Step 3: detokenize again
            listing_b = detokenize_bytes(rebuilt_data)

            # Compare listings (text level)
            if listing_a == listing_b:
                passed += 1
            else:
                failed += 1
                diff = list(difflib.unified_diff(
                    listing_a, listing_b,
                    fromfile='original', tofile='rebuilt', lineterm=''
                ))
                print(f'\nFAIL: {os.path.basename(fpath)}')
                for d in diff[:20]:
                    print(' ', d)
                if len(diff) > 20:
                    print(f'  ... ({len(diff)-20} more lines)')
        except Exception as e:
            failed += 1
            print(f'\nERROR: {os.path.basename(fpath)}: {e}')

    return passed, failed

# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def _usage():
    print(__doc__)
    sys.exit(1)


def main():
    args = sys.argv[1:]
    if len(args) < 1:
        _usage()

    cmd = args[0].lower()

    if cmd == 'list':
        if len(args) < 2:
            _usage()
        filepath = args[1]
        use_ascii = '--ascii' in args
        raw = '--raw' in args
        if not os.path.exists(filepath):
            print(f'Error: {filepath} not found', file=sys.stderr)
            sys.exit(1)
        lines = detokenize_file(filepath, use_ascii=use_ascii, raw=raw)
        for line in lines:
            print(line)

    elif cmd == 'build':
        if len(args) < 2:
            _usage()
        infile = args[1]
        outfile = args[2] if len(args) > 2 else os.path.splitext(infile)[0] + '.p'
        with open(infile, 'r', encoding='utf-8', errors='replace') as f:
            bas_text = f.read()
        data = tokenize_text(bas_text)
        with open(outfile, 'wb') as f:
            f.write(data)
        print(f'Written {len(data)} bytes → {outfile}')

    elif cmd == 'test':
        target = args[1] if len(args) > 1 else '.'
        passed, failed = run_test(target)
        total = passed + failed
        print(f'\nResults: {passed}/{total} passed, {failed} failed')
        sys.exit(0 if failed == 0 else 1)

    elif cmd == 'info':
        if len(args) < 2:
            _usage()
        show_info(args[1])

    else:
        _usage()


if __name__ == '__main__':
    main()
