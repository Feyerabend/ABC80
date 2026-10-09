
## towave

Text or program → ABC80 cassette (WAV) that the ABC80 can read with `LOAD CAS:`.
Text eller program → ABC80-kassett (WAV) som ABC80 kan läsa med `LOAD CAS:`.

[English](#english) · [Svenska](#svenska)

---

### English

| File | Job |
|------|-----|
| `uni2abc.py` | UTF-8 text → ABC80 text: ÄÖÅÉÜäöåéü¤ become 7-bit codes and line ends become CR |
| `abc2wav.c` | a file (BAS or BAC) → WAV, 44.1 kHz, 8-bit, mono |
| `alt/` | earlier sources: `abccas`/`abckonv` (Robert Juhasz 2008) and `abc80.c` (Stefano Bodrato, z88dk appmake) |

```sh
make
python3 uni2abc.py -i prog.txt -o PROG.BAS
./abc2wav -e PROG -i PROG.BAS -o prog.wav            # name on tape: PROG.BAS
./abc2wav -e PROG -f 4 -i PROG.BAC -o prog.wav       # name on tape: PROG.BAC
```

#### abc2wav

`abc2wav [-v] [-f hexflags] [-e name] [-i in] [-o out] [-h]`

Without `-i` it reads stdin. Without `-o` it writes to stdout.

| Option | Meaning |
|--------|---------|
| `-e name` | the file name on tape, 1–8 characters: a letter first, then letters and digits. Otherwise it is `DEFAULTS.BAS`. |
| `-f 1` | slightly faster (shorter bit cell, period 28) |
| `-f 2` | Juhasz's original settings (23,908 Hz). Takes precedence over `-f 1`. |
| `-f 4` | file type `BAC` instead of `BAS` |
| `-v` | tell what is being done |

Flags can be combined, for example `-f 5`.

The tape format is the one `wav2basic.py` in `../tobasic` reads:

- A name block comes first: `$FF $FF $FF` and the name.
- Then come data blocks: `$00`, the block number, and 253 bytes.
- Each block contains:
  - 256 zero bits
  - 3 × SYN
  - STX
  - 256 bytes
  - ETX
  - a checksum (the sum of the 256 bytes and ETX)

#### uni2abc.py

`uni2abc.py -i in.txt -o OUT.BAS [-v]`

The Swedish 7-bit code: `@`=É, `[`=Ä, `\`=Ö, `]`=Å, `^`=Ü, `` ` ``=é,
`{`=ä, `|`=ö, `}`=å, `~`=ü, `$`=¤.

Fixed: É did not become `@` before, because the table had È.

---

### Svenska

| Fil | Uppgift |
|-----|---------|
| `uni2abc.py` | UTF-8-text → ABC80-text: ÄÖÅÉÜäöåéü¤ blir 7-bitarskod och radslut blir CR |
| `abc2wav.c` | en fil (BAS eller BAC) → WAV, 44,1 kHz, 8 bitar, mono |
| `alt/` | äldre förlagor: `abccas`/`abckonv` (Robert Juhasz 2008) och `abc80.c` (Stefano Bodrato, z88dk appmake) |

```sh
make
python3 uni2abc.py -i prog.txt -o PROG.BAS
./abc2wav -e PROG -i PROG.BAS -o prog.wav            # namn på bandet: PROG.BAS
./abc2wav -e PROG -f 4 -i PROG.BAC -o prog.wav       # namn på bandet: PROG.BAC
```

### abc2wav

`abc2wav [-v] [-f hexflaggor] [-e namn] [-i in] [-o ut] [-h]`

Utan `-i` läses stdin. Utan `-o` skrivs till stdout.

| Flagga | Betydelse |
|--------|-----------|
| `-e namn` | filnamnet på bandet, 1–8 tecken: en bokstav först, sedan bokstäver och siffror. Annars blir det `DEFAULTS.BAS`. |
| `-f 1` | lite snabbare (kortare bitcell, period 28) |
| `-f 2` | Juhasz ursprungliga inställningar (23 908 Hz). Gäller före `-f 1`. |
| `-f 4` | filtyp `BAC` i stället för `BAS` |
| `-v` | berätta vad som görs |

Flaggorna kan läggas ihop, till exempel `-f 5`.

Bandformatet är detsamma som `wav2basic.py` i `../tobasic` läser:

- Först kommer ett namnblock, `$FF $FF $FF` och namnet.
- Sedan kommer datablock med `$00`, blocknummer och 253 byte.
- Varje block innehåller:
  - 256 nollbitar
  - 3 × SYN
  - STX
  - 256 byte
  - ETX
  - en kontrollsumma (summan av de 256 byten och ETX)

#### uni2abc.py

`uni2abc.py -i in.txt -o UT.BAS [-v]`

Den svenska 7-bitarskoden: `@`=É, `[`=Ä, `\`=Ö, `]`=Å, `^`=Ü, `` ` ``=é,
`{`=ä, `|`=ö, `}`=å, `~`=ü, `$`=¤.

Rättat: É gav tidigare inte `@`, eftersom tabellen hade È.
