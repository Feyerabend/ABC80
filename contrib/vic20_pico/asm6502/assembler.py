"""
assembler.py  —  Simple 2-pass 6502 assembler

Syntax:
  LABEL EQU $1234       ; constant
  LABEL:                ; address label (colon required)
      ORG $1000         ; set origin
      LDA #$00          ; immediate
      LDA $20           ; zeropage (if value <= $FF)
      LDA $1E00         ; absolute
      LDA $20,X         ; zeropage,X
      LDA $1E00,Y       ; absolute,Y
      LDA ($20,X)       ; indirect,X
      LDA ($20),Y       ; indirect,Y
      ASL               ; accumulator (no operand)
      BNE LOOP          ; relative branch
      DB $01,$02,$03    ; data bytes  (also DEFB, .BYTE)
      DW $1234          ; data word, little-endian (also DEFW, .WORD)
  ; hex: $xx or 0xXX   binary: %bbbb   decimal: 123

Usage:
  python assembler.py program.asm [-o output.prg] [--raw] [--listing]
"""

import sys
import re
import argparse
from instr6502 import INSTRUCTION_TABLE

# ---- Build mnemonic+mode -> opcode lookup --------------------------------

OP_MAP = {}
for _opc, _info in INSTRUCTION_TABLE.items():
    _name = _info['name'].upper()
    _mode = _info['mode']
    OP_MAP[(_name, _mode)] = _opc
    # ASL_A -> also register as ASL/accumulator
    if _name.endswith('_A'):
        OP_MAP[(_name[:-2], 'accumulator')] = _opc

BRANCH_OPS  = {'BCC','BCS','BEQ','BMI','BNE','BPL','BVC','BVS'}
ACCUM_OPS   = {'ASL','LSR','ROL','ROR'}
DIRECTIVES  = {'ORG','.ORG','EQU','=','DB','DEFB','.BYTE','DW','DEFW','.WORD'}
KNOWN_MNEMS = set(name for (name, _) in OP_MAP) | BRANCH_OPS | ACCUM_OPS | DIRECTIVES

# ---- Number / expression parser ------------------------------------------

def parse_num(s, symbols):
    """
    Parse an integer literal or symbol.  Returns int or None.
    Supports: $xx, 0xXX, %bbbb, decimal, symbol, symbol+n, symbol-n,
              <expr (low byte), >expr (high byte).
    """
    s = s.strip()
    if not s:
        return None
    # Low byte operator
    if s.startswith('<'):
        v = parse_num(s[1:], symbols)
        return (v & 0xFF) if v is not None else None
    # High byte operator
    if s.startswith('>'):
        v = parse_num(s[1:], symbols)
        return ((v >> 8) & 0xFF) if v is not None else None
    # hex dollar
    if s.startswith('$'):
        try:
            return int(s[1:], 16)
        except ValueError:
            return None
    # hex 0x
    if s.lower().startswith('0x'):
        try:
            return int(s[2:], 16)
        except ValueError:
            return None
    # binary %
    if s.startswith('%'):
        try:
            return int(s[1:], 2)
        except ValueError:
            return None
    # plain integer
    if re.fullmatch(r'-?\d+', s):
        return int(s)
    # symbol ± offset
    m = re.fullmatch(r'([A-Za-z_][A-Za-z0-9_]*)\s*([+\-])\s*(\d+)', s)
    if m:
        base = symbols.get(m.group(1).upper())
        if base is None:
            return None
        off = int(m.group(3))
        return base + off if m.group(2) == '+' else base - off
    # plain symbol
    return symbols.get(s.upper())

# ---- Operand mode detector -----------------------------------------------

def detect_mode(op_str, symbols):
    """
    Given raw operand text and current symbol table, return (mode, value).
    value may be None for unresolved forward references.
    mode is one of the INSTRUCTION_TABLE mode strings, or 'accumulator'.
    """
    s = op_str.strip()

    if not s or s.upper() == 'A':
        return 'accumulator', None

    # Immediate  #value
    if s.startswith('#'):
        return 'immediate', parse_num(s[1:], symbols)

    # (zp,X)
    m = re.fullmatch(r'\((.+)\s*,\s*[Xx]\)', s)
    if m:
        return 'indirect_x', parse_num(m.group(1), symbols)

    # (zp),Y
    m = re.fullmatch(r'\((.+)\)\s*,\s*[Yy]', s)
    if m:
        return 'indirect_y', parse_num(m.group(1), symbols)

    # (addr)  — JMP indirect
    m = re.fullmatch(r'\((.+)\)', s)
    if m:
        return 'indirect', parse_num(m.group(1), symbols)

    # addr,X
    m = re.fullmatch(r'(.+)\s*,\s*[Xx]', s)
    if m:
        v = parse_num(m.group(1), symbols)
        if v is not None and 0 <= v <= 0xFF:
            return 'zeropage_x', v
        return 'absolute_x', v

    # addr,Y
    m = re.fullmatch(r'(.+)\s*,\s*[Yy]', s)
    if m:
        v = parse_num(m.group(1), symbols)
        if v is not None and 0 <= v <= 0xFF:
            return 'zeropage_y', v
        return 'absolute_y', v

    # plain addr / label
    v = parse_num(s, symbols)
    if v is not None and 0 <= v <= 0xFF:
        return 'zeropage', v
    return 'absolute', v

def mode_size(mode):
    """Operand byte size for a given mode (not counting the opcode itself)."""
    if mode in ('implied', 'accumulator'):
        return 0
    if mode in ('immediate', 'zeropage', 'zeropage_x', 'zeropage_y',
                'indirect_x', 'indirect_y', 'relative'):
        return 1
    return 2  # absolute, absolute_x, absolute_y, indirect

# ---- Instruction size estimator (for pass 1) -----------------------------

def estimate_size(mnem, op_str, symbols):
    """Return byte size of this statement for pass-1 PC tracking."""
    m = mnem.upper()
    op = op_str.strip() if op_str else ''

    if m in ('DB','DEFB','.BYTE'):
        return len([p for p in op.split(',') if p.strip()])
    if m in ('DW','DEFW','.WORD'):
        return len([p for p in op.split(',') if p.strip()]) * 2
    if m in ('ORG','.ORG','EQU','='):
        return 0

    if not op or op.upper() == 'A':
        return 1  # implied / accumulator

    if m in BRANCH_OPS:
        return 2

    mode, val = detect_mode(op, symbols)
    return 1 + mode_size(mode)

# ---- Source line tokeniser -----------------------------------------------

def tokenise(raw_line):
    """
    Strip comment, split into (label_or_none, mnemonic_or_none, operand_or_none).
    Label must end with ':'.  EQU handled separately.
    """
    line = raw_line.split(';')[0].rstrip()
    if not line.strip():
        return None, None, None

    # Up to 3 whitespace-separated tokens (operand is everything after token 2)
    parts = line.split(None, 2)
    if not parts:
        return None, None, None

    label = None
    idx   = 0

    if parts[0].endswith(':'):
        label = parts[0][:-1].upper()
        idx = 1

    if idx >= len(parts):
        return label, None, None

    mnem = parts[idx].upper()
    operand = parts[idx+1].strip() if idx+1 < len(parts) else None

    return label, mnem, operand

# ---- Two-pass assembler --------------------------------------------------

class AssemblyError(Exception):
    pass

def assemble(source_text, default_origin=0x1000):
    lines   = source_text.splitlines()
    symbols = {}

    # ---- Pass 1: collect labels & EQUs, track PC ----

    stmts = []   # (lineno, label, mnem, operand)
    pc    = default_origin

    for lineno, raw in enumerate(lines, 1):
        line = raw.split(';')[0].rstrip()
        stripped = line.strip()

        if not stripped:
            stmts.append((lineno, None, None, None))
            continue

        parts = stripped.split(None, 2)
        label   = None
        mnem    = None
        operand = None
        idx     = 0

        # Label with colon
        if parts[0].endswith(':'):
            label = parts[0][:-1].upper()
            idx = 1

        # EQU (may or may not have a label)
        if idx < len(parts) and parts[idx].upper() in ('EQU', '='):
            # pattern: [LABEL:] EQU value  — but normally: LABEL EQU value (no colon)
            # handle: bare identifier followed by EQU
            if label is None and idx == 0:
                label = parts[0].upper()
                idx = 1
            val_str = parts[idx+1].strip() if idx+1 < len(parts) else '0'
            val = parse_num(val_str, symbols)
            if val is not None:
                symbols[label] = val
            stmts.append((lineno, label, 'EQU', val_str))
            continue

        # Another EQU form: SYMBOL EQU value (first token has no colon, second is EQU)
        if len(parts) >= 2 and parts[1].upper() in ('EQU', '=') and label is None:
            sym = parts[0].upper()
            val_str = parts[2].strip() if len(parts) > 2 else '0'
            val = parse_num(val_str, symbols)
            if val is not None:
                symbols[sym] = val
            stmts.append((lineno, sym, 'EQU', val_str))
            continue

        if idx < len(parts):
            mnem = parts[idx].upper()
            # Operand = everything after the mnemonic (rejoin split parts)
            operand = ' '.join(parts[idx+1:]).strip() if idx+1 < len(parts) else None

        # ORG
        if mnem in ('ORG', '.ORG'):
            val = parse_num(operand, symbols) if operand else None
            if val is None:
                raise AssemblyError(f"Line {lineno}: cannot resolve ORG value: {operand!r}")
            pc = val
            if label:
                symbols[label] = pc
            stmts.append((lineno, label, mnem, operand))
            continue

        # Regular statement
        if label:
            symbols[label] = pc

        if mnem:
            size = estimate_size(mnem, operand or '', symbols)
            pc += size

        stmts.append((lineno, label, mnem, operand))

    # ---- Pass 2: emit bytes ----

    output       = bytearray()
    load_address = default_origin
    pc           = default_origin
    first_org    = True

    for lineno, label, mnem, operand in stmts:
        if mnem is None:
            continue

        m   = mnem.upper()
        op  = (operand or '').strip()

        # ---- Directives ----

        if m in ('EQU', '='):
            continue

        if m in ('ORG', '.ORG'):
            val = parse_num(op, symbols)
            if not output and first_org:
                load_address = val
                first_org = False
            pc = val
            continue

        if m in ('DB','DEFB','.BYTE'):
            for part in op.split(','):
                v = parse_num(part.strip(), symbols)
                if v is None:
                    raise AssemblyError(f"Line {lineno}: unresolved DB value: {part.strip()!r}")
                output.append(v & 0xFF)
                pc += 1
            continue

        if m in ('DW','DEFW','.WORD'):
            for part in op.split(','):
                v = parse_num(part.strip(), symbols)
                if v is None:
                    raise AssemblyError(f"Line {lineno}: unresolved DW value: {part.strip()!r}")
                output.append(v & 0xFF)
                output.append((v >> 8) & 0xFF)
                pc += 2
            continue

        # ---- Branch instructions ----

        if m in BRANCH_OPS:
            opc = OP_MAP.get((m, 'relative'))
            if opc is None:
                raise AssemblyError(f"Line {lineno}: unknown branch: {m}")
            target = parse_num(op, symbols)
            if target is None:
                raise AssemblyError(f"Line {lineno}: unresolved branch target: {op!r}")
            offset = target - (pc + 2)
            if offset < -128 or offset > 127:
                raise AssemblyError(f"Line {lineno}: branch out of range ({offset:+d})")
            output.append(opc)
            output.append(offset & 0xFF)
            pc += 2
            continue

        # ---- Accumulator-mode shortcuts (ASL, LSR, ROL, ROR with no operand) ----

        if m in ACCUM_OPS and (not op or op.upper() == 'A'):
            opc = OP_MAP.get((m + '_A', 'implied'))
            if opc is None:
                raise AssemblyError(f"Line {lineno}: no accumulator mode for {m}")
            output.append(opc)
            pc += 1
            continue

        # ---- Implied (no operand) ----

        if not op:
            opc = OP_MAP.get((m, 'implied'))
            if opc is None:
                opc = OP_MAP.get((m + '_A', 'implied'))
            if opc is None:
                raise AssemblyError(f"Line {lineno}: unknown implied instruction: {m}")
            output.append(opc)
            pc += 1
            continue

        # ---- All other instructions ----

        mode, val = detect_mode(op, symbols)

        if val is None:
            raise AssemblyError(f"Line {lineno}: unresolved symbol in operand: {op!r}")

        if mode == 'accumulator':
            opc = OP_MAP.get((m + '_A', 'implied'))
            if opc is None:
                raise AssemblyError(f"Line {lineno}: no accumulator mode for {m}")
            output.append(opc)
            pc += 1
            continue

        opc = OP_MAP.get((m, mode))

        # Fallbacks: if exact mode not found, try wider form
        if opc is None and mode == 'zeropage':
            opc = OP_MAP.get((m, 'absolute'))
            if opc is not None:
                mode = 'absolute'
        if opc is None and mode == 'zeropage_x':
            opc = OP_MAP.get((m, 'absolute_x'))
            if opc is not None:
                mode = 'absolute_x'
        if opc is None and mode == 'zeropage_y':
            opc = OP_MAP.get((m, 'absolute_y'))
            if opc is not None:
                mode = 'absolute_y'

        if opc is None:
            raise AssemblyError(
                f"Line {lineno}: no opcode for {m} mode={mode} val=${val:04X}"
            )

        output.append(opc)
        pc += 1

        sz = mode_size(mode)
        if sz == 1:
            output.append(val & 0xFF)
            pc += 1
        elif sz == 2:
            output.append(val & 0xFF)
            output.append((val >> 8) & 0xFF)
            pc += 2

    return bytes(output), load_address, symbols


# ---- CLI entry point -------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description='6502 assembler for VIC-20')
    parser.add_argument('input',            help='Input .asm source file')
    parser.add_argument('-o','--output',    help='Output file (default: <input>.prg)')
    parser.add_argument('--raw',            action='store_true',
                        help='Raw binary output (no 2-byte .prg load-address header)')
    parser.add_argument('--listing',        action='store_true',
                        help='Print symbol table after assembly')
    args = parser.parse_args()

    with open(args.input, 'r') as fh:
        source = fh.read()

    try:
        binary, load_addr, symbols = assemble(source)
    except AssemblyError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)

    out_path = args.output or re.sub(r'\.asm$', '.prg', args.input)
    if out_path == args.input:
        out_path = args.input + '.prg'

    with open(out_path, 'wb') as fh:
        if not args.raw:
            fh.write(bytes([load_addr & 0xFF, (load_addr >> 8) & 0xFF]))
        fh.write(binary)

    print(f"OK: {len(binary)} bytes  load=${load_addr:04X}  → {out_path}")

    if args.listing:
        print(f"\n{'Symbol':<24} Value")
        print('-' * 32)
        for name, val in sorted(symbols.items()):
            print(f"  {name:<22} ${val:04X}  ({val})")


if __name__ == '__main__':
    main()
