#!/usr/bin/env python3
"""Bygg en kommenterad disassemblering ur mallen och ROM:en:
    python3 bygg.py abc80 [ROM]      ->  ut/abc80rom.pdf   (ABC80:s BASIC-ROM, 11273)
    python3 bygg.py basicii80 [ROM]  ->  ut/basicii80.pdf  (BASIC II för ABC80)
På Windows: py bygg.py abc80.

Mallen (<namn>-mall.tex) har namnen och kommentarerna men inte koden; den
fylls i ur ROM:en (fyll.py), som måste ha mallens kontrollsumma. Utan ROM
hämtas den: ABC80:s fyra kretsar ZA3506-ZA3509 från abc80.net:s arkiv,
BASIC II:s två halvor därifrån också.

PDF:en sätts med den första som finns av tectonic, latexmk, xelatex och
lualatex (dokumentet använder fontspec, så pdflatex går inte); -m väljer:
    python3 bygg.py -m xelatex abc80
Med TeX Live eller MiKTeX behövs typsnitten TeX Gyre Pagella och
Inconsolata (paketen tex-gyre och inconsolata).

I abc80-förrådet hämtas band 1:s kapitel ur ../../band1 och stilen ur
../../gemensamt; i det publika förrådet ligger allt här (inifran.sh).
"""
import os, shutil, subprocess, sys, urllib.request

ARKIV = "https://www.abc80.net/archive/luxor/Prom/fw/ABC80/"
MOTORER = ["tectonic", "latexmk", "xelatex", "lualatex"]


def hamta(mal, filer):
    """Hämta filerna ur arkivet efter varandra till mal, om den inte finns."""
    if os.path.exists(mal):
        return
    data = b""
    for f in filer:
        print("hämtar " + ARKIV + f, flush=True)
        with urllib.request.urlopen(ARKIV + f) as r:
            data += r.read()
    with open(mal, "wb") as ut:
        ut.write(data)


def python(*arg):
    """Kör ett av skripten här med samma python, utdata som UTF-8."""
    env = dict(os.environ, PYTHONUTF8="1", PYTHONIOENCODING="utf-8")
    r = subprocess.run([sys.executable, *arg], env=env, stdout=subprocess.PIPE)
    if r.returncode:
        sys.exit(r.returncode)      # skriptet har redan sagt varför
    return r.stdout


def satt(motor, hv, sok, pdf):
    """Sätt hv till ut/<pdf>.pdf med motorn; sok är katalogerna att leta i."""
    if motor == "tectonic":
        cmd = ["tectonic", "-o", "ut"]
        for d in sok:
            cmd += ["-Z", "search-path=" + d]
        r = subprocess.run(cmd + [hv], stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True, errors="replace")
        for rad in r.stdout.splitlines():
            if not rad.startswith("note:") and not (
                    rad.startswith("warning:") and ("Overfull" in rad or "Underfull" in rad)):
                print(rad)
        return
    # TeX Live och MiKTeX: katalogerna i TEXINPUTS, tomt sist ger standardvägarna
    env = dict(os.environ, TEXINPUTS=os.pathsep.join(sok + [""]))
    flaggor = ["-interaction=nonstopmode", "-halt-on-error"]
    if motor == "latexmk":
        cmd = ["latexmk", "-xelatex", "-outdir=ut", "-silent"] + flaggor + [hv]
        laps = 1
    else:
        # innehåll, länkar och longtable behöver tre varv
        cmd = [motor, "-output-directory=ut"] + flaggor + [hv]
        laps = 3
    for _ in range(laps):
        r = subprocess.run(cmd, env=env, stdout=subprocess.PIPE,
                           stderr=subprocess.STDOUT, text=True, errors="replace")
        if r.returncode:
            # visa felet, inte hela loggen
            rader = r.stdout.splitlines()
            fel = [i for i, rad in enumerate(rader) if rad.startswith("!")]
            print("\n".join(rader[fel[0]:fel[0] + 8] if fel else rader[-30:]))
            sys.exit("%s stannade; hela loggen i ut/%s.log" % (motor, pdf))


def main():
    arg = sys.argv[1:]
    motor = None
    if arg[:1] == ["-m"]:
        motor, arg = arg[1], arg[2:]
    if not arg or arg[0] not in ("abc80", "basicii80"):
        sys.exit("python3 bygg.py [-m tectonic|latexmk|xelatex|lualatex] abc80|basicii80 [ROM]")
    namn = arg[0]
    rom = os.path.abspath(arg[1]) if len(arg) > 1 else None
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    os.makedirs(os.path.join("ut", "listningar"), exist_ok=True)

    if motor is None:
        motor = next((m for m in MOTORER if shutil.which(m)), None)
        if motor is None:
            sys.exit("hittar ingen TeX: installera tectonic, TeX Live eller MiKTeX "
                     "(se README.md)")
    elif not shutil.which(motor):
        sys.exit("hittar inte " + motor)

    if namn == "abc80":
        if rom is None:
            rom = os.path.join("ut", "abc80old.rom")
            hamta(rom, ["ABC80-ZA%d-11273.bin" % k for k in range(3506, 3510)])
        fylld = os.path.join("ut", "listningar", "fylld.tex")
        with open(fylld, "wb") as f:
            f.write(python("fyll.py", "abc80-mall.tex", rom))
        # kod i citat i skrivmaskinsstil, som i boken
        kw = "kwfix.py" if os.path.exists("kwfix.py") else os.path.join("..", "kwfix.py")
        python(kw, fylld, os.path.join("ut", "listningar", "abc80.tex"))
        os.remove(fylld)
        hv = "abc80rom.tex" if os.path.exists("abc80rom.tex") else os.path.join("..", "abc80rom.tex")
        sok = ["ut"] + [d for d in (os.path.join("..", "..", "band1"),
                                    os.path.join("..", "..", "gemensamt")) if os.path.isdir(d)]
        pdf = "abc80rom"
    else:
        if rom is None:
            rom = os.path.join("ut", "basicii80a.rom")
            hamta(rom, ["basicii_abc80_1.bin", "basicii_abc80_2.bin"])
        hv = os.path.join("ut", "basicii80.tex")
        with open(hv, "wb") as f:
            f.write(python("fyll.py", "basicii80-mall.tex", rom))
        sok = ["ut"]
        pdf = "basicii80"

    ut = os.path.join("ut", pdf + ".pdf")
    if os.path.exists(ut):
        os.remove(ut)
    satt(motor, hv, sok, pdf)
    if os.path.exists(ut):
        print("klart: %s (%s)" % (ut, motor))


main()
