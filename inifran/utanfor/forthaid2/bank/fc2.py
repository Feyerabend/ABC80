#!/usr/bin/env python3
"""Korskompilatorn med banker (Forthaid II, steg 3 och 4; utanför boken).

    python3 fc2.py [-H | -h] [-n adr] [-m karta] [-s fil]
                   -e e7fc.py -k kärna.inc -o namn fil[@bank]...

Bygger på bokens e7fc.py (en kopia, -e), vars läsare och kompilator
används som de är; bara bygget (build) är nytt. En källfil utan @ hamnar
i den fasta delen på $4000, fil@n i bank n på $5000. Flera filer kan
dela bank. Ett ord som definieras igen hamnar i den sista filens bank.

-m karta: vilken bank orden hamnar i, oavsett fil, också orden i
maskinkod (;W i kärnan). Raderna är "n: ORD ORD ..." och \\ är en
kommentar. Ett ord som står i flera banker får en kopia i var och en,
och ett anrop inom banken går till bankens egen kopia; utifrån går
anropet till den första bankens. Raden "h: ORD ..." ger orden ett
fjärrord och ett huvud med -h också om de bara används i sin bank (ord
som ska gå att skriva vid OK).

Skriver namn.asm (den fasta delen) och namn_bN.asm för varje bank N,
från 0 till den högsta. En bankfil har hela den fasta delen först, så
att etiketterna där blir desamma, och sedan banken från $5000; den
fasta delen behöver inga adresser ur bankerna (fjärrordet slår upp
ordet i bankens tabell, se fb.inc). namn.banker får en rad per bank med
antalet ord och antalet i tabellen.

Ett anrop från ett ord i bank n till ett ord i bank n är direkt, också
ett anrop av den fasta delen. Ett anrop från den fasta delen eller från
en annan bank går genom ordets fjärrord i den fasta delen, och [']
och ENTRY ger alltid fjärrordet. Den fasta delens assembler (kärnan och
-s) kan nämna fjärrordet med dess etikett (F_ORD, FAR_etikett för ett
ord i maskinkod). -H: huvuden för alla ord, de bankade orden med huvudet
före fjärrordet i den fasta delen, så att ordlistan ligger där. -h: som
-H, men ett bankat ord har huvud och fjärrord bara om det anropas
utifrån. -n: där nya ord börjar (annars bildens slut). -s fil: läggs
sist i den fasta delen, efter ordlistan och ingångarna.
"""
import re
import sys

# Variablerna som i Forthaid II steg 1: gömställena som klarar sig med
# TKN80 i båda lägena ($7EFE är STEPF), sedan kassettbufferten.
RAM0 = ([a for b in (0x7CF8, 0x7DF8, 0x7EF8) for a in range(b, b + 8, 2)
         if a != 0x7EFE] + list(range(0xFB50, 0xFB6C, 2)))
BANKORG = 0x5000


def ladda(path):
    """Bokens e7fc.py utan bygget sist."""
    s = open(path, encoding="utf-8").read()
    s, n = re.subn(r"\nfor o in OUTS:\n    build\(\*o\)\n?$", "\n", s)
    if n != 1:
        sys.exit("fc2: hittar inte bygget sist i %s" % path)
    g = {"__name__": "e7fc"}
    exec(compile(s, path, "exec"), g)
    g["RAM0"] = RAM0
    return g


def las_karta(path):
    karta, synliga = {}, set()
    for rad in open(path, encoding="utf-8"):
        rad = rad.split("\\")[0].strip()
        if not rad:
            continue
        b, _, ord_ = rad.partition(":")
        for w in ord_.split():
            if b.strip() == "h":
                synliga.add(w.upper())
            else:
                karta.setdefault(w.upper(), []).append(int(b))
    return karta, synliga


def main(argv):
    heads, dict0, e7path, kern, namn, filer = None, None, None, None, None, []
    karta, synliga, svans = {}, set(), ""
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in ("-H", "-h"):
            heads = a
        elif a in ("-n", "-e", "-k", "-o", "-m", "-s"):
            i += 1
            v = argv[i]
            if a == "-n":
                dict0 = int(v[1:], 16) if v.startswith("$") else int(v, 0)
            elif a == "-e":
                e7path = v
            elif a == "-k":
                kern = v
            elif a == "-m":
                karta, synliga = las_karta(v)
            elif a == "-s":
                svans = open(v, encoding="utf-8").read()
            else:
                namn = v
        else:
            f, _, b = a.partition("@")
            filer.append((f, int(b) if b else None))
        i += 1
    if not (e7path and kern and namn and filer):
        sys.exit(__doc__)
    e7 = ladda(e7path)
    build(e7, namn, filer, heads, dict0, kern, karta, synliga, svans)


def build(e7, namn, filer, heads, dict0, kern, karta, synliga, svans):
    core, prims = e7["read_kernel"](kern)
    asmlabels = set(re.findall(r"(?m)^(\w+):", "".join(core)))
    asmlabels |= {"LATEST", "SLUT"}
    texter = [open(f, encoding="utf-8").read() for f, _ in filer]
    words, entries, imm = e7["compile_source"](
        e7["tokens"]("".join(texter)), prims, asmlabels)
    # Bankerna för varje ord: den sista filen som definierar det, eller
    # kartan. banker[w] är en lista, den första banken först.
    banker = {}
    for (f, b), t in zip(filer, texter):
        toks = e7["tokens"](t)
        for k in range(len(toks) - 1):
            if toks[k][0] == ":":
                if b is None:
                    banker.pop(toks[k + 1][0], None)
                else:
                    banker[toks[k + 1][0]] = [b]
    for w, bs in karta.items():
        if w not in words and w not in prims:
            sys.exit("fc2: %s i kartan finns inte" % w)
        banker[w] = bs
    labs = {p["label"]: n for n, p in prims.items()}

    # Som i e7fc.py: följ grafen från ingångarna; med huvuden allt.
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
            break
    banked = {w for w in need if w in banker}
    if banked:
        need.add("(FAR)")
        need |= {labs[d] for d in prims["(FAR)"]["deps"] if d in labs}
        if "(FAR)" in banked:
            sys.exit("fc2: (FAR) måste ligga i den fasta delen")

    def flab(w):                     # etiketten utanför banken
        if w in banked and w in prims:
            return "FAR_" + prims[w]["label"]
        return e7["label"](w, prims)

    def blab(w, b):                  # etiketten i bank b
        if w in prims:
            return prims[w]["label"]
        return "B%d%s" % (b, e7["label"](w, prims))

    def ref(x, b):
        """Etiketten för ordet x sett från bank b (None = fasta delen)."""
        if x in banked and b in banker[x]:
            return blab(x, b)
        return flab(x)

    # Vilka ord som behöver ett fjärrord: de som används utanför sina
    # banker, med ['] eller som ingång, de som den fasta delens assembler
    # nämner, och med -H alla.
    far = set(banked) if heads == "-H" else set()
    for w in need:
        if w not in words:
            continue
        for b in banker.get(w, [None]):
            for kind, x in words[w]:
                if x in banked and (kind == "xw" or b not in banker[x]):
                    far.add(x)
    far |= banked & set(entries)
    if synliga - banked:
        sys.exit("fc2: inte i en bank: %s" % " ".join(sorted(synliga - banked)))
    far |= synliga
    asmfast = "".join(core) + svans + "".join(
        "".join(p["lines"]) for n, p in prims.items()
        if n in need and n not in banked)
    nämnda = set(re.findall(r"\b(?:F|FAR)_\w+", asmfast))
    far |= {w for w in banked if flab(w) in nämnda}
    # En bankad maskinkod kan inte nämnas i den fasta delens assembler
    # eller i en annan banks, bara anropas genom fjärrordet.
    def kod(lines):
        return "".join(re.sub(r";.*", "", l) for l in lines)
    for w in banked & set(prims):
        lab = r"\b%s\b" % prims[w]["label"]
        if re.search(lab, kod(core) + kod(svans.splitlines(True))):
            sys.exit("fc2: kärnan eller -s nämner %s i en bank" % prims[w]["label"])
        for v, p in prims.items():
            if v in need and v != w and re.search(lab, kod(p["lines"][1:])) \
                    and not (v in banked and set(banker[v]) <= set(banker[w])):
                sys.exit("fc2: %s nämner %s i bank %s"
                         % (v, prims[w]["label"], banker[w]))
    order = [w for w in words if w in need]      # i källans ordning
    porder = [w for w in prims if w in need]
    nbank = max([b for _, b in filer if b is not None]
                + [b for bs in karta.values() for b in bs] + [-1]) + 1
    tabell = {b: [w for w in porder + order if w in far and banker[w][0] == b]
              for b in range(nbank)}
    plats = {w: 2 + 2 * tabell[banker[w][0]].index(w) for w in far}

    def stubbe(w):
        return "%s: CALL DOFAR\n        DEFB %d,%d\n" % (
            flab(w), banker[w][0], plats[w])

    def kropp(w, lab, b):
        out = ["%s: CALL DOCOL\n" % lab]
        for kind, x in words[w]:
            if kind == "w":
                out.append("        DEFW %s ; %s\n" % (ref(x, b), x))
            elif kind == "n":
                out.append("        DEFW LIT,%d\n" % x)
            elif kind == "xw":
                out.append("        DEFW LIT,%s ; ' %s\n" % (flab(x), x))
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
        return out

    out = list(core)
    prev = "0"
    for name in porder:
        p = prims[name]
        if name in banked:
            if name not in far:
                continue
            out.append("\n;W %s (bank %d)\n" % (name, banker[name][0]))
            if heads:
                prev, h = e7["header"](name, flab(name), prev, False)
                out.append(h)
            out.append(stubbe(name))
        elif heads:
            lines = p["lines"]
            k = 1
            while lines[k].startswith(";"):
                k += 1
            prev, h = e7["header"](name, p["label"], prev, False)
            out += lines[:k] + [h] + lines[k:]
        else:
            out += p["lines"]
    srcs = [f if b is None else "%s@%d" % (f, b) for f, b in filer]
    out.append("\n; Orden i Forth (från %s)\n" % ", ".join(srcs))
    for w in order:
        if w in banked and w not in far:
            continue
        lab = flab(w)
        out.append("\n; : %s%s%s\n" % (
            w, " IMMEDIATE" if w in imm else "",
            " (bank %s)" % ",".join(map(str, banker[w])) if w in banked else ""))
        if heads:
            prev, h = e7["header"](w, lab, prev, w in imm)
            out.append(h)
        if w in banked:
            out.append(stubbe(w))
        else:
            out += kropp(w, lab, None)
    out.append("\n; Ingångarna: CALL(ORG,n) kör ord n\nENTRY:\n")
    for i, e in enumerate(entries):
        out.append("        DEFW %s ; %d %s\n" % (flab(e), i, e))
    out.append("\n; Efter ett stackfel (STKERR)\nABORTT: DEFW %s\n"
               % (flab("ABORT") if "ABORT" in words else "BYE"))
    out.append("LATEST: EQU %s\n" % prev)
    if dict0 is None:
        out.append("SLUT:\n")
    else:
        out.append("BILDSLUT:\n; Kretsen kan inte skrivas: nya ord läggs i RAM\n"
                   "SLUT:   EQU $%04X\n" % dict0)
    out.append(svans)
    fast = "".join(out)
    open(namn + ".asm", "w", encoding="utf-8").write(fast)

    rader = []
    for b in range(nbank):
        bo = [fast, "\n; Bank %d på $%04X\nFASTSLUT:\n        DEFS $%04X-$\n"
              % (b, BANKORG, BANKORG),
              "        DEFB %d,%d ; bankens nummer, ord i tabellen\n"
              % (b, len(tabell[b]))]
        for w in tabell[b]:
            bo.append("        DEFW %s ; %d %s\n" % (blab(w, b), plats[w], w))
        for w in porder:
            if w in banked and b in banker[w]:
                bo.append("\n")
                bo += prims[w]["lines"]
        for w in order:
            if w in banked and b in banker[w]:
                bo.append("\n; : %s%s\n" % (w, " IMMEDIATE" if w in imm else ""))
                bo += kropp(w, blab(w, b), b)
        bo.append("BANKSLUT:\n")
        open("%s_b%d.asm" % (namn, b), "w", encoding="utf-8").write("".join(bo))
        nord = len([w for w in porder + order if w in banked and b in banker[w]])
        rader.append("%d %d %d\n" % (b, nord, len(tabell[b])))
    open(namn + ".banker", "w").write("".join(rader))
    ncode = len([w for w in need if w in prims])
    print("fc2: %d ord i maskinkod, %d kolonord (%d ord i banker, %d fjärrord),"
          " %d ingångar, %d banker -> %s"
          % (ncode, len(order), len(banked), len(far), len(entries), nbank,
             namn + ".asm"))


if __name__ == "__main__":
    main(sys.argv[1:])
