# Forthaid II: Forthaid with TKN80 (outside the book)

*Not part of the book.* Forthaid (`../forthaid/`) is a toolkit ROM in
the style of Smartaid 3, written in Forth on the interpreter from
example 7 of volume 2. Forthaid II is the same ROM moved out of the way
of TKN80, MyAB's card for 80 characters per line, and given more room by
banking in the threaded interpreter. It has all of Forthaid's commands,
a screen editor that knows about 80 columns, and two new commands,
SAVEW and LOADW, that keep your own Forth words on disk. How to use it
is described in `MANUAL.md` (in Swedish).

**The banked card never existed.** It is made up for the experiment and
exists only in the emulator and on the web page. Forthaid II has not
been tried on a real ABC80.

The experiment answers two questions (`PLAN.md`):

1. What does it cost to move Forthaid out of TKN80's way?
2. What does banking look like in a threaded interpreter, where every
   word is already an address in a list, and what does a call across the
   bank boundary cost?

No code is borrowed from Smartaid. Forthaid itself is not changed:
`gor.sh` copies its sources and the book's, and changes only the copies.

This is an English version of `LASMIG.md` ("läs mig", read me). File
and script names are Swedish: *gör* means make, *mätning* measurement,
*prov* test, *bank* bank, *kort* card, *fil* file, *banker* banks.

| File               | What                                                              |
|--------------------|-------------------------------------------------------------------|
| `gor.sh`           | builds `forthaid2.bin` and the banks from copies of Forthaid's and the book's sources; `-1` builds step 1, `-p` runs the tests |
| `banker.txt`       | which bank each of Forthaid's words goes into                     |
| `bank/fc2.py`      | the cross-compiler with banks, on the book's `e7fc.py`            |
| `bank/fb.inc`      | the far words in the kernel: DOFAR and the way back               |
| `bank/fil.fs`, `bank/fil.inc` | SAVEW and LOADW, and FCALL, which calls the ROM's file routines |
| `bank/prov.sh`     | the test of the far words, with example 7's interpreter and two banks; times the calls |
| `kort/prov.sh`     | the test of the bank card: switches banks from BASIC and reads back |
| `prov/`            | Forthaid's tests and new ones; `prov/steg1/` has what differs in step 1 |
| `prov/fil.sh`      | the test of SAVEW and LOADW, with the DOS and a copy of the example disk |
| `matning.sh`       | timings of FIND, VAR, REN and CHANGE against Forthaid and Smartaid 3, the calls across the bank boundary, and the size |
| `MANUAL.md`        | the manual (Swedish)                                              |
| `PLAN.md`          | the plan, with the steps                                          |
| `forthaid2.bin`    | the fixed part, 4 080 bytes at `$4000`–`$4FEF`                    |
| `forthaid2_b0.bin` … `_b4.bin` | the banks at `$5000`                                  |
| `steg1.bin`        | step 1, the ROM without banks, 6 128 bytes at `$4000`–`$57EF`     |


## How it is used

As Forthaid:

```
    Z=CALL(16384)       the commands in the Forth interpreter
    Z=CALL(16384,1)     the same, keeping your own words
    Z=CALL(16384,2)     hooks the ROM in: the commands directly in BASIC
```

Commands: SYS, LIST, PEEK, FIND, VAR, DEL, AUTO, OLD, ED, START,
CHANGE, REN, SAVEW, LOADW, HELP, FORTH and EX; in the Forth interpreter
also WORDS, BYE and your own words. With TKN80 in 80-column mode the
screen editor, ED and LIST use all 80 columns.

In the terminal emulator (from this directory):

```
../../emulator/abc80 -l forthaid2.bin@4000 -b forthaid2_b0.bin,forthaid2_b1.bin,forthaid2_b2.bin,forthaid2_b3.bin,forthaid2_b4.bin@5000
```

with `-tkn80 ../../emulator/roms/tkn80-iii.rom` for TKN80. On the web
page: `?krets=forthaid2`, with `&tkn80=pa` and `&skiva=system` (a disk
drive for SAVEW and LOADW). The SDL window has no bank card.


## Why Forthaid does not work with TKN80

TKN80 has its screen memory at `$5800`–`$5FFF`, and the upper half is
the same memory as the ABC80's screen at `$7C00`–`$7FFF`. Forthaid
collides with the card in three ways: the ROM fills `$4000`–`$5FFF`, so
its last 2 KB lie at the card's addresses; its interrupt vector table
lies at `$5F34`; and its variables lie in the invisible bytes of the
screen memory at `$7C78`–`$7F7F`, which in 80-column mode are in the
middle of lines 12–15. Only `$7xF8`–`$7xFF` are invisible in both modes.


## Step 1: without banks (question 1)

Forthaid uses 8 101 of 8 192 bytes; below `$5800` there are 6 144. About
a quarter of the ROM has to go. VAR and CHANGE share most of their code,
so one of them alone does not help. The choice was VAR, CHANGE, REN and
OLD, with 19 helper words; the ROM keeps SYS, LIST, PEEK, FIND, DEL,
AUTO, ED, START, HELP, the editor and the stepping, with 2 bytes free.
The variables moved to `$7CF8`–`$7EFF` and `$FB50`–`$FB6F`, your own
words to `$FB70`.

So Forthaid without banking can run with TKN80, but only by losing the
commands for variables and line numbers, the ones a Smartaid user
probably misses most. That is the reason for the banks.


## The bank card

Up to 8 banks of 2 KB in a window at `$5000`–`$57FF`; a write to
`$5040`+n selects bank n; the window is read-only like a ROM, and bank 0
is selected at start. It is the card the emulator's `-b` flag already
had (example 4 in volume 2 uses it). The way of selecting a bank is the
same as Super Smartaid's (`$4040`, `$4041`), moved to the window; it was
chosen because the emulator and the web page already had it. The card
has no RAM, and it cannot be read back, so each bank carries its own
number at `$5000`.


## Far words (question 2)

A word in a bank is called from the fixed part or from another bank
through a *far word* in the fixed part, five bytes:

    F_C1:   CALL DOFAR
            DEFB 1,2         ; bank 1, entry 2 in the bank's table

Each bank starts with its number at `$5000`, the number of entries at
`$5001` and from `$5002` a table of the addresses of the words that are
called from outside. DOFAR does what DOCOL does but also pushes the
current bank on the return stack, selects the new bank, looks the word
up in the table and enters it with a return address FARX. When the word
ends, its EXIT returns to FARX, whose FAREXIT selects the old bank again
and exits to the caller. The word in the bank does not know it was
called from outside; calls within a bank and calls to the fixed part are
ordinary addresses, as before. DOFAR, FARX and FAREXIT are 61 bytes.

The table means the fixed part needs no addresses from the banks, so a
bank can be rebuilt without touching the fixed part.

`bank/fc2.py` loads the book's `e7fc.py` and uses its reader and
compiler as they are; only the build is new. Source files are given a
bank (`file.fs@1`), and `banker.txt` (`-m`) places single words. A word
gets a far word only if it is called from the fixed part or another
bank, used with `[']`, is an entry, or is listed on an `h:` line (words
the manual says can be typed at OK). Machine-code words can lie in a
bank as well; `fc2.py` checks that nothing jumps to them directly.

| Call                                | T-states per call |
|-------------------------------------|------------------:|
| within the fixed part               |               335 |
| within a bank                       |               335 |
| from bank 1 to bank 2               |               754 |
| from the fixed part to bank 1       |               760 |

A call across the boundary takes about twice as long as an ordinary one,
roughly 250 µs against 112 µs. But with the commands placed as in
`banker.txt`, each command makes one call across the boundary however
long the program is (VAR with a name two more per matching line), since
the boundary lies between the commands and not inside their loops. In
the measured times it does not show.

Limits: data in a bank exists only while the bank is selected, and a
word that drops its caller's return address (`R> DROP`) does not work
across the boundary. Forthaid does neither.


## The banks

| Bank | What                                             | Bytes | Free |
|-----:|--------------------------------------------------|------:|-----:|
| –    | fixed part: kernel, interpreter, hooking in, stepping, far words | 4 080 | 413 before the vector table, 16 after |
| 0    | LIST, PEEK, FIND, SYS, START, ED, DEL            | 1 971 |   77 |
| 1    | VAR and CHANGE                                   | 1 145 |  903 |
| 2    | REN, AUTO, OLD and HELP                          | 1 365 |  683 |
| 3    | the compiler (`:`, `;`, IF, DO, VARIABLE …)      |   485 | 1 563 |
| 4    | SAVEW and LOADW                                  | 1 781 |  267 |

Forthaid II is 10 414 bytes in all, 45 far words. Small helper words
(PEEKC, SKIPBL, FINDLN, GETNUM, RANGE) get a copy in each bank that uses
them, which costs a few bytes and saves a far call every time.

The compiler in bank 3 is reached through its far words, also the
IMMEDIATE ones: the bit is in the header, which lies in the fixed part.
What the compiler lays down (`0BRANCH`, `(DO)`, DOCOL, `EXIT`) are
addresses in the fixed part, so your own words never need bank 3 when
they run.


## 80 columns

Forthaid's editor and LIST count 40 characters per line in four places.
With TKN80 in 80-column mode and a line longer than 40 characters, ED
moved the cursor up two lines and read the wrong one (ERR 31). The line
length is read from the ROM's line table (row 0 starts at `$58xx` in
80-column mode, `PEEK(885)`), each time a command runs, so the mode can
change between commands. The fix is in bank 0; step 1 has no room for
it.


## Your own words on disk

`SAVEW name` writes your own words to a file: an 8-byte header (`FW`,
LATEST, the length, the last word's header), the words with every
address of one of your words made relative to the start of the file,
and a bitmap with one bit per byte marking where such an address starts.
LATEST identifies the fixed part, so a file from another Forthaid II is
refused. `LOADW name` reads the words to RAM just below STKTOP and
lowers STKTOP; later LOADWs reuse the same memory, so BASIC loses only
as much as the largest file. The ROM's own OPEN, CLOSE, DVGET and DVPUT
are called through FCALL with a stack of its own, since the DOS needs
more stack than fits below Forth's. The cassette is refused, because its
buffers at `$FB00`–`$FCFF` hold your own words. Variables and constants
are saved as they are, so a constant holding the address of one of your
words points wrong after LOADW.


## Measurement

`./matning.sh`, 2026-10-26, ROM 9913, the same programs as Forthaid's.

| Program  | ROM              | FIND Z9 | VAR    | VAR Z9 | REN 10,10 | CHANGE B1 |
|----------|------------------|---------|--------|--------|-----------|-----------|
| 12 266 B | Forthaid II      | 2.29 s  | 1.36 s | 1.12 s | 3.90 s    | 1.09 s    |
|          | II, TKN80 80 col | 2.29 s  | 1.37 s | 1.14 s | 3.91 s    | 1.10 s    |
|          | Forthaid         | 2.29 s  | 1.36 s | 1.12 s | 3.90 s    | 1.09 s    |
|          | Smartaid 3       | 0.66 s  | 1.10 s | –      | 3.00 s    | 0.93 s    |

Forthaid II takes as long as Forthaid, to within 10 ms. With TKN80 in
80-column mode the commands take 0–20 ms longer, independent of program
length; that is output through TKN80's routines, not the banking.


## The tests

`prov/kor.sh` runs 26 tests in 9913 and 11273, without TKN80 and with
TKN80 in both modes, 166 runs; in 40-column mode the screen must be the
same as without the card, in 80-column mode the same characters apart
from line breaks. Tests whose names end in 80 run only in 80-column
mode. `kor.sh -1` runs step 1 (120 runs), `bank/prov.sh` the far words
and `kort/prov.sh` the card. `prov/fil.sh` tests SAVEW and LOADW with
the DOS, in both ROMs and with TKN80; the book's host emulator has no
disk drive, so it uses the emulator in `emulator/`. All pass.


## Conclusions

Moving Forthaid out of TKN80's way without banks costs a quarter of
it: the commands for variables and line numbers. With a bank card,
everything fits again, with room to spare: about 3.9 KB are free in the
fixed part and the five banks, and three more banks would fit on the
card.

Banking in a threaded interpreter is a small change: one new code field
(DOFAR), a return path, and a cross-compiler that knows which bank each
word is in and inserts the far words itself. The words in the banks are
written as before and do not know they are banked. A call across the
boundary costs about 420 T-states extra, but placed between commands
rather than inside their loops it does not show in the times.

The measurements are made in the emulator. Neither the card nor
Forthaid II has run on a real ABC80.
