# COMAL 0.5: the ABC80 BASIC with COMAL's structures (outside the book)

*Not part of the book.* A counterfactual experiment. Suppose the ABC80
BASIC (1978) had been given COMAL's control structures instead of being
replaced by BASIC II: what would it have cost in bytes and in time? The
experiment builds the structures in a patched copy of the old ROM
(abc80old) and an add-on ROM at $4000. It measures them in the emulator
against GOTO and GOSUB, and against BASIC II's WHILE/WEND. The book's
files and the ROMs in `emulator/roms/` are not changed. The background,
COMAL and the Swedish school computer Compis, is in `BAKGRUND.md` (in
Swedish).

This is an English version of `LASMIG.md` ("läs mig", read me). File
and script names are Swedish: *bygg* means build, *kör* run, *prov*
test, *mätt* measured, *ut* output, *krets* add-on ROM (circuit),
*bakgrund* background, *fel* error.

## The main points

- *The structures fit in 2215 bytes plus 23 bytes in the ROM.* They are
  WHILE, REPEAT, block IF with ELIF and ELSE, PROC/EXEC with parameters
  and LOCAL, CASE/WHEN/OTHERWISE, and LIST with indentation. BASIC II
  needed 30 KB, but it also gave long variable names, binary floating
  point and much else.
- *Old programs run as before.* The 16 tests from `../tolkjamforelse`
  give the same screen, the same errors and the same numbers. Each RUN
  takes about 450 T longer, because the blocks are linked before the
  program runs. The run itself is as fast as before.
- *The structures cost little where they replace GOTO.* WHILE is 5 %
  slower than the same loop with IF/GOTO, REPEAT 3 %, block IF 2.5 %,
  and CASE 4.4 % against a chain of IFs. BASIC II's WHILE/WEND costs 4 %
  against its own GOTO loop.
- *The procedures cost more.* EXEC with one parameter takes 35 % longer
  than GOSUB with an assignment, and LOCAL adds another quarter. Both
  swap the values of the variables on the stack, on the way in and on
  the way out.
- *CASE evaluates the selector once.* It is then kept on the stack, and
  each WHEN value is evaluated directly from the program and compared
  with it: two bytes for integers, the ROM's FCMP for floating point,
  the length and the characters for strings. The first attempt built
  the code "selector value =" on the stack for each value, and was 18 %
  slower than the IF chain.
- *Errors in the structure show before the run.* The blocks are paired
  at RUN, like FOR and NEXT, and a block that is never closed gives
  error 70 before the first line runs.

## Assumptions

- *Not COMAL.* It is the ABC80 BASIC with COMAL's structures. Line
  numbers, GOTO and GOSUB, all the old statements, the short variable
  names and the BCD numbers remain. Long variable names, multi-line FUNC
  and REF parameters are not included. Strings cannot be parameters.
- *The emulator, not a real machine.* The time is the emulator's T
  states at 2.9952 MHz, from CR after `RUN` to `READY` ($00CC in the
  ABC80, $0110 in BASIC II), measured with `../tolkjamforelse/prof`. The
  ABC80 has 16 KB RAM and BASIC II 32 KB.
- *The location.* The add-on ROM is at $4000-$48A6, where Superbasic
  was ($4000-$57FF). The ROM only gets the jumps there: KWSRCH searches
  the add-on's keyword list, PRELNK links the blocks, and LIST indents.
  It is one more ROM in the machine, as in tolkjamforelse.
- *Only what was possible in 1978.* It is plain Z80 code using the
  ROM's own routines for expressions, numbers and errors. No new
  algorithm is used.

## The files

| File          | What                                                        |
|---------------|-------------------------------------------------------------|
| `comal.asm`   | the patches to the ROM and the add-on ROM, with the form of the statements at the top |
| `bygg.py`     | assembles and writes `ut/comal.rom` and `ut/comal.krets`    |
| `kor.c`       | host on the emulator core: types in lines from a file, times RUN |
| `prov.sh`     | builds, runs `prov/*.txt` against `prov/*.ut`, and the tolkjamforelse tests against abc80old |
| `matt.sh`     | the measurements: `matt/*.BAS` in all three interpreters, `*.CML` in the COMAL ROM, `*.B2` in BASIC II |
| `BAKGRUND.md` | the history of COMAL, Compis and the ABC 80, with sources (Swedish) |

The programs are built with the emulator core (`../../emulator/karna`)
and the patches with `../../dis/build/z80asm`. `ut/comal.rom` and
`ut/comal.krets` (load at $4000) are the result.

## The statements

| Statement                    | Error                                     |
|------------------------------|-------------------------------------------|
| `WHILE v` ... `ENDWHILE`     | 70: the block is never closed             |
| `REPEAT` ... `UNTIL v`       | 71: a closer without an opener, or of the wrong kind |
| `IF v THEN` ... `ELIF v THEN` ... `ELSE` ... `ENDIF` | |
| `PROC name(a,b%)` ... `ENDPROC`, `EXEC name(x,y%)` | 72: PROC missing, wrong number or type, LOCAL in the wrong place |
| `LOCAL a,b%`                 |                                           |
| `CASE v OF` ... `WHEN a,b` ... `OTHERWISE` ... `ENDCASE` | 73: no WHEN matches; 17: string against number |

A block IF is an IF with nothing after THEN; with a statement after
THEN it is an ordinary IF, as before. LOCAL must come first on the line
after PROC. The parameters and LOCAL use shallow binding: the old values
are pushed on the stack at EXEC and restored at ENDPROC, so the
variables keep their addresses and recursion works. The type must
already match in EXEC (5 to an N% parameter is error 72; write 5%).

## How it is built

A statement that needs an address gets one or two slots, `$B9 lo hi`,
in the code. The compiler writes empty slots. At RUN, PRELNK goes
through the program with a stack of open blocks (the top at EOFA+1) and
fills in where the next part is, as it does for FOR and NEXT. During the
run the statements follow the slots and need no stack. So GOTO can jump
out of WHILE, REPEAT, IF and CASE without leaving anything behind. PROC
puts the return on the stack, like GOSUB.

The add-on ROM's 2215 bytes:

| Part                                     | Bytes |
|------------------------------------------|------:|
| the keyword list and tables (with the ROM's table 2 copied) | 369 |
| compiling                                | 313   |
| linking at RUN                           | 574   |
| running (of which CASE and WHEN 317)     | 725   |
| LIST with indentation                    | 234   |

## The times

Seconds from RUN to READY. The programs are in `matt/`. The S programs
count to 10000 with integers, the A programs add 1..2000 through a call,
and the V programs choose between four cases 2000 times.

| Test                       | ABC80 | COMAL ROM | BASIC II |
|----------------------------|------:|----------:|---------:|
| SFOR (FOR/NEXT)            |  3.01 |  3.01 |  2.90 |
| SGOTO (IF ... THEN 20)     | 11.52 | 11.52 |  9.35 |
| SWHILE                     |       | 12.10 |  9.76 (WEND) |
| SREPEAT                    |       | 11.91 |       |
| AGOSUB (X=I, GOSUB)        |  6.94 |  6.94 |  5.51 |
| AEXEC (EXEC ADD(I))        |       |  9.36 |       |
| ALOCAL (with LOCAL T)      |       | 11.76 |       |
| VIF (IF ... : GOTO)        | 27.30 | 27.30 | 14.75 |
| VBLOCK (IF/ELIF/ELSE)      |       | 27.98 |       |
| VCASE (CASE/WHEN)          |       | 28.50 |       |

VIF runs almost twice as fast in BASIC II because `I-INT(I/4)*4` is
computed in binary floating point, not because IF is faster (see
tolkjamforelse).

### What is not obvious

- *ENDWHILE evaluates the WHILE condition itself.* The first attempt
  jumped back to WHILE, and then WHILE was 14 % slower than GOTO
  (13.1 s). Now ENDWHILE goes to the statement after WHILE when the
  condition is true, and on without a jump when it is false: 12.1 s.
- *The slot first.* In CASE, WHEN and ELIF the slot comes before the
  expression. A part that follows the chain to the next part then does
  not have to skip the conditions and values. With the slot after,
  VCASE took 37.0 s and VBLOCK 29.6 s.
- *CASE without copies.* The first CASE built the code "selector value
  =" on the stack for each value. That needed the lengths of the
  expressions, and ENDTOK went through the expression code to find them
  every time (32.1 s). The lengths could have been written in at link
  time, but that is not needed when the selector is evaluated once and
  each value is evaluated where it stands: RST $38 leaves DE after the
  value (28.5 s). An integer and a floating-point number are compared
  by converting the integer to floating point (INTTOF) in five bytes in
  the CASE frame. A computed string (CHR$, +) stays on the stack until
  it has been compared. prov/case2.
- *What CASE still costs.* A floating-point value in WHEN ends with $B9,
  and ENDF then moves the return address below the number; the
  comparison in IF ends with an integer. That is most of the 4.4 %. CASE
  becomes faster than the IF chain when the selector is a longer
  expression, evaluated once instead of once per IF. A jump table for
  integer values would be fastest, but requires all values to be
  constants.
- *TRACE follows the jumps.* A jump to another line goes the same way as
  RUNLIN, so TRACE prints the line number as for GOTO.

## What turned up on the way

- *The ROM's table 2 ends at $95.* STKW2, STTAB2 and STEXE2 have 22
  statements, `$86 $80` to `$86 $95`, but the code reaches $FF if the
  tables are moved. The add-on ROM has copies of them with the new
  statements after, and the ROM points there.
- *The DIM bug.* On an error CMPDMI jumps into $41xx-$5Dxx with AF left
  on the stack (see `../tolkjamforelse/lappa.py`). Here it is fixed with
  DIMFIX, so the add-on ROM needs no forbidden bytes.
- *LIST pauses after 22 lines.* The tests therefore only list short
  programs.

## Not tried

- A real ABC80.
- BAC files with the new statements. The slots are rewritten at every
  RUN, so they should work.
- LIST to a file (the indentation is computed, but only the screen has
  been tested).
