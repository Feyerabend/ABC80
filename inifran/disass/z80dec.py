"""Z80 instruction decoder (documented instruction set).

decode(mem, pc) -> Insn or None (None = not a valid/documented opcode).
An Insn carries the byte length, a mnemonic template with {} placeholders
for address/immediate operands, the operand values, and control-flow info.
"""

from dataclasses import dataclass, field

R8 = ["B", "C", "D", "E", "H", "L", "(HL)", "A"]
R16 = ["BC", "DE", "HL", "SP"]
R16AF = ["BC", "DE", "HL", "AF"]
CC = ["NZ", "Z", "NC", "C", "PO", "PE", "P", "M"]
ALU = ["ADD  A,", "ADC  A,", "SUB  ", "SBC  A,", "AND  ", "XOR  ", "OR   ", "CP   "]
ROT = ["RLC", "RRC", "RL", "RR", "SLA", "SRA", None, "SRL"]  # SLL undocumented


@dataclass
class Op:
    """An operand placeholder in the mnemonic."""
    kind: str      # 'addr' (16-bit, may be label), 'imm8', 'imm16', 'port', 'disp', 'rst'
    value: int
    role: str = ""  # 'jump', 'call', 'data', 'rel' ...


@dataclass
class Insn:
    pc: int
    length: int
    fmt: str                       # mnemonic with {} for each Op
    ops: list = field(default_factory=list)
    fall: bool = True              # execution continues at pc+length
    targets: list = field(default_factory=list)  # code targets (jump/call)
    is_call: bool = False
    is_rst: int = -1


def _w(mem, a):
    return mem(a) | (mem(a + 1) << 8)


def _s8(v):
    return v - 256 if v >= 128 else v


def decode(mem, pc):
    """mem is a callable addr -> byte."""
    op = mem(pc)
    x, y, z = op >> 6, (op >> 3) & 7, op & 7
    p, q = y >> 1, y & 1

    if op == 0xCB:
        return _cb(mem, pc, pc + 1, None, 0)
    if op == 0xED:
        return _ed(mem, pc)
    if op in (0xDD, 0xFD):
        return _idx(mem, pc, "IX" if op == 0xDD else "IY")

    I = lambda n, f, ops=(), **kw: Insn(pc, n, f, list(ops), **kw)

    if x == 0:
        if z == 0:
            if y == 0:
                return I(1, "NOP")
            if y == 1:
                return I(1, "EX   AF,AF'")
            d = _s8(mem(pc + 1))
            t = (pc + 2 + d) & 0xFFFF
            if y == 2:
                return I(2, "DJNZ {}", [Op("addr", t, "jump")], targets=[t])
            if y == 3:
                return I(2, "JR   {}", [Op("addr", t, "jump")], fall=False, targets=[t])
            return I(2, "JR   %s,{}" % CC[y - 4], [Op("addr", t, "jump")], targets=[t])
        if z == 1:
            if q == 0:
                v = _w(mem, pc + 1)
                return I(3, "LD   %s,{}" % R16[p], [Op("imm16", v, "data")])
            return I(1, "ADD  HL,%s" % R16[p])
        if z == 2:
            tbl = {0: "LD   (BC),A", 1: "LD   A,(BC)", 2: "LD   (DE),A", 3: "LD   A,(DE)"}
            if y in tbl:
                return I(1, tbl[y])
            v = _w(mem, pc + 1)
            f = {4: "LD   ({}),HL", 5: "LD   HL,({})", 6: "LD   ({}),A", 7: "LD   A,({})"}[y]
            return I(3, f, [Op("addr", v, "data")])
        if z == 3:
            return I(1, ("INC  %s" if q == 0 else "DEC  %s") % R16[p])
        if z == 4:
            return I(1, "INC  %s" % R8[y])
        if z == 5:
            return I(1, "DEC  %s" % R8[y])
        if z == 6:
            return I(2, "LD   %s,{}" % R8[y], [Op("imm8", mem(pc + 1))])
        return I(1, ["RLCA", "RRCA", "RLA", "RRA", "DAA", "CPL", "SCF", "CCF"][y])

    if x == 1:
        if op == 0x76:
            return I(1, "HALT", fall=False)
        return I(1, "LD   %s,%s" % (R8[y], R8[z]))

    if x == 2:
        return I(1, ALU[y] + R8[z])

    # x == 3
    if z == 0:
        return I(1, "RET  %s" % CC[y])
    if z == 1:
        if q == 0:
            return I(1, "POP  %s" % R16AF[p])
        return [I(1, "RET", fall=False), I(1, "EXX"),
                I(1, "JP   (HL)", fall=False), I(1, "LD   SP,HL")][p]
    if z == 2:
        t = _w(mem, pc + 1)
        return I(3, "JP   %s,{}" % CC[y], [Op("addr", t, "jump")], targets=[t])
    if z == 3:
        if y == 0:
            t = _w(mem, pc + 1)
            return I(3, "JP   {}", [Op("addr", t, "jump")], fall=False, targets=[t])
        if y == 2:
            return I(2, "OUT  ({}),A", [Op("port", mem(pc + 1))])
        if y == 3:
            return I(2, "IN   A,({})", [Op("port", mem(pc + 1))])
        return I(1, {4: "EX   (SP),HL", 5: "EX   DE,HL", 6: "DI", 7: "EI"}[y])
    if z == 4:
        t = _w(mem, pc + 1)
        return I(3, "CALL %s,{}" % CC[y], [Op("addr", t, "call")], targets=[t], is_call=True)
    if z == 5:
        if q == 0:
            return I(1, "PUSH %s" % R16AF[p])
        t = _w(mem, pc + 1)
        return I(3, "CALL {}", [Op("addr", t, "call")], targets=[t], is_call=True)
    if z == 6:
        return I(2, ALU[y] + "{}", [Op("imm8", mem(pc + 1))])
    t = y * 8
    return I(1, "RST  {}", [Op("rst", t)], targets=[t], is_call=True, is_rst=t)


def _cb(mem, pc, at, ireg, disp):
    op = mem(at)
    x, y, z = op >> 6, (op >> 3) & 7, op & 7
    if ireg is None:
        n = 2
        reg = R8[z]
        ops = []
    else:
        n = 4
        if z != 6:
            return None  # undocumented copy-to-register forms
        reg = "(%s{})" % ireg
        ops = [Op("disp", disp)]
    if x == 0:
        if ROT[y] is None:
            return None
        return Insn(pc, n, "%-5s%s" % (ROT[y], reg), ops)
    name = ["", "BIT", "RES", "SET"][x]
    return Insn(pc, n, "%-5s%d,%s" % (name, y, reg), ops)


def _ed(mem, pc):
    op = mem(pc + 1)
    x, y, z = op >> 6, (op >> 3) & 7, op & 7
    p, q = y >> 1, y & 1
    I = lambda n, f, ops=(), **kw: Insn(pc, n, f, list(ops), **kw)
    if x == 1:
        if z == 0:
            return None if y == 6 else I(2, "IN   %s,(C)" % R8[y])
        if z == 1:
            return None if y == 6 else I(2, "OUT  (C),%s" % R8[y])
        if z == 2:
            return I(2, ("SBC  HL,%s" if q == 0 else "ADC  HL,%s") % R16[p])
        if z == 3:
            v = _w(mem, pc + 2)
            if q == 0:
                return I(4, "LD   ({}),%s" % R16[p], [Op("addr", v, "data")])
            return I(4, "LD   %s,({})" % R16[p], [Op("addr", v, "data")])
        if z == 4:
            return I(2, "NEG") if op == 0x44 else None
        if z == 5:
            if op == 0x45:
                return I(2, "RETN", fall=False)
            if op == 0x4D:
                return I(2, "RETI", fall=False)
            return None
        if z == 6:
            m = {0x46: 0, 0x56: 1, 0x5E: 2}
            return I(2, "IM   %d" % m[op]) if op in m else None
        t = {0: "LD   I,A", 1: "LD   R,A", 2: "LD   A,I", 3: "LD   A,R", 4: "RRD", 5: "RLD"}
        return I(2, t[y]) if y in t else None
    if x == 2 and z <= 3 and y >= 4:
        t = [["LDI", "CPI", "INI", "OUTI"], ["LDD", "CPD", "IND", "OUTD"],
             ["LDIR", "CPIR", "INIR", "OTIR"], ["LDDR", "CPDR", "INDR", "OTDR"]]
        return I(2, t[y - 4][z])
    return None


def _idx(mem, pc, r):
    op = mem(pc + 1)
    I = lambda n, f, ops=(), **kw: Insn(pc, n, f, list(ops), **kw)
    D = lambda: Op("disp", _s8(mem(pc + 2)))
    ix = "(%s{})" % r
    if op == 0xCB:
        return _cb(mem, pc, pc + 3, r, _s8(mem(pc + 2)))
    if op == 0x21:
        return I(4, "LD   %s,{}" % r, [Op("imm16", _w(mem, pc + 2), "data")])
    if op == 0x22:
        return I(4, "LD   ({}),%s" % r, [Op("addr", _w(mem, pc + 2), "data")])
    if op == 0x2A:
        return I(4, "LD   %s,({})" % r, [Op("addr", _w(mem, pc + 2), "data")])
    simple = {0x09: "ADD  %s,BC", 0x19: "ADD  %s,DE", 0x29: "ADD  %s,%s", 0x39: "ADD  %s,SP",
              0x23: "INC  %s", 0x2B: "DEC  %s", 0xE1: "POP  %s", 0xE3: "EX   (SP),%s",
              0xE5: "PUSH %s", 0xE9: "JP   (%s)", 0xF9: "LD   SP,%s"}
    if op in simple:
        f = simple[op]
        f = f % ((r, r) if f.count("%s") == 2 else r)
        return I(2, f, fall=(op != 0xE9))
    if op == 0x34:
        return I(3, "INC  " + ix, [D()])
    if op == 0x35:
        return I(3, "DEC  " + ix, [D()])
    if op == 0x36:
        return I(4, "LD   %s,{}" % ix, [D(), Op("imm8", mem(pc + 3))])
    x, y, z = op >> 6, (op >> 3) & 7, op & 7
    if x == 1 and z in (4, 5) and y not in (4, 5, 6):
        # odokumenterat LD r,IXH/IXL: z80asm kan det inte, skrivs som DEFB
        return I(2, "DEFB $%02X,$%02X  ; LD   %s,%s%s"
                 % (0xDD if r == "IX" else 0xFD, op, R8[y], r, "HL"[z - 4]))
    if x == 1 and op != 0x76:
        if z == 6:
            return I(3, "LD   %s,%s" % (R8[y], ix), [D()])
        if y == 6:
            return I(3, "LD   %s,%s" % (ix, R8[z]), [D()])
    if x == 2 and z == 6:
        return I(3, ALU[y] + ix, [D()])
    return None
