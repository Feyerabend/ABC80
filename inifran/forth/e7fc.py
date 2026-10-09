#!/usr/bin/env python3
"""Korskompilator för exempel 7 (en minimal Forth för ABC80).

    python3 e7fc.py

Läser kärnan e7k.inc (assembler, ;W-rader för orden i maskinkod) och
Forth-källan och skriver en assemblerfil med kärnan, de ord som
behövs och ingångstabellen. Bara ord som nås från ingångarna tas med:
anropsgrafen är utan cykler (ett ord kan bara använda ord som redan
är definierade), så det räcker att följa den från ingångarna.

Fem bilder skrivs:
    e7.asm   e7.fs på $4000 (kretsen)
    e7r.asm  e7.fs på $C000 (RAM under programmet, sedan BOFA har
             flyttats upp med POKE 65052,0,196 och NEW)
    e7i.asm  e7.fs och tolken e7i.fs på $C000, med huvuden: alla ord
             tas med och får namn, så att tolken hittar dem
             (POKE 65052,0,208 och NEW, 4 KB)
    e7ip.asm samma tolk på $4000, som en krets (ett 2732-EPROM) i
             stället för Smartaid: bara de nya orden hamnar i RAM, från
             $C000 och uppåt (POKE 65052,0,208 och NEW)
    e7c.asm  tolken på $4000 med e7c.fs, som lägger nya ord i
             CMOS-minnet på $5000 (inte med i boken)

Forth-källan:
    : namn ... ;          kolonord
    IMMEDIATE             det senaste ordet körs i stället för att
                          kompileras (bara tolken bryr sig)
    n CONSTANT namn       konstant (blir LIT n där den används)
    VARIABLE namn         två bytes RAM (de dolda bytena i bildminnet)
    ENTRY namn            nästa ingång: CALL(a,n), a = ORG
    IF ELSE THEN  BEGIN UNTIL AGAIN WHILE REPEAT  DO LOOP
    ." text"  S" text"  [CHAR] x  ['] namn  \\ kommentar  ( kommentar )
['] ger adressen till ett ord eller en etikett i kärnan (DOCOL ...);
LATEST och SLUT är det sista huvudet och där nya ord börjar: bildens
slut, eller RAM på $C000 för kretsen.
Tal skrivs decimalt eller med $ för hex.
Ett ord som definieras igen (i en senare källa) ersätter det tidigare,
på samma plats i ordlistan, och de ord som redan använder det får det
nya.
"""
import re
import sys

KERN = "e7k.inc"
# (fil, ORG, källor, huvuden, var nya ord börjar: None = bildens slut)
OUTS = [("e7.asm", 0x4000, ["e7.fs"], False, None),
        ("e7r.asm", 0xC000, ["e7.fs"], False, None),
        ("e7i.asm", 0xC000, ["e7.fs", "e7i.fs"], True, None),
        ("e7ip.asm", 0x4000, ["e7.fs", "e7i.fs"], True, 0xC000),
        ("e7c.asm", 0x4000, ["e7.fs", "e7i.fs", "e7c.fs"], True, 0x5008)]

# RAM för VARIABLE: de 8 dolda bytena efter var tredje rad i bildminnet
# ($7C78-$7C7F osv.); $7C78 och $7C7A är SAVESP och S0, $7FF8- lämnas
# åt exempel 1.
RAM0 = [a for b in range(0x7C78, 0x7FF8, 0x80) for a in range(b, b + 8, 2)
        if a not in (0x7C78, 0x7C7A)]


def read_kernel(path):
    core, prims, cur = [], {}, None
    for line in open(path, encoding="utf-8"):
        m = re.match(r";W\s+(\S+)\s+(\w+)(.*)", line)
        if m:
            cur = m.group(1)
            prims[cur] = {"label": m.group(2), "deps": m.group(3).split(),
                          "lines": [line]}
        elif cur is None:
            core.append(line)
        else:
            prims[cur]["lines"].append(line)
    return core, prims


def tokens(text):
    """Ord och textargument; kommentarer tas bort."""
    out, i, n = [], 0, len(text)
    while i < n:
        while i < n and text[i].isspace():
            i += 1
        if i >= n:
            break
        j = i
        while j < n and not text[j].isspace():
            j += 1
        w = text[i:j]
        i = j
        if out and out[-1][0] == ":":    # namnet: alltid ett ord
            out.append((w.upper(), None))
        elif w == "\\":
            i = text.find("\n", i)
            i = n if i < 0 else i
        elif w == "(":
            i = text.index(")", i) + 1
        elif w in ('."', 'S"'):
            k = text.index('"', i + 1)
            out.append((w, text[i + 1:k]))
            i = k + 1
        else:
            out.append((w.upper(), None))
    return out


def number(w):
    try:
        return int(w[1:], 16) if w.startswith("$") else int(w)
    except ValueError:
        return None


def compile_source(toks, prims, asmlabels):
    words, consts, vars_, entries, imm = {}, {}, {}, [], set()
    ram = list(RAM0)
    it = iter(toks)
    pend = None                      # tal före CONSTANT
    nlab = [0]

    def newlab():
        nlab[0] += 1
        return "L%d" % nlab[0]

    for w, arg in it:
        if w == ":":
            name = next(it)[0]
            body, ctl = [], []
            for w2, arg2 in it:
                if w2 == ";":
                    body.append(("w", "EXIT"))
                    break
                n = number(w2)
                if w2 in ('."', 'S"'):
                    body.append(("s", arg2))
                    if w2 == '."':
                        body.append(("w", "TYPE"))
                elif w2 == "[CHAR]":
                    body.append(("n", ord(next(it)[0][0])))
                elif w2 == "[']":
                    x = next(it)[0]
                    if x in words or x in prims:
                        body.append(("xw", x))
                    elif x in asmlabels or x in [p["label"]
                                                 for p in prims.values()]:
                        body.append(("xl", x))
                    else:
                        sys.exit("e7fc: okänt ord %s efter ['] i %s" % (x, name))
                elif w2 == "IF":
                    l = newlab(); ctl.append(l)
                    body += [("w", "0BRANCH"), ("ref", l)]
                elif w2 == "ELSE":
                    l = newlab()
                    body += [("w", "BRANCH"), ("ref", l), ("lab", ctl.pop())]
                    ctl.append(l)
                elif w2 == "THEN":
                    body.append(("lab", ctl.pop()))
                elif w2 == "BEGIN":
                    l = newlab(); ctl.append(l); body.append(("lab", l))
                elif w2 == "UNTIL":
                    body += [("w", "0BRANCH"), ("ref", ctl.pop())]
                elif w2 == "AGAIN":
                    body += [("w", "BRANCH"), ("ref", ctl.pop())]
                elif w2 == "WHILE":
                    l = newlab()
                    body += [("w", "0BRANCH"), ("ref", l)]
                    ctl.insert(-1, l)
                elif w2 == "REPEAT":
                    body += [("w", "BRANCH"), ("ref", ctl.pop()),
                             ("lab", ctl.pop())]
                elif w2 == "DO":
                    l = newlab(); ctl.append(l)
                    body += [("w", "(DO)"), ("lab", l)]
                elif w2 == "LOOP":
                    body += [("w", "(LOOP)"), ("ref", ctl.pop())]
                elif w2 in consts:
                    body.append(("n", consts[w2]))
                elif w2 in vars_:
                    body.append(("n", vars_[w2]))
                elif w2 in words or w2 in prims:
                    body.append(("w", w2))
                elif n is not None:
                    body.append(("n", n))
                else:
                    sys.exit("e7fc: okänt ord %s i %s" % (w2, name))
            assert not ctl, "ofullständig struktur i " + name
            words[name] = body
            last = name
        elif w == "IMMEDIATE":
            imm.add(last)
        elif w == "CONSTANT":
            consts[next(it)[0]] = pend
        elif w == "VARIABLE":
            vars_[next(it)[0]] = ram.pop(0)
        elif w == "ENTRY":
            entries.append(next(it)[0])
        elif number(w) is not None:
            pend = number(w)
        else:
            sys.exit("e7fc: okänt ord %s utanför definition" % w)
    return words, entries, imm


def label(w, prims):
    if w in prims:
        return prims[w]["label"]
    return "F_" + "".join(c if c.isalnum() else "_%02X" % ord(c) for c in w)


def header(name, lab, prev, immediate):
    """Huvudet före koden: länk till föregående huvud, längd (bit 7 =
    IMMEDIATE) och namnet."""
    h = "H_" + lab
    s = "%s: DEFW %s\n        DEFB %d\n" % (h, prev, len(name) | (
        0x80 if immediate else 0))
    if re.fullmatch(r"[A-Z0-9+\-*/.,:<>=@!?#']+", name):
        s += '        DEFM "%s"\n' % name
    else:
        s += "        DEFB %s ; %s\n" % (",".join(str(ord(c)) for c in name),
                                         name)
    return h, s


def build(out_name, org, srcs, heads, dict0):
    core, prims = read_kernel(KERN)
    asmlabels = set(re.findall(r"(?m)^(\w+):", "".join(core)))
    asmlabels |= {"LATEST", "SLUT"}
    text = "".join(open(s, encoding="utf-8").read() for s in srcs)
    words, entries, imm = compile_source(tokens(text), prims, asmlabels)
    labs = {p["label"]: n for n, p in prims.items()}
    # Följ grafen från ingångarna (och EXECUTE/BYE, som BOOT använder,
    # BRANCH för stackprovet och ABORT, som STKERR kör).
    # Med huvuden tas allt med.
    need = set(words) | set(prims) if heads else set()
    todo = entries + ["EXECUTE", "BYE", "BRANCH"] + (
        ["ABORT"] if "ABORT" in words else [])
    while todo:
        w = todo.pop()
        if w in need and not heads:
            continue
        need.add(w)
        if w in words:
            for kind, x in words[w]:
                if kind in ("w", "xw"):
                    todo.append(x)
                if kind in ("n", "xw", "xl"):
                    todo.append("LIT")
                if kind == "xl" and x in labs:
                    todo.append(labs[x])
                if kind == "s":
                    todo.append('(S")')
        else:
            todo += [labs[d] for d in prims[w]["deps"] if d in labs]
            todo += [d for d in prims[w]["deps"] if d in prims]
        if heads:
            break                    # need är redan allt

    out = list(core)
    prev = "0"
    for name, p in prims.items():
        if name not in need:
            continue
        if heads:
            lines = p["lines"]
            k = 1
            while lines[k].startswith(";"):
                k += 1
            prev, h = header(name, p["label"], prev, False)
            out += lines[:k] + [h] + lines[k:]
        else:
            out += p["lines"]
    out.append("\n; Orden i Forth (från %s)\n" % " och ".join(srcs))
    order = [w for w in words if w in need]      # i källans ordning
    for w in order:
        lab = label(w, prims)
        out.append("\n; : %s%s\n" % (w, " IMMEDIATE" if w in imm else ""))
        if heads:
            prev, h = header(w, lab, prev, w in imm)
            out.append(h)
        out.append("%s: CALL DOCOL\n" % lab)
        for kind, x in words[w]:
            if kind == "w":
                out.append("        DEFW %s ; %s\n" % (label(x, prims), x))
            elif kind == "n":
                out.append("        DEFW LIT,%d\n" % x)
            elif kind == "xw":
                out.append("        DEFW LIT,%s ; ' %s\n" % (label(x, prims), x))
            elif kind == "xl":
                out.append("        DEFW LIT,%s\n" % x)
            elif kind == "s":
                out.append('        DEFW XSQ\n        DEFB %d\n' % len(x))
                if x:
                    out.append('        DEFM "%s"\n' % x)
            elif kind == "ref":
                out.append("        DEFW %s_%s\n" % (lab, x))
            elif kind == "lab":
                out.append("%s_%s:\n" % (lab, x))
    out.append("\n; Ingångarna: CALL(ORG,n) kör ord n\nENTRY:\n")
    for i, e in enumerate(entries):
        out.append("        DEFW %s ; %d %s\n" % (label(e, prims), i, e))
    out.append("\n; Efter ett stackfel (STKERR)\nABORTT: DEFW %s\n"
               % (label("ABORT", prims) if "ABORT" in words else "BYE"))
    out.append("LATEST: EQU %s\n" % prev)
    if dict0 is None:
        out.append("SLUT:\n")
    else:
        out.append("BILDSLUT:\n; Kretsen kan inte skrivas: nya ord läggs i RAM\n"
                   "SLUT:   EQU $%04X\n" % dict0)
    text = re.sub(r"(?m)^(\s+ORG\s+)\$4000", r"\g<1>$%04X" % org, "".join(out))
    open(out_name, "w", encoding="utf-8").write(text)
    ncode = len([w for w in need if w in prims])
    print("e7fc: %d ord i maskinkod, %d kolonord, %d ingångar -> %s"
          % (ncode, len(order), len(entries), out_name))


for o in OUTS:
    build(*o)
