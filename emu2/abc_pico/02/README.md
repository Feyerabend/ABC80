
## ABC80 for Ants

*A (somewhat) faithful ABC80 emulator running on the Raspberry Pi Pico 2W + Pimoroni Display Pack 2.0*

This project brings the classic Swedish 8-bit computer ABC80 back to life on modern tiny hardware.
It runs real ABC80 BASIC and machine code with high accuracy, including the authentic character set,
screen layout, and 50 Hz interrupt behaviour.


### Features

- Accurate Z80 CPU emulation
- Original ABC80 ROM with full Swedish character support (å ä ö Å Ä Ö é ü Ü ¤)
- Exact 40×24 character screen (320×240) with correct glyph rendering and graphics (mosaic) mode
- 50 Hz hardware strobe interrupt — essential for BASIC to work properly
- Real-time clock (`TIME` variable) working correctly
- USB CDC serial keyboard — works with any terminal (minicom, screen, PuTTY, etc.) at any baud rate
- Proper handling of Swedish letters via UTF-8
- Arrow keys and common meta-key combinations supported
- Built-in powerful **monitor / debugger / assembler**


#### Hardware Controls
- *Button X* — Toggle monitor mode
- *Button Y* — Reset the emulated ABC80


### Built-in Monitor

Press *Button X* to enter the monitor:


#### Basic commands
- `D [addr]` — Hex dump 64 bytes
- `U [addr]` — Disassemble 16 instructions
- `R`        — Show Z80 registers and flags
- `S`        — BASIC memory status (BOFA, EOFA, HEAP, free memory…)
- `V`        — List all variables with current values
- `E`        — Show device list (`enhetslistan`)
- `? N`      — Show error message for ABC80 error code N
- `G [addr]` — Go / resume execution (default = current PC)
- `H`        — Help
- `Q` or `X` — Quit monitor and return to ABC80


#### Integrated Assembler (line-numbered editor)

You can write Z80 assembly directly in the monitor:

```asm
A 10     LD HL,42
A 20     LD (HL),A
A 30     RET
```

Commands:
- `A n text`  — Add or replace line `n`
- `A n`       — Delete line `n`
- `AL`        — List all lines
- `AL 10 50`  — List lines 10 to 50
- `AC`        — Clear all assembler lines
- `AS 8000`   — Assemble the program to address 8000h
- `P`         — Load the built-in *SNAKE* demo game source

After assembling ..

```asm
AS 8000
```

Switch back to ABC80 mode and run it ..

```basic
CALL 32768
```


### SD: Device (in progress)

A virtual `SD:` device is already injected into the ABC80 `enhetslistan`.

Current status:
- `SAVE "SD:PROG.BAC"` and `LOAD "SD:PROG.BAC"` work (tokenized programs)
- `SAVE "SD:PROG.BAS"` and `LIST "SD:PROG.BAS"` also supported (ASCII)
- All I/O goes through a blocking buffer mechanism that matches the original ABC80 driver model

*Next*: Connect a second Pico (via WiFi) that handles a real microSD card using FatFS.  
The current code already contains clean stubs for WiFi-based communication ..


### Keyboard

- Swedish characters work automatically when your terminal sends UTF-8
- Backspace / Delete --> ABC80 backspace
- Ctrl+C --> BREAK
- Arrow keys supported
- Option/Alt + keys mapped for several special characters


### Building & Running

1. Set up the Pico SDK as usual.
2. Build with CMake:
   ```bash
   mkdir build && cd build
   cmake ..
   make -j
   ```
3. Copy `abc80_pico.uf2` to your Pico 2W.
4. Connect any terminal to the USB serial port.
5. Enjoy the classic ABC80 startup.

