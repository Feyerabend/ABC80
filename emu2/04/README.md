
![Airfight Returns ..](./../../assets/images/airfight-emu.png)

## ABC80 Emulator: AIRFIGHT returns ..

> [!NOTE]
> *Work in progress: this is an early sketch, not a finished project.*
> The emulator core is functional but several features are incomplete or
> known to have bugs.  The bundled AIRFIGHT game in particular is still
> being tuned and will be refined further.  Expect rough edges.

> *Bundled game: AIRFIGHT*

A two-player dogfight game for the ABC80, originally written in BASIC by
Kristian Lidberg and Set Lonnert in 1981.  This version is a Z80 assembly
port of kind, pre-assembled on the host at build time and embedded directly
in the Pico firmware.

### Running

1. Press __Button B__ to enter the monitor (screen turns amber).
2. Type `P` - loads the AIRFIGHT binary into Z80 RAM at address `8000H`.
3. Type `G` (or `G 8000`) - jumps to the game entry point.

The game runs until a winner is found or time expires, then asks `SPELA IGEN?
(J/N)` - J restarts with the same names and mode, N returns to the intro.

### Controls

| Key | Player 1 | Player 2 |
|-----|----------|----------|
| Turn left  | `A` | `J` |
| Turn right | `D` | `L` |
| Fire       | `X` | `M` |

Controls are read over USB CDC serial (the same terminal used for BASIC).

### Game modes

| Mode | Description |
|------|-------------|
| Score (P) | First to hit the opponent 10 times wins |
| Time (T) | Most hits when the countdown reaches 0:00 wins; default 2 minutes |

### Intro screen

The title screen uses ABC80 mosaic (teletext) graphics to draw the original
1981 logo (large letters).  Rows 0-13 contain the logo; rows 9-13 hold a
decorative border. After the logo, the game asks whether the players have
played before - if yes, the instruction screen is skipped.

### Screen layout

```
Row  0        graphics border (col 0 = 0x97, enables mosaic mode for row)
Rows 1-19     play area (20 rows x 38 cols, planes and bullets move here)
Row  20       HUD: P1 score | P2 score | timer (time mode only)
Rows 21-23    messages, prompts, winner announcement
```

### Aircraft sprites

Each plane is a 2-cell mosaic sprite.  There are 8 directions (N, NE, E, SE,
S, SW, W, NW); player 1 and player 2 use different mosaic patterns so they
are distinguishable at a glance.  Turning is rate-limited (`TURN_DELAY`
frames between turns) to prevent diagonal directions being skipped by fast
key repeats.

### Bullets

One bullet per player can be in flight at a time.  Each frame the bullet
advances `BUL_STEPS` cells in the firing direction.  If it leaves the play
area or hits the opponent's sprite it is removed.  A hit shows brief debris
at the opponent's position, plays an explosion sound, increments the
shooter's score, resets both planes to their start positions, and pauses
~1.5 s before the round continues.

### Sounds

| Event | Sound |
|-------|-------|
| Startup | Intro whoosh |
| Player 1 fires | `SOUND_SH1` (boom, VCO+noise) |
| Player 2 fires | `SOUND_SH2` (boom, slightly different) |
| Hit / explosion | `SOUND_HIT` one-shot burst |
| Victory tune | Original 1981 melody: 6 notes, rising pitch & duration |

The victory tune is reproduced from the original BASIC source using a
software-driven square wave (direct `OUT 6` toggling, same as the original).
The `TUNE_SCALE` constant in `airfight.asm` controls the inner-loop waste
count; increase it if the pitch sounds too high, decrease if too low.

### Build pipeline

`airfight.asm` is *not* assembled on the Pico at run time.  CMake builds a
native `z80asm_host` binary from the same `z80asm.c` source and uses it to
produce `airfight.bin`, which `xxd -i` then converts to the C array
`airfight_bin.h`.  That header is compiled into the firmware and the monitor's
`P` command loads the array directly into Z80 RAM.

```
src/airfight.asm  -->  z80asm_host  -->  airfight.bin  -->  xxd  -->  airfight_bin.h
                                                                  v
                                                         linked into firmware
```

### Key constants (airfight.asm)

| Constant | Default | Meaning |
|----------|---------|---------|
| `SCORE_LIM` | 10 | Hits needed to win in score mode |
| `TIME_MIN` | 2 | Starting minutes in time mode |
| `BUL_STEPS` | 3 | Cells a bullet travels per 100 ms frame |
| `TURN_DELAY` | 3 | Frames between allowed turns (anti-double-fire) |
| `TUNE_SCALE` | 20 | Victory tune inner-loop waste; tune for correct pitch |
| `VAR_BASE` | `8F00H` | Start of 256-byte RAM variable block |

### Known issues / TODO

- *Play-again in time mode*: `TMIN_V` is not restored on `RESTART_GAME`,
  so the timer immediately expires again.  Fix: reset `TMIN_V` to `TIME_MIN`
  inside `GAME_INIT` when `MODE_TDIR != 0`.
- *Victory tune pitch*: `TUNE_SCALE` is an approximation of the original
  ABC80 BASIC interpreter speed.  Pitch is wrong. Game crash.
- *Logo rows 6-8*: currently blank (gap between upper and lower logo halves).
  Not to worry.
- Not finished ..


### Building

Requires Pico SDK 2.2.0, pico-extras, and the ARM GCC toolchain.
The VS Code Pico extension installs these automatically.

```sh
mkdir build && cd build
cmake ..
make -j4
```

Flash `abc_pico.uf2` to the Pico in BOOTSEL mode.


### Source layout

```
02/
+-- src/
│     +-- main.c          display loop, 50 Hz strobe, button handling
│     +-- abc80.c         ABC80 machine init, keyboard poll, screen RAM
│     +-- z80.c           Z80 CPU core
│     +-- z80asm.c        Z80 assembler (embedded lib + standalone host build)
│     +-- disasm.c        Z80 disassembler
│     +-- display.c       VGA driver + framebuffer (pico_scanvideo_dpi)
│     +-- monitor.c       built-in debugger / assembler; P cmd loads AIRFIGHT
│     +-- sn76477.c       SN76477 sound chip emulation (PWM output)
│     +-- sd_device.c     ABC80 SD: device driver (PC-trap based)
│     +-- sd_fat.c        FatFS glue
│     +-- diskio.c        bit-bang SPI SD card driver
│     +-- airfight.asm    AIRFIGHT Z80 game source (pre-assembled at build time)
+-- include/            header files
+-- build/
│     +-- z80asm_host     native assembler (built from z80asm.c, no SDK)
│     +-- airfight.bin    assembled game binary (8000H-8xxxH)
│     +-- airfight_bin.h  C array generated by xxd; linked into firmware
+-- CMakeLists.txt
+-- PLAN.txt            phase-by-phase implementation log for AIRFIGHT
```
