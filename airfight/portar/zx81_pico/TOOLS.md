
## Developer Tools

This project has tools for three tasks: building `.p` files from assembly,
working with ZX81 BASIC programs, and loading `.p` files onto the Pico.

All commands are run from the **project root** unless stated otherwise.



### Quick reference

| Task                     | Command                                                                                                     |
|--------------------------|-------------------------------------------------------------------------------------------------------------|
| Build airfight and load  | `bash tools/build_airfight.sh /dev/tty.usbmodem*`                                                           |
| Build any .asm → .p      | `python3 tools/make_p.py games/foo.asm`                                                                     |
| Send a .p to the Pico    | `python3 tools/send_p_termios.py /dev/tty.usbmodem* game.p`                                                 |
| Inspect a .p file        | `python3 token/zx81bas.py list game.p`                                                                      |
| Inspect system variables | `python3 token/zx81bas.py info game.p`                                                                      |
| Edit BASIC as text       | `python3 token/zx81bas.py list game.p > game.bas` then edit, then `python3 token/zx81bas.py build game.bas` |
| Assemble only            | `asm/z80asm -o out.bin source.asm`                                                                          |



### tools/make_p.py — Assembler + .p file builder

Assembles a Z80 `.asm` file and wraps the binary in a valid ZX81 `.p` file.
Reads `ORG` and `DFILE EQU` addresses from the source automatically.

```shell
python3 tools/make_p.py <source.asm> [out.p] [options]
```

Options:
```
  --org   0xNNNN    override ORG address from .asm
  --dfile 0xNNNN    override DFILE address from .asm
  --header          also write src/<name>_bin.h (C array of binary for Pico embed)
```

The generated `.p` file layout:
```
0x4009–0x407C   System variables
0x407D          BASIC line 10: RAND USR <org>
VARS            0x80 end-of-vars marker
<zeros>         padding from VARS to ORG
ORG–...         Machine code (from assembled binary)
DFILE–...       Display file: 0x76 + 24 × (32 spaces + 0x76)  [793 bytes]
```

The `.asm` source must define:

```asm
    ORG     0x8000          ; code load address
DFILE   EQU 0x8800          ; display file base (must be after end of code)
```

Examples:

```bash
# Build airfight.p (reads ORG + DFILE from asm, writes games/airfight.p)
python3 tools/make_p.py games/airfight.asm

# Build and also produce src/airfight_bin.h for the Pico C code
python3 tools/make_p.py games/airfight.asm --header

# Build with explicit addresses (for .asm that uses a different convention)
python3 tools/make_p.py foo.asm out.p --org 0x8000 --dfile 0x8800
```



### tools/build_airfight.sh — One-shot airfight build + optional load

Assembles `games/airfight.asm`, writes `games/airfight.p` and
`src/airfight_bin.h`, then optionally loads onto the Pico.

```bash
bash tools/build_airfight.sh                        # build only
bash tools/build_airfight.sh /dev/tty.usbmodem*    # build + load
```



### tools/send_p_termios.py — Load a .p file onto the Pico

Sends a `.p` file to the Pico emulator over USB-CDC serial.
No external Python packages needed (uses built-in `termios`).

```bash
python3 tools/send_p_termios.py <port> <file.p>
```

Examples:

```bash
# macOS (port name varies)
python3 tools/send_p_termios.py /dev/tty.usbmodem* games/airfight.p

# Linux
python3 tools/send_p_termios.py /dev/ttyACM0 games/airfight.p
```

Protocol: the script sends `Ctrl+L` (0x0C) to trigger load mode, then a
2-byte little-endian file length, then the raw `.p` bytes.
The Pico responds with `LOAD: waiting...` and `OK (...)` on success.

`tools/send_p.py` is an older version that requires `pip install pyserial`.
Prefer `send_p_termios.py`.



### tools/make_hello.py — Minimal smoke-test .p file

Generates a machine-code `.p` file that writes `HELLO` to the screen and
loops. Used to verify that `.p` loading + display rendering + USR call all work.

```bash
python3 tools/make_hello.py [output.p]
# default output: games/hello.p
```



### tools/make_rom_header.py — ROM binary → C header

Converts a binary ROM file to a C `const uint8_t[]` header.

```bash
python3 tools/make_rom_header.py roms/zx81.rom src/zx81_rom.h
```



### token/zx81bas.py — ZX81 BASIC tokenizer / detokenizer

Converts between tokenized `.p` files and human-readable BASIC text.
Round-trip verified against 103 real ZX81 programs.
Full documentation: `token/README.md`.

```shell
python3 token/zx81bas.py list  <file.p>           detokenize; print listing
python3 token/zx81bas.py list  <file.p> --raw     show unknown bytes as [XX] hex
python3 token/zx81bas.py list  <file.p> --ascii   ASCII fallbacks for block graphics
python3 token/zx81bas.py build <file.bas>         tokenize text → .p file
python3 token/zx81bas.py test  [dir|file.p]       round-trip test
python3 token/zx81bas.py info  <file.p>           dump system variable values
```

Examples:

```bash
# Show the BASIC listing of any .p file (including machine-code games)
python3 token/zx81bas.py list games/airfight.p
python3 token/zx81bas.py list token/monster.p

# Edit a BASIC program: detokenize → edit → retokenize
python3 token/zx81bas.py list token/monster.p > /tmp/monster.bas
# ... edit /tmp/monster.bas ...
python3 token/zx81bas.py build /tmp/monster.bas   # writes /tmp/monster.p

# Inspect a file's system variable pointers
python3 token/zx81bas.py info games/airfight.p

# Test round-trip fidelity for all .p files in a directory
python3 token/zx81bas.py test old/ABCD/
```

Text BASIC format for `build`:

```basic
10 CLS
20 PRINT "HELLO WORLD"
30 FOR I=1 TO 10
40 PRINT I;" ";
50 NEXT I
60 GOTO 20
```

- Lines starting with `#` are comments (ignored)
- Keywords are case-insensitive
- After `REM`, the rest of the line is literal (no keyword scanning)
- Numbers get embedded 5-byte ZX81 floats automatically
- Inverse-video characters: `[INV:X]`; raw bytes: `[XX]` hex



### old/z80asm — Z80 assembler binary

Pre-compiled two-pass Z80 assembler (arm64, macOS). Source is in `asm/z80asm.c`.

```
asm/z80asm [-o out.bin] [-l] [-v] source.asm
```

`make_p.py` calls this automatically. Use directly only if you need the raw binary.

```bash
old/z80asm -o /tmp/game.bin games/airfight.asm
```



### Typical workflows

#### New game: write, assemble, test on emulator

```bash
# 1. Write your game in games/mygame.asm with ORG 0x8000 and DFILE EQU 0x????
# 2. Build the .p file
python3 tools/make_p.py games/mygame.asm

# 3. Inspect the BASIC trampoline (should say: 10 RAND USR 32768)
python3 token/zx81bas.py list games/mygame.p

# 4. Load onto Pico
python3 tools/send_p_termios.py /dev/tty.usbmodem* games/mygame.p
```

#### Inspect or modify a real ZX81 BASIC program

```bash
# View listing
python3 token/zx81bas.py list old/ABCD/MYGAME.P

# Extract to text, edit, rebuild
python3 token/zx81bas.py list old/ABCD/MYGAME.P > /tmp/mygame.bas
python3 token/zx81bas.py build /tmp/mygame.bas
python3 tools/send_p_termios.py /dev/tty.usbmodem* /tmp/mygame.p
```

#### Full airfight rebuild from source

```bash
bash tools/build_airfight.sh /dev/tty.usbmodem*
```
