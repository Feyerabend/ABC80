# Two interpreters, the same improvements? (outside the book)

*Not part of the book.* A counterfactual experiment. Suppose the ABC80
BASIC (1978) and BASIC II for the ABC80 (the ABC800 interpreter, moved
to the ABC80 around 1984-86) had been given one more round of work: what
would have made them faster, and does the same thing help both? The
experiment measures the interpreters with the same tests in the
emulator, attributes the time to the ROM routines, and tries changes in
patched copies of the ROMs. The emphasis is on the ABC80. The book's
files and the ROMs in `emulator/roms/` are not changed.

This is an English version of `LASMIG.md` ("läs mig", read me). File
and script names are Swedish: *prov* means test, *lapp* patch, *kant*
edge, *fel* error or fault, *ut* output, *allt* everything, *bok* book.
Patch names start with `a_` for the ABC80 and `b_` for BASIC II.

## The main points

- *The ABC80 BASIC could have run 14-31 % faster without any change to
  the language.* It takes seven changes, about 730 bytes of code and
  512 bytes of tables in an add-on ROM at `$5000`, plus a few jumps in the
  old ROM. Every test gives the same screen, the same errors and the
  same numbers (B1 and FIZZ1 gain only 3.5-3.8 %).
- *The time is spent in three places.* They are the BCD division, the
  statement loop with the variables, and the assignment `V=V+X`, which
  is a large part of the inner loops.
- *Where interpretation is what costs, the ABC80 becomes as fast as
  BASIC II.* This holds also when BASIC II is patched the same way:
  NOEL1 6.42 s against 6.42, and SIEVE 20.9 against 25.4. The BASIC II
  interpreter is not fundamentally faster.
- *Where the numbers are what costs, BASIC II stays far ahead.* B8 takes
  11.3 s against 2.9, and MANDEL 139.6 against 92.6. The difference is
  BCD against binary floating point, and no patch to the interpretation
  can touch it.
- *The same technique helps both, but by different amounts.* The
  `V=V+X` pattern gives 23 % in the ABC80 and 17 % in BASIC II (NOEL1).
  The loop in the array index gives 14 % in BASIC II (SIEVE), and the
  ABC80 does not need it. NEXT with HL instead of IX gives almost
  nothing.
- *A complete rewrite of the ABC80 BASIC would not have paid off for
  the interpretation.* It would only have bought the floating point,
  and the price would have been compatibility: the BAC files, the
  add-on ROMs and the decimals of the numbers. See "Would a rewrite
  have paid off?" below.

## Preconditions

- *The emulator, not a real machine.* The time is the emulator's
  T-states at 2.9952 MHz, from the moment CR after `RUN` is pressed
  until the interpreter is back in `READY` (`$00CC` in the ABC80, `$0110` in
  BASIC II). Everything written to the screen is included. BASIC II in
  the emulator agrees with the author's measurements on a real ABC 802:
  B3 5.98 against 5.91 s, B7 19.83 against 19.84, NOEL1 7.84 against
  8.01 and MANDEL 92.6 against 93. It has not been checked against a
  real ABC80.
- *Memory.* The ABC80 has 16 KB of RAM and BASIC II 32 KB.
- *The ABC80 ROM.* The changes apply to the old ROM (abc80old). The new
  one runs equally fast in every test.
- *Where the changes go.* The ABC80 ROM is nearly full. The new
  routines are therefore placed in a chip at `$5000`, like an add-on ROM,
  and the ROM only gets the jumps that lead there. That is one more chip
  in the machine. In BASIC II the patches are at `$0450-$05D3`. Only
  zeros are there (`$044A-$06A9`), because the ABC800 M cassette driver
  CAS: was removed in the ABC80 version. `$7800-$7FFF` is `$FF` in the ROM
  file but not free in the machine; see "What turned up along the way".
- *Nothing in the language changes.* Numbers are computed with the same
  BCD and the same rounding, and the code written by the compiler is
  the same. No new expression code is added: `V=V+X` is a pattern the
  interpreter recognises in the old code. What changes is how the ROM
  gets to the same result.
- *Changes that were possible in 1978.* No new algorithm is used, only
  HL instead of (IX+n), tables instead of loops, one test less per
  statement, and the pattern.
- *The tests.* The tests are 16 programs from `ABC80/abc802` in the
  public repository. B1-B7 are Rugg/Feldman (Kilobaud 1977), B8 added by PCW
  1978 with 100 iterations; NOEL and NOEL1 are unchanged. The others are
  changed so that the ABC80 can run them:
  - `MOD(A,B)` is `A-INT(A/B)*B` and `WHILE` is `IF`/`GOTO`;
  - the indentation and the timing (`TIME$`, `EXTEND`) are removed;
  - variable names have one letter and one digit;
  - INTER goes to 100 instead of 1000.

  The output is the same in both interpreters. How much another program
  gains depends on what it does. The changes themselves are tested
  against much more than the benchmarks (see "The changes do not just
  work for the benchmarks").

## The files

| File          | What                                                        |
|---------------|-------------------------------------------------------------|
| `allt.sh`     | reruns everything below in order, output in `ut/allt.txt`   |
| `prof.c`      | host on the emulator core: types in a program, RUN, measures T-states and instructions per address (`MINNE=file` saves memory afterwards) |
| `kor.sh`      | runs the tests in the old and new ABC80 ROM and in BASIC II |
| `profil.py`   | sums a profile per routine (names from the listings)        |
| `handelser.py`| counts lines, statements and expression codes per test      |
| `lappa.py`    | builds patched ROMs (and for the ABC80 a chip at `$5000`)     |
| `lapp/*.asm`  | the changes, one file each, with a description at the top   |
| `lapptid.sh`  | runs the tests with a patched ROM and compares time and screen |
| `prov/*.BAS`  | the tests                                                   |
| `kantprov.sh` | the edge cases in `kant/prov.txt` in the unpatched and patched ROM |
| `felprov.py`  | deliberate faults in `a_sup` and `b_sup`, run against the edge cases |
| `fprov.c`     | FMUL and FDIV called directly in both ROMs, random and edge-case numbers |
| `mulprov.c`   | `b_mul` against BASIC II's MUL16, register by register      |
| `bokprov.sh`  | the book's interpreter tests (`../../dis/04/host/tests`) with the patched ROMs |

The programs are built with the emulator core (`../../emulator/karna`)
and the patches with `../../dis/build/z80asm`. `./allt.sh` reruns
everything; it takes a good while.

## How fast they are

`./kor.sh`, 2026-10-11 (seconds; the ratio is ABC80 / BASIC II):

| Test   | ABC80 (old) | ABC80 (new) | BASIC II | Ratio |
|--------|---------|---------|---------|------|
| B1     |   0.958 |   0.958 |   0.858 | 1.12 |
| B2     |   2.115 |   2.115 |   1.814 | 1.17 |
| B3     |  10.853 |  10.853 |   5.977 | 1.82 |
| B4     |  11.827 |  11.827 |   5.998 | 1.97 |
| B5     |  12.246 |  12.246 |   6.440 | 1.90 |
| B6     |  17.485 |  17.485 |  11.683 | 1.50 |
| B7     |  23.574 |  23.574 |  19.833 | 1.19 |
| B8     |  13.318 |  13.318 |   2.886 | 4.61 |
| FIZZ   |   3.573 |   3.573 |   2.229 | 1.60 |
| FIZZ1  |   1.403 |   1.403 |   1.473 | 0.95 |
| INTER  |  12.929 |  12.929 |   6.728 | 1.92 |
| MANDEL | 169.131 | 169.131 |  92.573 | 1.83 |
| NOEL   |  24.418 |  24.418 |  18.766 | 1.30 |
| NOEL1  |   9.333 |   9.333 |   7.839 | 1.19 |
| PRIMES |  93.562 |  93.564 |  44.262 | 2.11 |
| SIEVE  |  26.312 |  26.312 |  30.655 | 0.86 |

BASIC II gains most on floating point: B8, with `^`, `LOG` and `SIN`,
runs 4.6 times faster, because the ABC80 computes in BCD. With integers
the interpreters are nearly equal, and the ABC80 wins on arrays
(SIEVE).

## Where the time goes

From the profiles (`profil.py`, the routines named in the listings):

- *Floating point.* In B4, 27 % of the ABC80's time goes to the BCD
  division (FDLOOP) and about 30 % more to multiplication and shifting;
  in BASIC II, 17 % to FDIV and 13 % to FMUL. The ABC80 multiplication
  already skips zero bits but builds a table (2a ... 80a) every time.
- *The expression codes.* The ABC80 goes through EXEXP for every code:
  RST `$28`, INC SP twice, JP, PUSH HL and the table lookup, about 135 T.
  EXEXP is 17-25 % of the time in the integer tests. BASIC II's EXECA
  is an equally large share, but each code does more. There the codes
  are chained with RST `$28` (EXECNX), about 98 T per code.
- *Lines and statements.* The ABC80 tests TRACE for every line and calls
  TSTBRK for every statement (82 T); RUNLIN and RUNSTM are 20 % of
  NOEL1. BASIC II tests everything (BREAK, TRACE, CTRL-S) on one flag
  byte, 35 T.
- *Variables.* A value is two codes in the ABC80 (VAR0 with the
  address, then GETF or GETI), one in BASIC II (LDFVAR, LDIVAR).
- *Arrays.* BASIC II multiplies the index by the element size (2 for
  integers) in MUL16, always 16 iterations, about 1000 T: 17 % of
  SIEVE. The ABC80 is faster there.
- *FOR/NEXT.* NEXT is 20 % of NOEL1 in BASIC II and 9 % in the ABC80
  (plus FORTSI).

## The changes

Each change is a file of its own in `lapp/`. `lappa.py a|b name patch
...` builds `ut/name.rom`, and for the ABC80 also `ut/name.krets` (the
chip).

| Patch    | Interpreter | Bytes | What |
|----------|----------|------|-----|
| `a_brk`  | ABC80    |    8 | the BREAK flag is read directly in RUNSTM (27 T instead of 82); TSTBRK's clearing is done only when BREAK has come |
| `a_tab`  | ABC80    | 19+512 | EXEXP looks up the code in two tables on whole pages (`$5E00`, `$5F00`), 82 T instead of 91, with the same B, C and CY as before |
| `a_var`  | ABC80    |   35 | VAR0 checks whether the next code is GETF or GETI and fetches the value directly; the generated code is the same |
| `a_sup`  | ABC80    |  297 | as `a_var`, and VAR0 also recognises `V=V+X` and `V=V-X` (X a constant or a variable of the same type) and does the whole statement itself |
| `a_madd` | ABC80    |   37 | the addition in FMUL (MADD3 and the top byte) with HL instead of (IX+n) |
| `a_fswp` | ABC80    |   39 | FMUL swaps the factors so that the one with the most zero bytes at the low end becomes the multiplier |
| `a_ftab` | ABC80    |  262 | FMUL's table (a, 2a ... 80a) without loops, with HL and DE |
| `a_fdiv` | ABC80    |   72 | the subtractions in FDIV (FDLOOP) with HL and DE instead of (IX+n) |
| `b_mul`  | BASIC II |   28 | index times element size in ARRELM with a loop that stops when the size is used up (2-3 iterations) instead of MUL16 |
| `b_nxt`  | BASIC II |   95 | NEXT for an integer variable with HL through the block instead of (IX+n) |
| `b_sup`  | BASIC II |  260 | the statement loop recognises `V=V+X` and `V=V-X` (integer and floating point) and does the statement itself |

The sets are:

- `a_hel` = a_brk, a_tab, a_sup;
- `a_flt` = a_madd, a_fswp, a_ftab, a_fdiv;
- `a_alla` = both;
- `b_alla` = b_mul, b_nxt, b_sup.

`a_var` is contained in `a_sup` and is not in the sets on its own.

## The timings

`./lapptid.sh name`, 2026-10-11, the gain as a percentage of the time.
The screen is the same as without the patch in all 16 tests and all
sets.

Integers, statements and variables in the ABC80:

| Test   | a_brk | a_tab | a_var | a_sup | a_hel |
|--------|-------|-------|-------|-------|-------|
| B1     |   3.5 |   0.0 |   0.0 |   0.0 |   3.5 |
| B2     |   3.3 |   1.6 |   3.3 |  10.9 |  14.8 |
| B3     |   0.9 |   0.8 |   2.3 |   3.4 |   4.8 |
| B4     |   0.9 |   0.6 |   0.9 |   1.9 |   3.2 |
| B5     |   1.3 |   0.6 |   0.8 |   1.9 |   3.5 |
| B6     |   2.0 |   0.5 |   0.6 |   1.2 |   3.6 |
| B7     |   2.2 |   0.8 |   2.0 |   2.4 |   5.3 |
| B8     |   0.1 |   0.1 |   0.1 |   0.2 |   0.3 |
| FIZZ   |   0.5 |   0.3 |   0.6 |   0.6 |   1.3 |
| FIZZ1  |   1.3 |   0.7 |   1.9 |   1.9 |   3.8 |
| INTER  |   2.0 |   0.7 |   2.0 |   1.3 |   3.9 |
| MANDEL |   1.5 |   0.9 |   2.7 |   2.7 |   4.7 |
| NOEL   |   2.8 |   0.9 |   2.9 |  10.9 |  14.1 |
| NOEL1  |   7.3 |   2.3 |   9.8 |  23.0 |  31.2 |
| PRIMES |   0.8 |   0.4 |   1.3 |   1.3 |   2.5 |
| SIEVE  |   6.4 |   2.4 |   9.0 |  12.7 |  20.6 |

Floating point in the ABC80, and everything together:

| Test   | a_madd | a_fswp | a_ftab | a_fdiv | a_flt | without a_fswp | a_alla |
|--------|--------|--------|--------|--------|-------|-------------|--------|
| B1     |   0.0  |   0.0  |   0.0  |   0.0  |   0.0 |   0.0       |   3.5  |
| B2     |   0.0  |   0.0  |   0.0  |   0.0  |   0.0 |   0.0       |  14.8  |
| B3     |   1.8  |   3.3  |   2.6  |   7.5  |  14.0 |  11.9       |  18.8  |
| B4     |   0.8  |  −0.8  |   2.4  |  14.2  |  16.6 |  17.3       |  19.8  |
| B5     |   0.7  |  −0.7  |   2.3  |  13.7  |  16.0 |  16.7       |  19.5  |
| B6     |   0.5  |  −0.5  |   1.6  |   9.6  |  11.2 |  11.7       |  14.8  |
| B7     |   0.4  |  −0.4  |   1.2  |   7.1  |   8.3 |   8.7       |  13.6  |
| B8     |   7.1  |   0.4  |   7.7  |   0.0  |  14.5 |  14.8       |  14.8  |
| FIZZ   |   0.7  |  −0.7  |   1.9  |  15.1  |  17.1 |  17.7       |  18.4  |
| FIZZ1  |   0.0  |   0.0  |   0.0  |   0.0  |   0.0 |   0.0       |   3.8  |
| INTER  |   0.0  |   0.0  |   0.0  |  21.6  |  21.6 |  21.6       |  25.5  |
| MANDEL |   6.1  |   3.5  |   4.5  |   0.0  |  12.7 |  10.6       |  17.5  |
| NOEL   |   0.0  |   0.0  |   0.0  |   0.0  |   0.0 |   0.0       |  14.1  |
| NOEL1  |   0.0  |   0.0  |   0.0  |   0.0  |   0.0 |   0.0       |  31.2  |
| PRIMES |   0.7  |  −0.8  |   2.3  |  18.2  |  20.5 |  21.3       |  23.0  |
| SIEVE  |   0.0  |   0.0  |   0.0  |   0.0  |   0.0 |   0.0       |  20.6  |

`without a_fswp` is `ut/a_flt2`, with a_madd, a_ftab and a_fdiv. The
gains roughly add up, because the patches touch different routines.

BASIC II:

| Test   | b_mul | b_nxt | b_sup | b_alla |
|--------|-------|-------|-------|--------|
| B1     |   0.0 |  −0.6 |   0.0 |  −0.6  |
| B2     |   0.0 |   0.0 |   4.6 |   4.6  |
| B3     |   0.0 |   0.0 |   1.4 |   1.4  |
| B4     |   0.0 |   0.0 |   1.4 |   1.4  |
| B5     |   0.0 |   0.0 |   1.3 |   1.3  |
| B6     |   0.0 |  −0.2 |   0.7 |   0.5  |
| B7     |   6.4 |  −0.1 |   0.2 |   6.6  |
| B8     |   0.0 |   0.0 |   0.1 |   0.1  |
| FIZZ   |   0.0 |   0.0 |   0.0 |   0.0  |
| FIZZ1  |   0.0 |   0.1 |   0.0 |   0.1  |
| INTER  |   0.0 |  −0.1 |   0.0 |  −0.1  |
| MANDEL |   0.0 |   0.0 |  −0.0 |  −0.0  |
| NOEL   |   0.0 |  −0.3 |   5.8 |   5.6  |
| NOEL1  |   0.0 |   1.4 |  16.7 |  18.1  |
| PRIMES |   0.0 |  −0.1 |   0.1 |   0.0  |
| SIEVE  |  13.9 |   0.3 |   3.0 |  17.2  |

Both interpreters patched, in seconds (the ratio is ABC80 / BASIC II,
before and after):

| Test   | a_alla | b_alla | Ratio before | Ratio now |
|--------|--------|--------|------------|---------|
| B1     |  0.924 |  0.863 | 1.12       | 1.07    |
| B2     |  1.801 |  1.730 | 1.17       | 1.04    |
| B3     |  8.812 |  5.893 | 1.82       | 1.50    |
| B4     |  9.490 |  5.914 | 1.97       | 1.60    |
| B5     |  9.857 |  6.356 | 1.90       | 1.55    |
| B6     | 14.891 | 11.622 | 1.50       | 1.28    |
| B7     | 20.372 | 18.529 | 1.19       | 1.10    |
| B8     | 11.342 |  2.882 | 4.61       | 3.94    |
| FIZZ   |  2.915 |  2.229 | 1.60       | 1.31    |
| FIZZ1  |  1.349 |  1.471 | 0.95       | 0.92    |
| INTER  |  9.636 |  6.733 | 1.92       | 1.43    |
| MANDEL |139.572 | 92.613 | 1.83       | 1.51    |
| NOEL   | 20.981 | 17.722 | 1.30       | 1.18    |
| NOEL1  |  6.421 |  6.417 | 1.19       | 1.00    |
| PRIMES | 72.053 | 44.254 | 2.11       | 1.63    |
| SIEVE  | 20.888 | 25.386 | 0.86       | 0.82    |

### What is not obvious

- *`a_fswp` depends on the program.* The swap pays off when one factor
  is a small integer or has few digits. In B3 and MANDEL it gives about
  2 percentage points. When it does not, it costs the test 0.3-0.8 (B4,
  B5, PRIMES and others). Including it in `a_alla` is a choice for these
  particular tests.
- *`a_tab` gives little for its 512 bytes.* The first version, 54 T,
  did not set B and C as the ROM does and was therefore faster. It gave
  errors in every test with functions. With the same registers as the
  ROM, 9 T per code remain, 0.5-2.4 %. Counted in bytes it is the worst
  deal of the patches.
- *`b_nxt` gives almost nothing.* The patch takes only integer
  variables and leaves floating point to the ROM. It gains in NOEL1
  (1.4 %) and almost nothing in FIZZ1 and SIEVE. It loses a little in
  seven tests, most in B1 (0.6 %). B1 is just `FOR K=1 TO 1000` with a
  float. The loss there is about 15 T per NEXT: the jump to the patch
  and HL instead of IX up to the type.

  The first version computed the block and the variable's reference,
  tested the type and then left floats to the ROM at `$2816`, where
  everything was done again. It lost 130 T per NEXT, 5.1 % in B1 and
  2.4 % in NOEL. Two ways of avoiding that were tried:
  - the type first, with block + 6 computed in A before anything else:
    the loss becomes about 78 T (3.0 % in B1), but integers get 26 T
    more and NOEL1 gains only 0.3 %;
  - the variable's address and VFF36 before the type, and floats to the
    ROM's floating-point path at `$285E` with IX and HL as the ROM has
    them there. No work is redone, and the integer path is the same.
    That is the patch now.
- *`a_sup` is a pattern, not a new code.*
  - *The code.* `C%=C%+1%` compiles to
    `C1 n b lo hi | C1 n b lo hi D5 | C7 01 00 | F5 | B7`, five codes
    (four with `a_var`). `K=K+1` compiles to `C0 … | C0 … D4 | C3` and
    five bytes `| F4 | B6`.
  - *Recognition.* When VAR0 is about to push an address, that is, for
    the variable being assigned, it checks whether the rest is exactly
    the pattern: the same address with GETI or GETF, a constant or a
    variable of the same type with GET, plus or minus, and the
    assignment.
  - *The statement.* VAR0 then does the statement itself: integers with
    ADD or SBC like ADDI and SUBI, floats with the ROM's FADD and FSUB
    on the stack like FARITH. The variable is written only when they
    have returned, so that error 4 leaves it untouched. VAR0X ends like
    ASGNI and ASGNF, with HL, B and DE as they leave them. The codes are
    read in turn from the start of a code, so the pattern cannot be
    found in the middle of another code's bytes.
  - *The gain* is large where the statement is in the inner loop: NOEL1
    (`S%=S%+J%`) 23.0 % against 9.8 with `a_var`, NOEL and B2 about 11
    against 3, SIEVE 12.7 against 9.0.
  - *The cost* is 34 T for an assignment where the next code is not a
    variable, and 104 T when the next code is another variable of the
    same type, as `M=N/K` in INTER. There the gain becomes 1.3 % instead
    of 2.0.
- *`b_sup` is the same pattern in BASIC II.* It gives less than in the
  ABC80, because BASIC II already has one code per variable.
  - *The code.* `S%=S%+J%` compiles to `C1 r | C1 r' | 9C | B5 r`, four
    codes through EXECA. X can also be a small integer (`$DD-$FF`) or
    INTCON (`C7 lo hi`). `K=K+X` compiles to `C0 r | X | 9B | B4 r` (9E
    for minus), where X is a variable or a constant (`$D5-$DC` and 1-8
    bytes).
  - *Where it sits.* The statement loop calls EXECA for the statement's
    first code (`$1D1A`). The patch takes that call. The codes inside an
    expression and FOR's assignment (RST `$20`) do not go there and cost
    nothing.
  - *When the pattern does not match*, the patch goes straight to LDIVAR
    or LDFVAR, since the lookup in EXECA has already been done. The
    first version sat in LDIVAR's table entry and lost time in SIEVE and
    FIZZ1.
  - *Integers* are computed with ADD and SBC and wrap around without an
    error, as in IADD and ISUB. The patch ends like STIVAR.
  - *Floats* are pushed with the ROM's FLOAD and NUMPSH. FADD or FSUB is
    called with the same registers and PRGPTR as from FDISP, and
    overflow goes to FARITH's error 130. The statement ends in the ROM's
    STFVAR, so the variable is written only when the addition has
    succeeded. DOUBLE (8 bytes) takes the same path.
  - *The gain* is NOEL1 16.7 %, NOEL 5.8 % and B2 4.6 %.

## The changes do not just work for the benchmarks

That the tests give the same screen only says that the changes work for
what the 16 tests do. The changes are therefore tested in four more
ways.

- *The routines directly.*
  - `fprov` calls FMUL and FDIV in the unpatched and the patched ROM
    with the same numbers. It compares the result's five bytes, A, F,
    BC, DE, HL and error 4.
  - The numbers are edge cases (exponent 1, 2, `$7F-$82`, `$FE`, `$FF`;
    mantissas such as 999999, 999995, 100001 and 500005), numbers with
    one to three digits, a·a and a/a, in both states of the rounding
    flag.
  - That makes 300,000 cases per routine with `a_flt` and as many with
    `a_alla` and another seed, with no differences.
  - `mulprov` does the same for `b_mul` against MUL16: every HL with
    sizes 1-16, 32, 64 and 256 in both directions, and 200,000 random
    pairs. That makes 2.7 million cases with no differences in HL and
    CY. On overflow only CY is compared, since that is what ARRELM can
    learn.
- *The book's interpreter tests.* `bokprov.sh` runs the 569 tests in
  `../../dis/04/host/tests` with `a_alla` and `b_alla` instead of the
  ROMs. 545 give the same screen. The other 24 differ only in what
  depends on time:
  - 21 tests stop after a fixed number of instructions or count them,
    and the patched ROM gets further or needs fewer (FMUL, FDIV and
    others);
  - `random2` has RANDOMIZE, which takes the seed from the R register;
  - `bii/step` presses BREAK after 100 ms, and `b_alla` manages to
    print one more number;
  - `iec/standby` presses BREAK while the IEC ROM waits in WAITBO, and
    BREAK is lost in CHKBRK (see "What turned up along the way"). With
    30 different delays it is lost once in the unpatched ROM and five
    times with `a_brk`. It is the same code in the IEC ROM, and where
    the interrupt lands in the loop depends on how long everything
    before took.

  No test gives a different screen for any other reason.
- *The edge cases.* `kantprov.sh` runs 63 programs in `kant/prov.txt`
  in both interpreters, unpatched and patched. The cases cover the
  following:
  - *FOR/NEXT:* up and down, equal limits, step 0 and large steps, the
    limits ±32767, nested FOR, NEXT without a variable, NEXT without
    FOR, the variable or limit changed in the loop, FOR in GOSUB and
    floating-point FOR.
  - *Arrays:* integers, strings, two indices, index out of range,
    negative and floating-point indices.
  - *The rest of the language:* DEF FN, strings, comparisons, ON GOTO
    and BREAK in a loop.
  - *Numbers:* floats near overflow and underflow, a/a and 2^.5;
    integers at ±32767 and overflow in multiplication.
  - *The DIM bug* below.
  - *`V=V±X`:*
    - integers and floats, constants and variables;
    - statements that almost match (`A%=A%+1%+1%`, `A%=A%+1%*2%`,
      `A%=B%+1%`, `A%=A%+1`, `A%=A%+B%-A%`, `K=K+J-K`);
    - overflow and underflow in the statement;
    - V=V+X as a FOR limit and after THEN;
    - two variables with the same low address byte or reference byte;
    - for BASIC II also small integers against INTCON (34 and 35),
      constants of different lengths, and DOUBLE.

  All 126 runs (63 cases in two interpreters) give the same screen with
  `a_alla` and `b_alla`. Each BASIC II patch on its own also gives the
  same screen in every case. `brk_slinga` (`10 I%=I%+1`, `20 GOTO 10`,
  BREAK after a fixed time) can differ, because it stops on line 10 or
  line 20 depending on where the loop is when BREAK comes. With 200
  delays between 300 and 500 ms, the unpatched ROM stops on line 20
  twice and `a_alla` three times. The rest of the time it stops on line
  10, except when BREAK is lost (see below). The test thus shows where
  the time happens to land, and a faster interpreter lands somewhere
  else.

  The edge cases in BASIC II wait 1000 ms after each RETURN. abc80host
  feeds one key every 100 ms, and BASIC II compiles the line when RETURN
  comes and drops keys that arrive meanwhile. Without the wait, lines
  were truncated and RUN disappeared, so both ROMs gave the same error
  output and the test said nothing. That affected the first runs of
  `b_mul` and `b_nxt`, which have been redone.
- *Deliberate faults.* The patches have been built with faults inserted
  to see that the tests notice. `felprov.py` repeats this for `a_sup`
  and `b_sup`; the faults and the explanations are in the script. The
  floating-point patches are tested by `fprov`. A fault that no test
  notices is either harmless or a hole in the tests, and each such fault
  is explained from the listing below.
  - *`a_var`.* All five faults give errors in 3-25 edge cases: without
    FNPAR, GETF and GETI swapped, INC DE missing in both branches, and
    C not set.
  - *`a_brk`.* Neither of the two faults gives anything different, and
    the listing shows why:
    - BRKSTP is `RST $10` with code 0. RST10 and ERROR overwrite HL and
      DE and take the line from CURLIN, so EX DE,HL before the jump does
      not matter.
    - Error 0 always goes to READY, which calls TSTBRK after INLINE
      (`$00F0`), so BRKFLG is cleared there anyway.

    Both instructions are kept, as in TSTBRK, but are not needed.
  - *`b_nxt`.*
    - Without the JP PO correction or without the swap for a negative
      step, four edge cases fail.
    - In the floating-point path, three faults each show up in three
      edge cases (for_flyt, for_flyt2, for_bland): HL not swapped with
      DE, a jump past the addition (`$2865`), and IY instead of IX.
    - Without the write to VFF36 nothing is noticed. VFF36 is written at
      `$22D1` and `$2826` but read nowhere in the ROM, so the patch writes
      it only because the ROM does.
  - *`a_tab`.*
    - With B ≠ 0 there are six failures (functions, strings).
    - With CY = 1 no test noticed anything at first, because every
      subtraction in them was floating point (`A%-1` is computed in
      floating point). `hel_d` and `hel_e`, with integer against
      integer, were added and catch it.
    - C is set by the ROM but read by none of the 74 routines in
      EXPTAB1. None of them jumps on S, Z or P before setting them
      itself either. This is checked in the listing, not just with the
      tests.
  - *`b_mul`.* Without CY on overflow the fault is not noticed, because
    overflow cannot happen: the index has already been checked against
    the array size.
  - *`a_sup`.* 14 of 20 faults give something different in the edge
    cases:
    - only the low or only the high address byte tested;
    - without the test for ASGNI or ASGNF;
    - the subtraction computed as addition, and FSUB instead of FADD;
    - DE not advanced after the statement;
    - the integer constant's high byte 0, and the constant's exponent
      one byte off;
    - four bytes instead of five in the right operand;
    - the variable not written back.

    The six that are not noticed are explained by the listing:
    - Four remove the test for GETI or GETF after the first variable or
      after X. PRIMARY always writes `$D4` or `$D5` directly after a
      variable in an expression, so the test is a safeguard that never
      triggers.
    - Two leave a wrong HL or B after the statement. The assignment is
      only received by LET, which returns to RUNSTM, and by FOR, which
      overwrites HL and B with the next RST `$38`. Nobody reads them.

    Both kinds are kept, as in the ROM.
  - *`b_sup`.* 18 of 25 faults give something different:
    - only the low or only the high reference byte tested (integers);
    - without the test for B5 or B4 (`A%=A%+B%-A%` and `K=K+J-K` have
      the reference after the operator but no assignment there);
    - the subtraction as addition, and V and X in reverse order;
    - DE wrong after the statement or when the pattern does not match;
    - errors in small integers, INTCON and the constants' length to
      NUMPSH;
    - the high byte not written;
    - X read as V;
    - PRGPTR not set;
    - overflow without error 130.

    Seven are not noticed:
    - B, C and HL wrong after the integer statement, and VFF36 not
      written. FOR never goes through `$1D1A`, and the statement loop
      (L1D1D) overwrites B, C and HL before anyone reads them. VFF36 is
      read nowhere.
    - IX not set after a constant. Nothing after the statement reads IX
      before it is set.
    - A constant with the wrong length in the test. Then the pattern
      does not match, and the ROM does the statement as before. It only
      changes the time: B2 runs 1.5 % slower instead of 4.6 % faster.
    - Only the low reference byte tested in the floating-point path.
      That is not a fault. STFVAR reads the target from the code, so
      `A1=Q1+1` with Q1 = `$1108` and A1 = `$0108` comes out right
      (`sup_b2g`). The test only delimits the pattern.

## Would a rewrite have paid off?

The question concerns the ABC80. The rewrite already exists to look at:
BASIC II is the ABC800 interpreter moved to the ABC80. It has binary
floating point, DOUBLE and a different code, and it needs 30 KB of ROM
instead of 16.

*What the measurements say.* The patches split the difference into two
parts.

- *Interpretation.* Statements, variables, integers, loops and arrays.
  Here the patched ABC80 BASIC reaches BASIC II, also when BASIC II is
  patched as much:
  - NOEL1 6.42 against 6.42 s;
  - B2 1.80 against 1.73;
  - FIZZ1 1.35 against 1.47;
  - SIEVE 20.9 against 25.4.

  A new interpreter with a different code would not have given much
  more than the patches. What remains is 34-104 T when the pattern does
  not match and the 135 T per expression code in EXEXP. A new code could
  remove that, for example with addresses instead of codes or with
  `V=V+X` as a code of its own.
- *The numbers.* Here BASIC II stays far ahead:
  - B8 11.3 against 2.9 s;
  - B4, B5, PRIMES and MANDEL 1.5-1.6 times.

  The patches make the BCD routines 8-22 % faster, but digit-by-digit
  BCD division and multiplication are slower than binary arithmetic.
  Only a new number representation removes the difference.

*What a rewrite would have cost.*

- *The programs.* SAVE writes the compiled code, a BAC file. With a new
  code no saved BAC file can be read. BAS files, that is, programs as
  text, can be loaded and recompiled. With a new number format the
  numbers in the BAC files' constants change too.
- *The add-ons.* The add-on ROMs call the ROM at fixed addresses. The
  DOS ROMs (abcdos80, ufd80) call the jump vectors `$000B-$0044` and three
  internal routines (`$0727`, `$0901` and LNKSRC `$1074`). SuperBASIC and
  Schoolaid each call 44 addresses inside the ROM (counted in the
  listings in `../../dis/build`). Machine-code programs using CALL do
  the same. A new interpreter would have to keep every such entry
  point, with the same registers in and out, or the add-ons would have
  to be rewritten. The patches leave the entry points where they were.
- *Numbers and output.* BCD represents decimal fractions such as 0.1
  exactly, and binary floating point does not. An ABC80 with binary
  floating point prints different digits in some programs. Programs
  that compare numbers with = can take a different path, for example
  FOR with STEP 0.1 or `IF X=0.3`. That is a change of the language,
  not of the interpreter.
- *Space.* 16 KB of ROM is nearly full. The patches already need an
  extra chip at `$5000` for 730 bytes of code and 512 bytes of tables.
  BASIC II needs 30 KB. An interpreter with binary floating point and
  the same language in 16 KB would have required something else to be
  removed. In 1978 one more ROM chip cost money in every machine.
- *Testing it.* Every patch here is tested against the unpatched ROM.
  The answer must be exactly the same, so every edge case, interpreter
  test and random number has a reference answer. A new interpreter has
  no reference except the manual and the programs. The book's appendix
  "Fel och egenheter i BASIC II" (bugs and quirks in BASIC II) has 67
  entries for an interpreter that had nevertheless been tested in the
  ABC800.

*The trade-offs.* Had the ABC80 BASIC been given one more round, it
would have paid to do as here, but from the start and in the ROM:

- Keep the entry points, the BAC code and the BCD format.
- Remove the tests in the statement loop (`a_brk`, 3.5-7.3 % for 8
  bytes).
- Make the variable and the value one code (`a_var`, up to 9.8 % for
  35 bytes).
- Recognise `V=V+X` (`a_sup`, up to 23 % for 297 bytes, including `a_var`).
- Write the BCD division with HL and DE (`a_fdiv`, up to 22 % for 72
  bytes).

That is 377 bytes for most of the gain. The tables in `a_tab` (512
bytes for 0.5-2.4 %) and the swap in `a_fswp`, which depends on the
program, would have had to give way. The only thing a complete rewrite
would have bought beyond that is the floating point. BASIC II paid that
price: a different code, different numbers, a new DOS interface and
twice as much ROM. For a machine that already had its programs, add-ons
and users, that was a new machine, not another round.

## What turned up along the way

- *The DIM bug in the ABC80 ROM.* When CMPARG fails in CMPDMI (`$1BD5`,
  e.g. `DIM A` without an index or `DIM B$(1,2,3)`), it jumps to CMPERR
  with AF from `$1BD1` still on the stack. POP BC takes AF instead of BC,
  and RET jumps to VARNAM's result: the letter (`$41-$5D`) as the high
  byte, the digit and type as the low.
  - On an ABC80 with nothing at `$4100-$5DFF`, `$FF` is read there. That is
    RST `$38`, and the error becomes ERR 4 by chance. An add-on ROM with
    code at those addresses runs that code.
  - The first chip was at `$4000`, and the tables at `$4100` and `$4200` gave
    ERR 5 and ERR 14 in the book's test dim4.
  - The chip is now at `$5000` with the patches in the gaps `$x93-$xEF`,
    and `lappa.py` refuses to build if any code lands on an address the
    bug can jump to.
- *`$7800-$7FFF` is not free in BASIC II.* The area is `$FF` in the ROM
  file, and the patches were there at first. But in the machine
  `$7800-$7BFF` is RAM for the disk drives' DOS, with device entries and
  buffers, and `$7C00`- is the screen. abc80host does the same with `-F`.
  There the book's tests wrote their file buffers over the patch, and
  13 BASIC II tests failed. The patches are now in the zeros after the
  cassette driver (`$044A-$06A9`), and `lappa.py` refuses to put them
  anywhere else.
- *BREAK lost in the IEC ROM.* CHKBRK (`$7028`) reads BRKFLG, tests it and
  writes 0. If the keyboard interrupt comes between the read and the
  write, BREAK disappears. TSTBRK (`$033E`) in the ABC80 ROM does the
  same. The unpatched ROM also loses BREAK with certain instruction
  timings in the emulator; the patch only moves where it happens.
- *Integers that wrap around.* In both interpreters `FOR I%=... TO
  32767` never finishes: I% goes from 32767 to −32768. The ABC80
  multiplies integers without noticing overflow (182·182 becomes
  −32412). BASIC II's IADD and ISUB wrap around without an error.
- *Zero with a sign byte.* When FMUL gets 0 through underflow, the sign
  byte becomes the XOR of two bytes that happen to be in memory. The
  unpatched ROM therefore gives different sign bytes against itself.
  FCMP only looks at the exponent when it is 0, so a comparison does not
  see it.
- *BASIC II's references.* A variable's reference in the code (VARREF,
  `$0F63`) is not a sequence number. It depends on the name: in one of
  the edge cases A1 has the reference `$0108` and Q1 `$1108`. Two variables
  with the same digit therefore share a low byte, and that is how
  `sup_b2g` tests the reference comparison.

## Not tested

- *Floating point in BASIC II.* FDIV and FMUL are 17 and 13 % of B4.
  They are already binary and written with registers, and the emphasis
  here is the ABC80.
- *A new BCD algorithm in the ABC80.* The patches rewrite the old
  routines with other registers and tables. A different division method
  has not been tried.
- *A real ABC80.* The timings are the emulator's. Only BASIC II has been
  compared with a real machine (ABC 802).
