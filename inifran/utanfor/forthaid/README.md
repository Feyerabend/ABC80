# Forthaid: an open Smartaid in Forth (outside the book)

*Not part of the book.* An experiment to test a claim that volume 2
makes at the end of example 7: that a Forth at `$4000` would have been
another way than Smartaid's. Forthaid is a toolkit ROM in the style of
Smartaid 3. It is written in Forth on the interpreter from example 7 and
built for `$4000`–`$5FFF` (8 KB, no banking). Unlike Smartaid it is
open: the commands are Forth words, and users can write their own. How
to use it is described in `MANUAL.md` (in Swedish).

No code is borrowed from Smartaid. The Smartaid chapter in volume 2 was
used only to learn *what* the commands do and how such ROMs take over
the keyboard.

This is an English version of `LASMIG.md` ("läs mig", read me). File
and script names are Swedish: *gör* means make, *mätning* measurement,
*prov* test, *bygg* build, *kör* run.

| File           | What                                                                |
|----------------|---------------------------------------------------------------------|
| `gor.sh`       | builds `forthaid.bin` from copies of the book's sources; `-p` runs the tests |
| `fa.fs`        | the commands in Forth, hooking in (`PA`) and line input (`KBD`)     |
| `fak.inc`      | the new machine-code words                                          |
| `fai.asm`      | the keyboard interrupt and single-stepping, in assembler            |
| `MANUAL.md`    | the manual (Swedish): starting, commands, editor, stepping, Forth   |
| `prov/`        | 25 tests, each run in ROM 9913 and 11273 (`prov/kor.sh`)            |
| `matning.sh`   | timings of FIND, VAR and REN, and the size per command              |
| `forthaid.bin` | the ROM, 8 176 bytes                                                |


## How it is used

```
    Z=CALL(16384)       the commands in the Forth interpreter (entry 0, FORTH)
    Z=CALL(16384,1)     the same, keeping your own words
    Z=CALL(16384,2)     hooks the ROM in: the commands directly in BASIC
```

After `Z=CALL(16384,2)` the ROM reads lines in direct mode with its own
screen editor. A line that starts with one of its commands is run by
Forthaid; other lines go to the BASIC ROM as before. While a program
runs, CTRL-C pauses it, and CTRL-T and CTRL-S single-step it. `EX`
unhooks the ROM.

Commands: SYS, LIST, PEEK, FIND, VAR, DEL, AUTO, OLD, ED, START,
CHANGE, REN, HELP, FORTH and EX; in the Forth interpreter also WORDS,
BYE and your own words.

In the emulator: `emulator/abc80` and `emulator/abc80f` load it with
`-l utanfor/forthaid/forthaid.bin@4000`, and the web page has it under
the ROM choice (`?krets=forthaid`). It has not been tried on a real
ABC80.


## What is changed from the book

`gor.sh` copies the book's `e7k.inc`, `e7fc.py`, `e7.fs` and `e7i.fs`
(from `../../forth` in the public repository) to `bygg/` and builds
there; the book's files are never changed. In the copies:

- the book's example words (`MAIN`, `TIO`, `FIND` with `MATCH` and
  `LINE?`) and the entries are removed; `fa.fs` has its own, with FORTH
  as entry 0;
- the kernel's `BYE` is renamed `(BYE)`, so that `BYE` in `fa.fs` can
  return in two ways: as before, or to the ROM's end of program
  (`ENDPRG`, CLEAR and the ABC80 prompt) when a command has changed the
  program, since the line with `CALL` and its variable may then be gone;
- the kernel's `(LOOKUP)` is removed; `fak.inc` has its own, which
  ignores case (2026-10-26), so that `list`, `List` and `LIST` are the
  same command, both in BASIC and at OK. What follows the command is
  left as it is (`find a` searches for a lower-case a). The ROM grew by
  17 bytes, and the stepping tests run one loop shorter (`steg.out`,
  `steg2.emu.out`); `prov/gemen` tests the new behaviour.

New machine-code words in `fak.inc`: `*`, `U/MOD`, `DETOK` (a keyword
from the ROM's table), `MATCHVAR` (the next occurrence of a variable in
a line), `SETVARS` (a line's variables into VAR's table), `SEARCH` (a
string in another, for FIND), `J`, `CMOVE`, `ROMCALL`, `ENDPRG`, `(INS)`
(insert a line into the program), `I@` and `I!` (the I register),
`TOROM` (back into the command loop), `CMD?` (is the word a command), `(LOOKUP)` (the dictionary search without
regard to case, replacing the kernel's)
and `EDIT` with the screen editor `EDLIN`. The keyboard interrupt and
stepping are written in assembler in `fai.asm`, which goes last; the
vector table must be at `$5F34` (I = `$5F`), which `gor.sh` checks.

RAM:

| Where           | What                                                          |
|-----------------|---------------------------------------------------------------|
| `$7C78`–`$7EFE` | the interpreter's and the commands' variables, in the invisible bytes of the screen memory |
| `$7F7E`         | `STEPF`: nonzero while CTRL-T or CTRL-S is held down          |
| `$FB00`–`$FB4F` | the line buffer                                               |
| `$FB50`–`$FC7F` | your own words, 304 bytes                                     |
| `$FC80`–`$FCFF` | the stacks while the ROM reads a line and runs a command      |

`$FB00`–`$FCFF` are the cassette block buffers, which the BASIC ROM only
uses while a file is read or written. The stacks are used only while
Forthaid reads a line and runs one of its commands, and then no file is
open. At first they lay below BASIC's SP, and when a program filled the
memory they overwrote its end. Now Forthaid borrows nothing from BASIC:
with and without it, a program ends at the same address, and LIST,
FIND, VAR, REN and SYS work with full memory. 128 bytes is enough: on a
12 KB program the return stack went at most 20 of 64 bytes deep and the
data stack 46 of 62 (FIND), measured by filling the area with `$AA`.


## Estimate against measurement

`./matning.sh`, 2026-10-20, in the book's emulator with ROM 9913. Times
are the emulated machine's, for a program of the given length beyond
one of two lines (see the head of `matning.sh`). FIND looks for a
variable that is only on the last line, VAR without a name lists all
variables (81), VAR Z9 shows the line where Z9 is (Forthaid only), REN
10,10 renumbers everything and CHANGE replaces B1, on every eighth line,
with Q1 (`CHANGE B1 TO Q1` in Smartaid 3).

| Program  | ROM        | FIND Z9 | VAR    | VAR Z9 | REN 10,10 | CHANGE B1 |
|----------|------------|---------|--------|--------|-----------|-----------|
| 2 060 B  | Forthaid   | 0.35 s  | 0.60 s | 0.18 s | 0.41 s    | 0.18 s    |
|          | Smartaid 3 | 0.08 s  | 0.21 s | –      | 0.23 s    | 0.14 s    |
| 8 180 B  | Forthaid   | 1.50 s  | 1.05 s | 0.74 s | 2.21 s    | 0.72 s    |
|          | Smartaid 3 | 0.42 s  | 0.74 s | –      | 1.60 s    | 0.62 s    |
| 12 266 B | Forthaid   | 2.29 s  | 1.36 s | 1.12 s | 3.90 s    | 1.09 s    |
|          | Smartaid 3 | 0.66 s  | 1.10 s | –      | 3.00 s    | 0.93 s    |

Forthaid is 3.5–4.5 times slower with FIND, 1.2–3 times with VAR,
1.3–1.8 times with REN and 1.2–1.3 times with CHANGE. The gap shrinks
with program length, since part of it is fixed. REN is faster than FIND
because most of the work is done by the ROM's own routines.

FIND was at first 10 times slower: it turns each line into text with
`DETOK` and then compared the text in a Forth `DO` loop. The
machine-code word `SEARCH` now does the comparison, which made FIND 2.8
times faster and 23 bytes shorter. VAR was at first 15 times slower: for
each occurrence it searched a table of names already seen. Now every
possible variable has one bit in a 215-byte table (26 letters × 11
digits × 3 types × with or without index = 1 716 bits), set by
`SETVARS`, and the list comes out sorted without sorting. The table
needs 256 bytes between HEAP and STKTOP, otherwise VAR answers `?`.
`MATCHVAR` made CHANGE and VAR with a name 25 per cent faster; both it
and `SETVARS` walk the line with the shared routine `NVSUB`, using the
ROM's SKPELM and SKPEXP.

Size per command (`matning.sh`; a word counts to the command that alone
uses it, including machine-code words):

| Part                                  | Bytes     |
|---------------------------------------|-----------|
| SYS                                   |   194     |
| LIST                                  |    96     |
| PEEK                                  |   433     |
| FIND                                  |   255     |
| VAR                                   |   513     |
| DEL                                   |   129     |
| AUTO                                  |   145     |
| OLD                                   |   322     |
| CHANGE                                |   295     |
| REN                                   |   445     |
| ED                                    |   119     |
| START                                 |    56     |
| HELP                                  |   159     |
| EX, BYE                               |    65     |
| words shared by several commands      | 1 791     |
| hooking in (`PA`, `KBD`)              |   261     |
| interrupt and stepping (`fai.asm`)    |   212     |
| the book's interpreter, without the example words | 2 611 |
| free before the vector table at `$5F34` |  75     |
| **total**                             | **8 176** |

Against the estimate made before building (2026-10-20):

|                                  | Estimated            | Measured                                         |
|----------------------------------|----------------------|--------------------------------------------------|
| ROM, all                         | 6–8 KB               | 8 084 bytes, with the editor and stepping        |
| interpreter                      | 3 167 bytes          | 2 672 bytes (without the book's example words)   |
| commands in threaded code        | 3–4 KB               | 4 939 bytes, with shared and machine-code words  |
| FIND over 8–10 KB                | 0.5–1 s              | 4.2 s (8 KB), 1.5 s with `SEARCH`                |
| REN, a large program             | a few seconds        | 3.9 s (12 KB)                                    |
| Forth vs assembler in byte loops | 5–10 times slower    | FIND 10 times, 3.5–4.5 with `SEARCH`; VAR 15 times, 1.2–3 with the bit table (against Smartaid 3) |

Smartaid 3 is also 8 KB, but has room for eighteen commands and a
printer routine. Forthaid lacks RESUME, DIR, LIB, `FOR` in direct mode
and PR:; instead it has VAR with a name and an interpreter in which the
commands can be extended.


## Limitations

- **OLD** requires the ROM to be started with `PRINT CALL(16384)`:
  `Z=CALL(16384)` puts Z in the first line, which OLD is to recover.
  After hooking in (`Z=CALL(16384,2)`) this does not apply.
- **CHANGE** does not change the parameters of `DEF FN`.
- **No automatic start.** Smartaid 3 hooks itself in through the DOS
  ROM's cold-start slot (`$404B`); Forthaid is hooked in with
  `Z=CALL(16384,2)`.
- **The command loop is recognised on the stack.** The interrupt takes
  over only when `$FE24` is 1 and the four bytes below STKTOP are
  `$FE40` and `$00F0`, as the ROM's command loop leaves them before
  calling INLINE. 9913 and 11273 both do.
- **The pause.** The key that ends a pause is not passed on to the
  program (to `GET`, say).
- **304 bytes for your own words**, `$FB50`–`$FC7F`. The compiler does
  not check the limit; words beyond it are overwritten by the stacks.
- **`STEPF` at `$7F7E`–`$7F7F`**, the last invisible bytes of the screen
  memory. A program that uses them itself collides with Forthaid.
- **START** keeps the variables only as long as the program is not
  changed: the ROM clears them when a line is added or removed, and when
  the program ends.
- **ED** gives the ROM the line only up to the cursor; the rest must be
  taken with the right arrow, like the template with TAB in the ROM's ED.
- **8 101 of 8 192 bytes** are used: 75 are free before the vector table
  and 16 after `fai.asm`.


## A bug in the stepping

If the key was released in the middle of the ROM's TSTBRK (`$033E`),
between reading BRKFLG and clearing it, TSTBRK overwrote after the pause
the flag that CTRL-C had set, and the program did not stop (3–4 of 161
hold times). Now the pause checks the return address (`PSBRK` in
`fai.asm`, 22 bytes): if it lies in that window, TSTBRK starts over.
The program now stops at all 161 hold times in both ROMs.


## The tests

`prov/kor.sh` runs each test in 9913 and 11273 and compares the screen
with the saved one. Stepping needed two things the emulator did not
have, and it got both (the core is shared with the SDL window and the
web page):

- PIO A's interrupt follows bit 5 of the control word: `$B7` interrupts
  when a key is pressed, as the ROM sets it, and `$97` when it is
  released, as the stepping sets it;
- `\hN` in `-k`: hold the next key down for N ms.

In the public repository `kor.sh` uses `emulator/abc80`, with the ROMs
in `emulator/roms` (fetched with `hamta.py` there). The book's own host
emulator counts time in instructions (6 000 per 20 ms) and
`emulator/abc80` in T-states, so the programs in `steg` and `steg2` get
different distances; they have their own output, `steg.emu.out` and
`steg2.emu.out`. All 25 tests pass in both emulators.


## Conclusions

It can be done. A Smartaid in Forth fits in `$4000`–`$5FFF`, with the
same way of taking over the keyboard, a screen editor and stepping, and
it is open: the commands are words in an interpreter that users can add
their own words to. The claim at the end of example 7 holds in the
sense that the path existed.

The price is space and time. The interpreter takes about 2.7 KB of the
8, and what Smartaid 3 fits beyond Forthaid (four commands and the
printer) does not fit. Where the work was Forth loops over the
program's bytes, Forthaid was 10–15 times slower. Where the ROM does the
work, as in REN, the difference is small.

The remedy was the one the estimate assumed: machine-code words for the
hot loops. They also made the code shorter, 199 bytes in all. Forthaid
is now 1.2–1.8 times slower than Smartaid 3 on a 12 KB program, except
with FIND (3.5 times), where most of the time probably goes to the ROM's
`DETOK`; that is not measured. A Forth tool with its heaviest loops in
machine code comes close to one written entirely in assembler.

The measurements are made in the emulator; they have not been checked
against a real ABC80 or a real Smartaid 3.
