#!/usr/bin/env python3
"""felprov.py -- utanför boken: medvetna fel i a_sup och b_sup.

Varje fel nedan är en liten ändring i lappens källa: en prövning borta,
en instruktion utbytt. Lappen byggs med felet (ut/fel.rom), alla
kantfall i kant/prov.txt körs i den, och skärmarna jämförs med den
olappade ROM:ens (ut/kant_<fall>.a.txt eller .b.txt, som kantprov.sh
skriver; kör den först). Ett fel som inget kantfall märker är antingen
ofarligt (förklaringen står vid felet) eller ett hål i proven.

  python3 felprov.py [a_sup|b_sup] [fel ...]

Lappens källa återställs efteråt, också om något går fel.
"""
import os, subprocess, sys

HÄR = os.path.dirname(os.path.abspath(__file__))
os.chdir(HÄR)
V = "../../dis/04/host/abc80host"

# (namn, var, ny text, väntat). "var" är ett radnummer i lappen (raden
# prövas mot "rad") eller en text som ska finnas exakt en gång; ny text
# None tar bort raden. "väntat" säger om proven ska märka felet, och om
# inte, varför.
A_SUP = [
    ("h_lag",   63, "JR   NZ,NEJI", None, "märks (hel_d): bara den höga adressbyten prövad"),
    ("h_hog",   67, "JR   NZ,NEJI", None, "märks (sup_adrh): bara den låga adressbyten prövad"),
    ("h_geti1", 71, "JR   NZ,NEJI", None,
     "märks inte: PRIMARY skriver alltid GETI/GETF direkt efter en variabel"),
    ("h_geti2", 91, "JR   NZ,NEJI", None, "märks inte: som h_geti1, för X"),
    ("h_b7",   103, "JR   NZ,NEJI", None, "märks: utan prövningen av ASGNI"),
    ("h_sub_som_add", 124, "JR   NZ,SUPIS", None, "märks: subtraktionen blir addition"),
    ("h_ny_de", 107, "INC  HL", None, "märks: DE inte flyttad efter satsen"),
    ("h_konst_hog", 79, "LD   B,(HL)", "        LD   B,$00", "märks: konstantens höga byte 0"),
    ("h_hl_fel", 132, "LD   (HL),E", "        LD   (HL),E\n        INC  HL",
     "märks inte: bara LET och FOR tar emot tilldelningen, och ingen läser HL"),
    ("f_lag",  146, "JR   NZ,NEJF", None, "märks (sup_inte): bara den höga byten prövad"),
    ("f_hog",  150, "JR   NZ,NEJF", None, "märks (sup_adrf): bara den låga byten prövad"),
    ("f_getf1", 154, "JR   NZ,NEJF", None, "märks inte: som h_geti1"),
    ("f_getf2", 175, "JR   NZ,NEJF", None, "märks inte: som h_geti1"),
    ("f_b6",   180, "JR   NZ,NEJF", None, "märks: utan prövningen av ASGNF"),
    ("f_fsub_som_fadd", 189, "LD   IX,$3460", "        LD   IX,$33F7", "märks"),
    ("f_konst_bc", 162, "LD   C,L", "        LD   C,L\n        DEC  BC", "märks: exponenten en byte fel"),
    ("f_hoger_4", 211, "LD   C,$05", "        LD   C,$04", "märks: fyra byte i stället för fem"),
    ("f_ny_de", 184, "INC  HL", None, "märks: DE inte flyttad"),
    ("f_ej_tillbaka", 230, "LDDR", None, "märks: variabeln inte skriven"),
    ("f_b_1",  236, "LD   B,$00", "        LD   B,$01", "märks inte: som h_hl_fel, för B"),
]

B_SUP = [
    ("bara_lag", "        LD   A,(DE)\n        DEC  DE\n        CP   (HL)\n        JR   NZ,SXROM\n",
     "        LD   A,(DE)\n        DEC  DE\n", "märks (sup_adrh)"),
    ("bara_hog", "        LD   A,(DE)              ; samma referens?\n        CP   (HL)\n        JR   NZ,SXROM\n",
     "        LD   A,(DE)              ; samma referens?\n", "märks (hel_d)"),
    ("utan_b5", "        CP   $B5                 ; STIVAR\n        JR   NZ,SXROM\n",
     "        CP   $B5                 ; STIVAR\n", "märks (sup_b2, rad 60)"),
    ("sub_som_add", "        CP   $9C\n        JR   NZ,SXSUB\n", "        CP   $9C\n", "märks"),
    ("sub_omvand", "SXSUB:  PUSH HL                  ; V - X\n        LD   H,B\n        LD   L,C\n        POP  BC\n",
     "SXSUB:  NOP\n", "märks: X - V i stället för V - X"),
    ("de_fel", "        POP  DE                  ; efter satsen\n", "        POP  AF\n", "märks"),
    ("intcon_hog0", "SXLD:   LD   A,(HL)\n        INC  HL\n        LD   H,(HL)\n",
     "SXLD:   LD   A,(HL)\n        INC  HL\n        LD   H,0\n", "märks"),
    ("litet_fel", "        SUB  $DD\n        JR   C,SXX2\n", "        SUB  $DC\n        JR   C,SXX2\n", "märks"),
    ("hog_byte_ej", "        INC  HL\n        LD   (HL),B\n        DEC  HL\n", "        INC  HL\n        DEC  HL\n", "märks"),
    ("miss_de", "SXROM:  JP   LDIVAR", "SXROM:  DEC  DE\n        JP   LDIVAR", "märks"),
    ("x_ar_v", "        CALL VARREF              ; HL = X:s adress\n",
     "        INC  DE\n        INC  DE\n        POP  HL\n        PUSH HL\n        POP  BC\n        POP  HL\n        PUSH HL\n        PUSH BC\n",
     "märks: X läst som V"),
    ("utan_vff36", "        LD   (VFF36),HL\n", "", "märks inte: VFF36 läses ingenstans i ROM:en"),
    ("b_inte_1", "        LD   B,$01\n", "        LD   B,$00\n",
     "märks inte: satsslingan (L1D1D) skriver över B innan någon läser den"),
    ("c_fel", "        POP  DE                  ; efter satsen\n",
     "        POP  DE                  ; efter satsen\n        LD   C,$55\n", "märks inte: som b_inte_1, för C"),
    ("for_hl", "        DEC  HL\n        POP  DE", "        POP  DE",
     "märks inte: som b_inte_1, för HL (FOR går inte genom $1D1A)"),
    ("f_bara_lag", "        LD   A,(DE)\n        DEC  DE\n        CP   (HL)\n        JR   NZ,SFROM\n",
     "        LD   A,(DE)\n        DEC  DE\n",
     "märks inte och är inget fel: STFVAR läser målet ur koden, så A1=Q1+1 blir rätt (sup_b2g)"),
    ("f_utan_b4", "        CP   $B4                 ; STFVAR\n        JR   NZ,SFROM\n",
     "        CP   $B4                 ; STFVAR\n", "märks (sup_b2g, K=K+J-K)"),
    ("f_sub_som_add", "        CP   $9B\n        JR   Z,SFR\n        LD   HL,FSUB\n",
     "        CP   $9B\n        JR   Z,SFR\n", "märks"),
    ("f_konst_len", "        INC  A\n        ADD  A,L\n", "        ADD  A,L\n",
     "märks inte: mönstret hittas aldrig med en konstant, och ROM:en gör satsen som förut"),
    ("f_numpsh", "SFK:    SUB  $D4", "SFK:    SUB  $D5", "märks"),
    ("f_utan_ix", "        LD   IX,$20F4            ; NUMPSH:s returadress i NUMCON\n", "",
     "märks inte: inget efter satsen läser IX innan det sätts"),
    ("f_de", "        INC  DE                  ; förbi B4\n", "", "märks"),
    ("f_prgptr", "        LD   (PRGPTR),DE\n", "", "märks"),
    ("f_utan_fel", "        JP   C,FERR              ; spill: fel 130, variabeln orörd\n", "",
     "märks (sup_b2f)"),
    ("f_x_ar_v", "        CP   $C0\n        JR   NZ,SFK\n        CALL VARREF              ; en variabel, som LDFVAR\n",
     "        CP   $C0\n        JR   NZ,SFK\n        INC  DE\n        INC  DE\n        CALL $22E6\n", "märks"),
]

def andra(orig, var, rad, ny):
    if isinstance(var, int):
        r = orig.split("\n")
        if r[var - 1].strip().split(";")[0].strip() != rad:
            sys.exit("rad %d är %r, inte %r" % (var, r[var - 1], rad))
        r[var - 1] = "; borttagen: " + r[var - 1].strip() if ny is None else ny
        return "\n".join(r)
    if orig.count(var) != 1:
        sys.exit("%r finns %d gånger" % (var, orig.count(var)))
    return orig.replace(var, ny)

def kantfall():
    for l in open("kant/prov.txt"):
        if l.startswith("#") or "|" not in l:
            continue
        namn, prog = l.rstrip("\n").split("|", 1)
        # som kantprov.sh: vänta 1000 ms efter varje RETURN
        yield namn, prog.replace("\\r", "\\r\\w1000\\.")

def kor(sida, prog):
    if sida == "a":
        arg = ["-r", "ut/fel.rom", "-l", "ut/fel.krets@5000"]
    else:
        arg = ["-r", "ut/fel.rom", "-m", "32"]
    ut = subprocess.run([V] + arg + ["-t", "2000", "-k", prog],
                        capture_output=True, text=True).stdout
    return "\n".join(l for l in ut.split("\n")
                     if l.strip() and "instr.)" not in l and "STANNADE" not in l)

def main():
    lappar = {"a_sup": A_SUP, "b_sup": B_SUP}
    valda = sys.argv[1:2] or list(lappar)
    bara = set(sys.argv[2:])
    for lapp in valda:
        sida, fil = lapp[0], "lapp/%s.asm" % lapp
        orig = open(fil).read()
        try:
            for f in lappar[lapp]:
                namn = f[0]
                if bara and namn not in bara:
                    continue
                if len(f) == 5:
                    _, var, rad, ny, vantat = f
                else:
                    _, var, ny, vantat = f
                    rad = None
                open(fil, "w").write(andra(orig, var, rad, ny))
                subprocess.run(["python3", "lappa.py", sida, "fel", lapp],
                               check=True, capture_output=True)
                olika = [n for n, p in kantfall()
                         if kor(sida, p) != open("ut/kant_%s.%s.txt" % (n, sida)).read().strip()]
                print("%-6s %-16s %2d  %-40s %s" % (lapp, namn, len(olika),
                      " ".join(olika)[:40], vantat), flush=True)
        finally:
            open(fil, "w").write(orig)

main()
