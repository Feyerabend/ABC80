#!/usr/bin/env python3
"""kwfix.py IN UT -- STIL-regeln för \\kw (2026-10-12) på en kopia av en
listning för de publika PDF:erna; boken själv rättas för hand.

Citat med raka citattecken ({\\char34}...{\\char34}) i löptexten:
- redan inne i \\texttt{...} (strängar i kod, VAL("1E")): orörda
- med svenska ord (tre bokstäver eller fler i gemener), eller ett
  felmeddelandes ord i versaler (”NEXT” SAKNAS): orörda
- ett strängvärde (efter = eller som argument mellan ( , och , )):
  \\texttt{"..."}, med citattecknen
- annars kod eller skärmutskrift: \\texttt{...} utan citattecken, med
  inre \\texttt och länkar upplösta, så att hela koden står i ett stycke
Sedan slås \\texttt{CHR\\$}(321) ihop till \\texttt{CHR\\$(321)}.
Raderna i Verb (\\VL, \\SY) och kodfältet i \\CL lämnas orörda."""
import re
import sys

C = r"{\char34}"
Q = re.compile(r"\{\\char34\}(.*?)\{\\char34\}")
LINK = re.compile(r"\\hyperlink\{[^{}]*\}\{((?:[^{}]|\{[^{}]*\})*)\}")
TT = re.compile(r"\\texttt\{((?:[^{}]|\{[^{}]*\})*)\}")
ORD = re.compile(r"(?<![\\\w])[a-zåäö]{3,}")
# skärmutskrift på engelska (BASIC II): räknas inte som svenska ord
SKARM = re.compile(r"^\s*(?:Error|Abort|Stop|Found|in line)\b")
FELORD = re.compile(r"^ (?:SAKNAS|UTAN|EFTER)\b|\b(?:UTAN|EFTER|ÖKA) $")
# funktionsnamn + argument utan blanka: \texttt{CHR\$}(321)
ANROP = re.compile(r"\\texttt\{([A-Z][A-Z0-9]*(?:\\[$%])?)\}"
                   r"(\((?:[^()\s{}\\]|\{\\char34\}|\\texttt\{(?:[^{}]|\{\\char34\})*\})*\))")
# enstaka ställen som reglerna inte klarar
SARFALL = [
    (C + r"a {\char62} b" + C + r"\texttt{: COMP\%(}",
     r"\texttt{a {\char62} b}: \texttt{COMP\%}("),
    # BASIC II: INPUT LINES$ läses som INPUT LINE S$
    (r"\texttt{INPUT} \texttt{LINE} S\$", r"\texttt{INPUT LINE S\$}"),
    # negativa tal i strängar: inget \mbox inne i \texttt
    (C + r"\mbox{-}", C + "-"),
]


def plain(s):
    """inre märkning bort: \\hyperlink{x}{\\texttt{Y}} och \\texttt{Y} -> Y"""
    while True:
        t = TT.sub(r"\1", LINK.sub(r"\1", s))
        if t == s:
            return t
        s = t


def synlig(s):
    """blanka först och sist syns i skärmutskrift (”??? ”, ” 0”)"""
    if not s.strip():
        return "\\ " * len(s)
    n = len(s) - len(s.lstrip(" "))
    k = len(s) - len(s.rstrip(" "))
    return "\\ " * n + s.strip(" ") + "\\ " * k


def i_tt(t, pos):
    """står pos inne i argumentet till \\texttt eller \\kw?"""
    for m in re.finditer(r"\\(?:texttt|kw)\{", t[:pos]):
        d = 1
        for c in t[m.end():pos]:
            d += (c == "{") - (c == "}")
            if d == 0:
                break
        if d > 0:
            return True
    return False


def citat(t):
    def f(m):
        s = m.group(1)
        fore, efter = t[:m.start()], t[m.end():]
        if (i_tt(t, m.start()) or FELORD.search(efter[:8]) or FELORD.search(fore[-6:])
                or ORD.search(re.sub(r"\\[a-zA-Z]+", "", s)) and not SKARM.match(s)):
            return m.group(0)
        s = plain(s)
        if (fore.rstrip().endswith("=") or not s.strip() or
                fore.endswith(("(", ",")) and efter.startswith((",", ")"))):
            return r"\texttt{%s%s%s}" % (C, synlig(s), C)
        return r"\texttt{%s}" % synlig(s)
    return Q.sub(f, t)


def anrop(m):
    args = re.sub(r"\\texttt\{((?:[^{}]|\{\\char34\})*)\}", r"\1", m.group(2))
    return r"\texttt{%s%s}" % (m.group(1), args)


def kommentar(rad):
    """\\CL{kod}{kommentar}: index där kommentaren börjar, annars 0"""
    if not rad.startswith(r"\CL{"):
        return 0
    d = 0
    for i, c in enumerate(rad[3:], 3):
        if c == "{" and rad[i - 1] != "\\":
            d += 1
        elif c == "}" and rad[i - 1] != "\\":
            d -= 1
            if d == 0:
                return i + 1
    return len(rad)


def main(inn, ut):
    rader = open(inn, encoding="utf-8").read().split("\n")
    n = 0
    for j, rad in enumerate(rader):
        if rad.startswith((r"\VL", r"\SY")):
            continue
        b = kommentar(rad)
        t = rad[b:]
        for a, ny in SARFALL:
            t = t.replace(a, ny)
        t = ANROP.sub(anrop, citat(t))
        n += t != rad[b:]
        rader[j] = rad[:b] + t
    open(ut, "w", encoding="utf-8").write("\n".join(rader))
    print("kwfix: %d rader ändrade" % n, file=sys.stderr)


main(*sys.argv[1:])
