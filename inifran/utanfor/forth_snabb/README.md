# A faster Forth (outside the book)

**Not part of the book.** An experiment with the Forth interpreter from
example 7 in volume 2: how much of its slowness comes from the stack
checking, and what does it cost in safety to make the checking cheaper?
Everything else is as in the book; the comparison concerns the
interpreter as a chip at `$4000` (`e7ip`, the same as
`emulator/exempel/krets/forth.bin`).

This is an English version of `LASMIG.md` ("läs mig", read me). File
names are Swedish: *gör* means make, *mätning* measurement, *tid* time.

| File              | What                                                       |
|-------------------|------------------------------------------------------------|
| `gor.sh`          | builds `forth_snabb.bin` from a copy of the book's sources |
| `forth_snabb.bin` | the interpreter as a chip at `$4000` with the new `BRANCH`   |
| `forth_snabb.lst` | the listing from z80asm                                    |
| `matning.sh`      | measures BASIC, the book's interpreter and the variant     |
| `TID.BAS`         | the BASIC part of the measurement                          |

## The change

Only `BRANCH` in the kernel (`e7k.inc`, in the public repository
`inifran/forth/e7k.inc`) is replaced, in a copy that `gor.sh` makes.
Every loop goes back through `BRANCH` (`(LOOP)`, `AGAIN`, `REPEAT`, and
`0BRANCH` when it jumps). In the book it checks three things on every
iteration:

1. the data stack has not been emptied past S0,
2. the data stack has not come down to BASIC's variables (HEAP + 64),
3. the return stack has not come down to the canary (as in `DOCOL`).

That costs 218 T-states, more than half of an empty `DO LOOP`
iteration (405 T). The variant checks only the second, the one
that protects the BASIC program, and only on the high byte:

```asm
    BRANCH: LD   HL,-64
            ADD  HL,SP
            LD   A,(HEAP+1)
            CP   H
            JP   NC,STKERR      ; SP - 64 on HEAP's page or lower
            EX   DE,HL          ; as before: jump
            ...
```

So it triggers up to 256 bytes too early, but it costs 48 T and needs
neither DE nor RAM. The first and third cases are caught by `?STACK`
after each line, as in the book; `DOCOL` still checks the return stack
on every call.

## The measurement

`./matning.sh`, 2026-10-10. Time is measured inside the emulated
machine with the clock at 65008 (50 Hz, 20 ms resolution), with the
correction for the low byte that volume 1 describes; without it one
measurement came out 256 ticks (5.12 s) wrong. These are the emulator's
T-states; it has not been checked against a real ABC80. 10,000
iterations in every test except the sieve (the primes up to 1000, 168
in all three).

| Test                                   | BASIC  | Forth, book  | Forth, variant   |
|----------------------------------------|--------|--------------|------------------|
| empty loop, `FOR I%` / `DO LOOP`       | 2.20 s | 1.36 s (1.6×) | 0.78 s (2.8×)   |
| empty loop with floats, `FOR I`        | 9.26 s | (6.8×)       | (11.9×)          |
| `A%=I%+I%-5%` / `I I + 5 - A !`        | 8.94 s | 3.50 s (2.6×) | 2.92 s (3.1×)   |
| `GOSUB` / call of an empty colon word  | 5.60 s | 2.48 s (2.3×) | 1.92 s (2.9×)   |
| the sieve to 1000                      | 2.82 s | 1.72 s (1.6×) | 1.48 s (1.9×)   |
| `BEGIN 1 - DUP 0= UNTIL`               | –      | 2.08 s       | 1.50 s           |

In parentheses: how many times faster than BASIC. The empty loop runs
almost twice as fast; in tests with more work in each iteration the
gain is smaller, since there it is `NEXT`, `DOCOL` and the words
themselves that cost. The Forth sieve is split into short words (the
line buffer holds 79 characters), and the interpreter has no `*` or
`/`, so the tests use only `+` and `-`.

## What it costs

Tested with one loop for each fault, followed by `1 2 + .` and `BYE`:

| Loop                                  | Book      | Variant                     |
|---------------------------------------|-----------|-----------------------------|
| `BEGIN 1 AGAIN` (fills)               | `STACK ?` | `STACK ?`                   |
| `BEGIN DROP AGAIN` (empties)          | `STACK ?` | `STACK ?`                   |
| `BEGIN DROP DROP 1 AGAIN`             | `STACK ?` | `STACK ?`, BASIC survives   |
| `10 0 DO R> DROP LOOP` (return stack) | `STACK ?` | hangs                       |

In the variant, the emptied data stack is caught only once SP has
wrapped around memory and come down below HEAP's page; that BASIC
survived the test is luck rather than protection. A loop that empties
the return stack is not caught at all: `R>` then takes BASIC's stack as
return addresses.

Checking the stacks only in the outer interpreter is what fig-FORTH
did: `INTERPRET` calls `?STACK` after each word it interprets, but not
inside colon words. The book's interpreter is more careful than that,
because an error could otherwise take the BASIC program with it.

## Building and measuring

```
    ./gor.sh          (forth_snabb.bin from a copy of the Forth sources, which are not changed)
    ./matning.sh
```

In the emulator (from its folder): `./abc80 -l <path>/forth_snabb.bin@4000`,
then `POKE 65052,0,208`, `NEW` and `Z=CALL(16384,3)`, as with the book's
chip.
