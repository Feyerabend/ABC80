
## zx81bas.py — ZX81 BASIC Tokenizer / Detokenizer

A tool for converting ZX81 `.p` files to readable BASIC text and back,
with verified round-trip correctness across a library of real programs.



### Usage

```
python3 zx81bas.py list  <file.p>         detokenize; print BASIC listing to stdout
python3 zx81bas.py build <file.bas>       tokenize text BASIC; write <file.p>
python3 zx81bas.py test  [dir|file.p]     round-trip test(s)
python3 zx81bas.py info  <file.p>         dump system variable info
```

Options for `list`:
```
  --raw    show raw [XX] hex for unrecognised bytes
  --ascii  use ASCII fallbacks for block-graphic characters
```

#### Examples

```
# Show a BASIC listing
python3 zx81bas.py list games/alien.p

# Build a .p file from a text BASIC source
python3 zx81bas.py build my_program.bas

# Run round-trip tests on all .p files in a directory
python3 zx81bas.py test old/ABCD/

# Dump system variable info for a .p file
python3 zx81bas.py info games/hello.p
```



### Text BASIC format (for `build`)

Each line starts with a line number followed by the BASIC statement.
Keywords are case-insensitive. Lines starting with `#` are ignored.

```
10 CLS
20 PRINT "HELLO WORLD"
30 FOR I=1 TO 10
40 PRINT I;" ";
50 NEXT I
60 GOTO 20
```

Rules:
- After `REM`, the rest of the line is stored as literal characters (no keyword scanning).
- String literals `"..."` are stored character by character; no keyword scanning inside.
- Numbers get both their digit characters AND an embedded 5-byte ZX81 float.
- Inverse-video characters in strings/REM are written as `[INV:X]` where X is the
  base character. Raw bytes use `[XX]` hex notation (e.g. `[7F]`).



### Listing format (output of `list`)

The output approximates what the ZX81 displays when you type `LIST`:

- Each keyword token is preceded by a space (as the ROM does when listing).
- Block-graphic characters are shown as Unicode block elements
  (`▘ ▝ ▀ ▖ ▌ ▞ ▛ ▗ ▚ ▐`) or as `#` with `--ascii`.
- Inverse-video bytes (0x80–0xBF) appear as `[INV:X]`.
- Token bytes found inside string literals appear as `[XX]` hex — some ZX81
  programs store machine-code or data tables as string bytes; this notation
  preserves them for round-trip correctness.
- Unknown/undefined bytes appear as `[XX]` hex.



### Generated `.p` file structure

```
Offset 0x0000  116 bytes  System variables (with correct pointers set)
Offset 0x0074  n bytes    BASIC program (tokenized lines)
Offset 0x0074+n  1 byte   VARS end marker (0x80)
Offset +1      793 bytes  Display file (1 leading HALT + 24 rows × 33 bytes)
```

System variables set in generated files:

| Variable | Address | Value                      |
|----------|---------|----------------------------|
| VERSN    | 0x4009  | 0                          |
| E_PPC    | 0x400A  | first line number          |
| D_FILE   | 0x400C  | address of display file    |
| DF_CC    | 0x400E  | D_FILE + 1                 |
| VARS     | 0x4010  | address of VARS marker     |
| E_LINE   | 0x4014  | address after display file |
| NXTLIN   | 0x4029  | 0x407D (start of BASIC)    |
| CDFLAG   | 0x403B  | 0x40 (SLOW mode)           |
| MARGIN   | 0x4028  | 55 (PAL)                   |



### Technical notes

#### ZX81 character set

| Code range | Meaning                                  |
|------------|------------------------------------------|
| 0x00       | Space                                    |
| 0x01–0x0A  | Block graphics (quarter-blocks)          |
| 0x0B       | `"` (double-quote)                       |
| 0x0C–0x0F  | `£`, `$`, `:`, `?`                       |
| 0x10–0x1B  | `( ) > < = + - * / ; , .`                |
| 0x1C–0x25  | Digits `0`–`9`                           |
| 0x26–0x3F  | Letters `A`–`Z`                          |
| 0x40       | `RND` (special function token)           |
| 0x41       | `INKEY$` (special function token)        |
| 0x42       | `PI` (special function token)            |
| 0x76       | NEWLINE / HALT (line terminator)         |
| 0x7E       | Number marker (followed by 5-byte float) |
| 0x80–0xBF  | Inverse video of codes 0x00–0x3F         |
| 0xC0–0xFF  | BASIC keyword tokens                     |

#### Keyword token table (0xC0–0xFF)
```
0xC0 ""     0xC1 AT     0xC2 TAB     0xC3 ?       0xC4 CODE   0xC5 VAL
0xC6 LEN    0xC7 SIN    0xC8 COS     0xC9 TAN     0xCA ASN    0xCB ACS
0xCC ATN    0xCD LN     0xCE EXP     0xCF INT     0xD0 SQR    0xD1 SGN
0xD2 ABS    0xD3 PEEK   0xD4 USR     0xD5 STR$    0xD6 CHR$   0xD7 NOT
0xD8 **     0xD9 OR     0xDA AND     0xDB <=      0xDC >=     0xDD <>
0xDE THEN   0xDF TO     0xE0 STEP    0xE1 LPRINT  0xE2 LLIST  0xE3 STOP
0xE4 SLOW   0xE5 FAST   0xE6 NEW     0xE7 SCROLL  0xE8 CONT   0xE9 DIM
0xEA REM    0xEB FOR    0xEC GOTO    0xED GOSUB   0xEE INPUT  0xEF LOAD
0xF0 LIST   0xF1 LET    0xF2 PAUSE   0xF3 NEXT    0xF4 POKE   0xF5 PRINT
0xF6 PLOT   0xF7 RUN    0xF8 SAVE    0xF9 RAND    0xFA IF     0xFB CLS
0xFC UNPLOT 0xFD CLEAR  0xFE RETURN  0xFF COPY
```

#### BASIC line format

Each line in the program area:

```
2 bytes   Line number (big-endian)
2 bytes   Body length including terminator (little-endian)
n bytes   Body (tokens and character codes)
1 byte    0x76 NEWLINE terminator
```

#### BASIC program bounds in saved `.p` files

A finding which is not verified completely is that the memory
layout in a saved `.p` file is *not* the standard order documented
in the ZX81 manual. Saved files have:

```
[NXTLIN … D_FILE)      BASIC program
[D_FILE … D_FILE+793)  Display file (24 rows)
[VARS … )              Variable table
```

The end of BASIC is `min(VARS, D_FILE)` (not VARS alone).
Files where `NXTLIN == D_FILE` contain no BASIC program
(machine code only).

#### ZX81 5-byte floating-point format

```
value = 0:
    bytes = 00 00 00 00 00

value ≠ 0:
    sign  = 1 if value < 0, else 0
    e     = floor(log2(|value|)) + 1           <-- unbiased exponent
    byte0 = e + 128                            <-- biased exponent
    frac  = |value| / 2^(e-1) − 1.0            <-- fractional mantissa ∈ [0, 1)
    m31   = round(frac × 2^31)                 <-- 31-bit stored mantissa
    byte1 = (sign << 7) | ((m31 >> 24) & 0x7F)
    byte2 = (m31 >> 16) & 0xFF
    byte3 = (m31 >>  8) & 0xFF
    byte4 =  m31        & 0xFF
```

In the token stream, every numeric literal is stored as its visible digit
characters followed by `0x7E` and the 5 float bytes. The detokenizer skips
the `0x7E` + 5 bytes; the visible digits are used for display.

#### Tokenizer keyword matching

Keywords are matched using *longest-match* scanning, tried in decreasing
length order. A space in the listing always signals a keyword boundary — the
ZX81 ROM prepends one space when listing each keyword token, so no stored
spaces separate keywords from the identifiers that follow them.

The tokenizer only attempts keyword matching immediately after a space or at
the very start of a line body. This prevents false matches inside sequences of
adjacent single-letter variables (e.g. `SCORE` is `S`, `C`, `O`, `R`, `E`
— not `S`, `C`, `OR`-token, `E`).

Space handling: a run of N spaces before a keyword stores N−1 explicit `0x00`
bytes (extra programmer-typed spaces) and discards the last one (the keyword's
listing prefix). A run of spaces before a non-keyword stores all N as `0x00`.

#### REM and string handling

- *REM*: after the `REM` token, the remainder of the line body is stored as
  literal ZX81 character codes. No keyword scanning.
- *Strings*: characters between `"` markers (byte `0x0B`) are stored as ZX81
  character codes. Token bytes appearing inside strings (used by some programs
  as data) are shown as `[XX]` in the listing and reconstructed by the
  tokenizer via the same escape notation.



### Round-trip testing

```
python3 zx81bas.py test games/
```

For each `.p` file the test:
1. Detokenizes --> `listing_A`
2. Retokenizes `listing_A` --> `file_B.p`
3. Detokenizes `file_B.p` --> `listing_B`
4. Compares `listing_A` with `listing_B` — PASS if identical

Machine-code-only files (no BASIC program) are skipped automatically.
