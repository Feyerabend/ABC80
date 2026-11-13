# Pascal på ABC 802

*(English below.)*

En Pascal med p-kod för ABC 802: kompilatorn är skriven i Pascal, och
tolken är skriven i Z80-assembler. Den går i BASIC-läget och under CP/M
(`PVM.COM`). Maskinen kompilerar och kör alltså Pascal själv.

- [`PASCAL.md`](PASCAL.md): upplägget, p-kodformatet, tolken och vad som
  är gjort.
- [`PASCAL800.md`](PASCAL800.md): anteckningar om Luxors Pascal 800, ur
  skivavbilden och körningar i emulatorn.
- `pc.pas`: kompilatorn för delmängden, skriven i delmängden själv;
  `pcs.pas`: kompilatorn för det större språket, med RECORD och WITH.
- `pvm.asm` och `preal.asm`: tolken; flyttalen kommer från FPP ur
  BBC BASIC (`fpp/`).
- `pc.py` och `pvm.py`: kompilatorn och tolken i Python, förlagorna;
  `pasm.py` och `pkod.py`: assemblern för p-kod och p-kodens instruktioner.
- `skiva.py`, `cpmfil.py` och `pdis800.py`: skivorna, och Pascal 800:s
  filer.
- `prov/`: provprogrammen, med de utskrifter som de ska ge.

`sh bygg.sh` bygger Pascal-skivan `pascal.img`. Bruksanvisningen är
[`../manualer/pascal.pdf`](../manualer/pascal.pdf), och kapitel 10 i
[`../inifran/`](../inifran/) beskriver den.

---

A p-code Pascal for the ABC 802. The compiler is written in Pascal and
the interpreter in Z80 assembly. It runs in BASIC mode and under CP/M.
See `PASCAL.md` (in Swedish) and `sh bygg.sh`.
