
## Transpilation plan: abclisp --> Z80 assembly

This document describes how to translate the C interpreter into native Z80
assembly, step by step.  The goal is the same goal Microsoft BASIC had in
1977: a complete language system that fits in ROM and boots into a prompt on
*any* Z80-based machine.  The only machine-specific code needed is a handful
of I/O routines--everything else is portable Z80.

The toolchain is already in this repo:

| Tool         | File                    | Role                                |
|--------------|-------------------------|-------------------------------------|
| Emulator     | `z80.c` / `z80.h`       | Accurate Z80 — runs `m[65536]`      |
| Assembler    | `z80asm.c` / `z80asm.h` | 2-pass assembler --> `m[]`          |
| Disassembler | `disasm.c` / `disasm.h` | Verify assembled code               |
| Harness      | `test_vm.c`             | Phases 0–10 proven, 78 tests pass   |



### What "transpile" means here

There are three levels of ambition:

| Level           | What runs on Z80       | What runs on host     |
|-----------------|------------------------|-----------------------|
| *A — VM only*   | Bytecode VM            | Parser + compiler (C) |
| *B — VM + REPL* | VM + I/O loop          | Parser + compiler (C) |
| *C — Full ROM*  | Parser + compiler + VM | Nothing               |

*Start at level A.*  It proves the critical path--that Lisp programs
execute correctly on real Z80 silicon--without touching the parser.
Levels B and C expand outward from the same core.



### Platform-agnostic design

The key insight: the abclisp VM is pure computation.  Every pool (ops, args,
heap, envs, funs, syms, strs, stk) is an indexed table in RAM.  The entire
VM--fetch, dispatch, every op handler--contains *zero machine-specific
instructions*.  The only machine-specific code is:

```
PUTCHAR  — send one byte to the terminal
GETCHAR  — read one byte from the terminal
INIT     — set SP, clear pools, call k_init
```

This is exactly the CP/M model: a portable kernel plus a thin BIOS.
Microsoft BASIC used the same pattern: one binary, many machines, one page of
port I/O glue per machine.

#### Target profiles

Define a `config.inc` per target.  Everything else `INCLUDE`s it.

```asm
; config-sbc.inc  (bare Z80 SBC, character I/O on port 0/1)
TARGET       DEFM  "Generic Z80 SBC"
PORT_OUT     EQU   0          ; OUT (0), A  --> terminal
PORT_IN      EQU   1          ; IN  A, (1)  <-- terminal
ROM_BASE     EQU   0x0000     ; ROM starts here
RAM_BASE     EQU   0x4000     ; all pools here
STACK_INIT   EQU   0xFE00     ; hardware stack top

; config-spectrum.inc  (ZX Spectrum 48K)
; ROM: 0x0000–0x3FFF (16 KB Sinclair BASIC ROM — already there)
; We load into RAM at 0x8000 (page 2, above display file)
TARGET       DEFM  "ZX Spectrum 48K"
PORT_OUT     EQU   0xFF       ; printer port (or use ROM RST 16 for CHAN output)
PORT_IN      EQU   0xFE       ; keyboard
RAM_BASE     EQU   0x8000     ; above display file + attributes
STACK_INIT   EQU   0xFF40     ; below system variables

; config-cpm.inc  (CP/M — any Z80 machine running CP/M 2.2)
; BIOS console I/O via CP/M system calls (BDOS call 2/1)
TARGET       DEFM  "CP/M 2.2"
RAM_BASE     EQU   0x0100     ; TPA starts at 0x0100
STACK_INIT   EQU   0x0100     ; CP/M sets SP on entry; we reset it

; config-trs80.inc  (TRS-80 Model I / III)
TARGET       DEFM  "TRS-80 Model I"
PORT_OUT     EQU   0xFF       ; video memory at 0x3C00–0x3FFF (memory-mapped)
RAM_BASE     EQU   0x4200     ; after system workspace
STACK_INIT   EQU   0x43FF
```

*The PUTCHAR / GETCHAR stubs per target:*

```asm
; Generic SBC (port I/O)
PUTCHAR:  OUT  (PORT_OUT), A   ;  1 instruction
          RET

GETCHAR:  IN   A, (PORT_IN)
          RET

; ZX Spectrum (via ROM RST 16--simplest possible approach)
PUTCHAR:  RST  16              ; ROM prints A to current stream
          RET

GETCHAR:  ; call ROM keyboard scanner at 0x028E
          CALL 0x028E
          RET

; CP/M (via BDOS)
PUTCHAR:  PUSH BC \ PUSH DE \ PUSH HL
          LD   E, A
          LD   C, 2            ; BDOS function 2 = CONOUT
          CALL 5               ; BDOS entry
          POP  HL \ POP DE \ POP BC
          RET

GETCHAR:  PUSH BC \ PUSH DE \ PUSH HL
          LD   C, 1            ; BDOS function 1 = CONIN
          CALL 5
          POP  HL \ POP DE \ POP BC
          RET
```

Everything above this layer--the VM, the compiler, the parser--is pure
portable Z80.  Change `config.inc` and recompile; the Lisp ROM boots on a
different machine.

#### The macro-assembler portability layer

The assembler in this repo (`z80asm.c`) already supports everything needed:

- `EQU` for named constants
- `IF / ELSE / ENDIF` for conditional assembly
- `INCLUDE` (trivial to add) for per-target config files
- `DEFM`, `DEFW`, `DEFB` for data

A typical conditional I/O section:

```asm
INCLUDE "config.inc"          ; one line to switch targets

PUTCHAR:
IF TARGET_SPECTRUM
    RST  16
ELSE
    OUT  (PORT_OUT), A
ENDIF
    RET
```

This is not a new idea--it is exactly how Locomotive BASIC on the Amstrad
CPC and Microsoft BASIC-80 were structured.  The kernel was portable; the
machine HAL was swapped.

*Should we build a macro-assembler instead of direct transpilation?*

For the initial port: no.  The existing `z80asm.c` is already sufficient.
Write the Z80 assembly first with hardcoded port numbers; once it works on one
machine, extracting the machine-specific parts into `config.inc` is a one-hour
refactoring pass.

For subsequent ports (different Z80 machines): yes, absolutely.  Once the
first working `.asm` exists, a `config.inc` plus a few `IF` guards is the
right abstraction.  The VM and compiler assembly will never need to change.



### Confirmed Z80 constraints (from test_vm.c)

The test harness (`test_vm.c`) proved phases 0–6 and revealed two non-obvious
Z80 constraints that differ from what you might write in C or pseudo-code.

#### Constraint 1 — LD (nn), r is only valid for r = A

In Z80, the only legal 8-bit store to an absolute address is `LD (nn), A`.
Storing any other register requires going through A or using a 16-bit form:

```z80
; WRONG — not a valid Z80 instruction:
LD   (0x8000), E
LD   (0x8000), H

; RIGHT — 8-bit via A:
LD   A, E
LD   (0x8000), A

; RIGHT — 16-bit store (HL always; BC/DE via ED prefix, both valid):
LD   (0x8000), HL      ; opcode: 22 00 80
LD   (0x8000), DE      ; opcode: ED 53 00 80
LD   (0x8000), BC      ; opcode: ED 43 00 80
```

*Impact on the VM:* Every time a handler stores a result Val to memory, it
must use `LD (nn), HL` (when the result is in HL) rather than two separate
8-bit stores.  For the Lisp stack (IX-relative), `LD (IX+0), L` and
`LD (IX+1), H` are both legal--the restriction only applies to absolute
`(nn)` addressing.

#### Constraint 2 — ADD HL, HL doubles the full 16-bit HL

When computing a word-array index (`base + 2 * index`), the natural C
translation of `index * 2 + base` doubles the *entire* HL--including the
base address bits already loaded into H.

```z80
; WRONG — doubles 0x4200 into 0x8400:
LD   HL, 0x4200        ; HL = base
LD   E, B              ; E  = index
LD   D, 0
ADD  HL, HL            ; HL = 0x8400  <-- BASE doubled, not index!
ADD  HL, DE

; RIGHT — double only the index, then add base:
LD   L, B              ; HL = index (zero-extended)
LD   H, 0
ADD  HL, HL            ; HL = 2 * index   (only index doubled)
LD   DE, 0x4200        ; DE = base
ADD  HL, DE            ; HL = base + 2 * index
```

*Impact on the VM:* The fetch loop loads `args[ip]` from the parallel word
array.  This pattern appears at least three times: loading `vm_arg`, indexing
`optbl`, and loading env values.  Always double the *index register* before
adding the base.



### Phase 0 — Test harness -> DONE

`test_vm.c` is the test harness.  It assembles Z80 snippets using
`z80asm_assemble()`, runs them through the emulator, and checks results.

```
78 / 78 tests pass.  Build:
  cc -std=c11 -O2 -Wno-unused-function -o test_vm test_vm.c
```

The harness is a single-translation-unit file that includes `z80.c`,
`z80asm.c`, and `disasm.c` directly.  The disassembler requires a memory-read
hook `z80_read_mem`; the stub in `test_vm.c` is just `return m[addr]`.

Tests proven so far:

| Phase | What is tested                                                       |
|-------|----------------------------------------------------------------------|
| 0     | `LD A,42 / HALT` — harness alive                                     |
| 2a    | `4×RRCA; AND 0Fh` extracts tag from NIL, INT, SYM, BOOL             |
| 2b    | `AND 0Fh / L` extracts 12-bit payload                                |
| 3     | `LD (IX+d),r` / `LD r,(IX+d)` Lisp stack PUSH/POP, LIFO ordering    |
| 4     | 12-bit payload ADD with 12-bit wrap (`INT(2000)+INT(100)=INT(52)`)   |
| 5     | Full fetch-dispatch jump table: NOP, NOP, HALT --> halted, ip=3      |
| 6     | End-to-end mini-VM: PUSH INT(3), PUSH INT(4), ADD, HALT --> INT(7)   |
| 7     | ENV_ADDR, ENV_NEW, ENV_DEF, ENV_GET, ENV_UPDATE, ENV_CHAIN           |
| 8a    | FUN_ADDR: fid → correct base address                                 |
| 8b    | Lambda call: `((lambda (x) (+ x 1)) 41)` → INT(42)                  |
| 9     | OP_DISPLAY: INT→decimal via port PUTCHAR and memory-mapped PUTCHAR   |
| 9b    | OP_DISPLAY extended: NIL→"()", BOOL→"#t"/"#f", CHAR→char, STR→string |
| 10    | OP_SUB, OP_MUL (shift-and-add), OP_JZ; cross-check: C VM == Z80 VM   |



### Phase 1 — Memory layout

Before writing any VM code, fix the address map.  Everything else depends
on it.  Use a `config.inc` (see above) and define all pools as `EQU`.

*Generic SBC layout (reference):*

```
0x0000 – 0x3FFF   ROM  (16 KB)  — interpreter code
0x4000 – 0xBFFF   RAM  (32 KB)  — pools and user data
0xFE00 – 0xFEFF   Z80 hardware stack (256 bytes)
0xFF00 – 0xFFFF   Reserved / I/O shadow
```

Pool base addresses (EQU table--lock this in before writing any code):

```asm
; VM state scalars
VM_IP    EQU  0x4000   ; uint8_t  — current bytecode ip
VM_ENV   EQU  0x4001   ; uint8_t  — current env frame index
VM_FID   EQU  0x4002   ; uint8_t  — current function id
VM_HALT  EQU  0x4003   ; uint8_t  — 0=running, 1=halted
VM_RES   EQU  0x4004   ; uint16_t — vm_result (2 bytes)
VM_ARG   EQU  0x4006   ; uint16_t — current instruction argument
NHEAP    EQU  0x4008   ; uint8_t  — heap allocation pointer
NENVS    EQU  0x4009   ; uint8_t
NFUNS    EQU  0x400A   ; uint8_t
NSTRS    EQU  0x400B   ; uint8_t
NSYMS    EQU  0x400C   ; uint8_t  — user symbol count only (see sym table section)
NCODE    EQU  0x400D   ; uint8_t
STK_SP   EQU  0x400E   ; uint8_t  — Lisp value stack pointer
CS_SP    EQU  0x400F   ; uint8_t  — call frame stack pointer

; Pools
OPS_BASE  EQU  0x4100   ; uint8_t  ops[255]             255 bytes
ARGS_BASE EQU  0x4200   ; uint16_t args[255]            510 bytes
STK_BASE  EQU  0x4400   ; uint16_t stk[32]               64 bytes
CS_BASE   EQU  0x4440   ; Frame    cs[16]  (3B each)     48 bytes
SYMS_BASE EQU  0x4480   ; char     user_syms[24][12]    288 bytes  (see sym section)
STRS_BASE EQU  0x4580   ; char     strs[32][16]         512 bytes
HEAP_BASE EQU  0x4780   ; Cell     heap[64] (16B each) 1024 bytes  (padded)
ENVS_BASE EQU  0x4B80   ; Env      envs[32] (32B each) 1024 bytes  (padded)
FUNS_BASE EQU  0x4F80   ; Fun      funs[48] (8B each)   384 bytes  (padded)
; Total used: ~4.8 KB.  0x5100 onward is free for user data.
```

*Struct padding to powers of 2* is one of the first real design decisions.
Non-power-of-2 sizes force slow multiply-by-odd-constant for indexing.
Padding wastes a few bytes but saves many instructions:

| Struct | Natural size | Padded to | Index becomes |
|--------|--------------|-----------|---------------|
| Cell   | 13 bytes     | 16 bytes  | `i << 4`      |
| Env    | 20 bytes     | 32 bytes  | `i << 5`      |
| Fun    | 7 bytes      | 8 bytes   | `i << 3`      |

Do this padding in `lisp.c` first--add pad bytes to each struct--then the
Z80 index arithmetic is clean left-shifts.

Lock this down first.  Changing it later cascades everywhere.



### Phase 2 — Register conventions

Decide once; never violate.

```
DE   = ip register (address of current opcode byte in OPS_BASE)
       DE = OPS_BASE + vm_ip
       Fetch:  LD A,(DE) / INC DE
       On CALL/RET: save/restore from call-frame stack (CS_BASE)

HL   = working value register — the "accumulator" for Val words
       Most op handlers: receive result in HL, call LISP_PUSH

BC   = vm_arg (the 16-bit argument of the current instruction)
       Loaded in fetch loop from ARGS_BASE

A    = scratch — tag comparisons, flag checks

IX   = Lisp value stack pointer (base STK_BASE, grows up by 2)

IY   = reserved for future use; avoid (slow on Z80, 4-byte prefix)

SP   = Z80 hardware stack — subroutine CALL/RET only
       Never used for Lisp values
```

Key insight: `DE` tracking the ip mirrors how the Z80 CPU fetches its own
instructions.  After `INC DE` and `JP (HL)` dispatch, DE already points to
the next opcode — no extra work.



### Phase 3 — Val helpers ✓ PROVEN

Tag and payload extraction are verified in the harness (Phase 2a/2b tests).

```z80
; VAL_TAG: tag nibble of Val in HL --> A  (destroys A only)
VAL_TAG:
    LD   A, H
    RRCA \ RRCA \ RRCA \ RRCA
    AND  0x0F
    RET

; VAL_DAT: 12-bit payload of Val in HL --> HL  (clears upper nibble of H)
VAL_DAT:
    LD   A, H
    AND  0x0F
    LD   H, A
    RET

; MKVAL: tag in A (0..7) + 12-bit payload in HL --> HL
MKVAL:
    RLCA \ RLCA \ RLCA \ RLCA   ; A = tag << 4
    OR   H                       ; merge with payload high bits
    LD   H, A
    RET
```



### Phase 4 — Lisp stack primitives  PROVEN

IX is the Lisp stack pointer; each Val is 2 bytes (little-endian).
The harness proves PUSH/POP round-trips and LIFO ordering.

```z80
; LISP_PUSH: push HL onto Lisp value stack (IX = stack pointer)
LISP_PUSH:
    LD   (IX+0), L
    LD   (IX+1), H
    INC  IX
    INC  IX
    RET

; LISP_POP: pop top of Lisp stack into HL
LISP_POP:
    DEC  IX
    DEC  IX
    LD   L, (IX+0)
    LD   H, (IX+1)
    RET
```

*Note:* `LD (IX+d), r` is valid for any 8-bit register `r`.  This only
works when IX is close to the value — IX+d displacement is signed 8-bit
(−128..+127).  Since the Lisp stack is at most 64 bytes deep, IX-relative
access works throughout.

For larger pools (heap, env, funs), compute the absolute address with HL
arithmetic then use `LD r, (HL)` / `LD (HL), r`.



### Phase 5 — The fetch-dispatch loop  PROVEN

The harness (Phase 5 test) proves the full NOP/HALT dispatch.
The Phase 6 test proves fetch with args[] loading and the ADD handler.

```z80
VM_RUN:
    LD   IX, STK_BASE      ; Lisp stack pointer
    LD   SP, STACK_INIT    ; hardware stack
    XOR  A
    LD   B, A              ; vm_ip = 0
    LD   DE, OPS_BASE      ; DE = instruction pointer into ops[]

FETCH:
    ; ---- load opcode ----
    LD   A, (DE)            ; A  = ops[ip]
    INC  DE                 ; ip++

    ; ---- load vm_arg = args[ip-1] ----
    ; (Constraint 2: double only the index, not the base)
    PUSH AF                 ; save opcode
    LD   A, E
    SUB  LOW(OPS_BASE) + 1  ; index = DE_low - base_low - 1  (already incremented)
    LD   L, A
    LD   H, 0
    ADD  HL, HL             ; HL = 2 * index
    LD   BC, ARGS_BASE
    ADD  HL, BC             ; HL = &args[index]
    LD   C, (HL)
    INC  HL
    LD   B, (HL)            ; BC = vm_arg
    POP  AF                 ; restore opcode

    ; ---- dispatch via jump table ----
    LD   L, A
    LD   H, 0
    ADD  HL, HL             ; HL = opcode * 2
    LD   BC, OPTBL
    ADD  HL, BC             ; HL = &optbl[opcode]
    LD   A, (HL)
    INC  HL
    LD   H, (HL)
    LD   L, A               ; HL = handler address
    JP   (HL)               ; ---- dispatch ----
    ; each handler ends with JP FETCH
```

OPS_BASE is at 0x4100 — the low byte is 0x00, so
`SUB LOW(OPS_BASE) + 1` = `SUB 1` after DE has been incremented.
If OPS_BASE is page-aligned, the index is just `E - 1` (one instruction).

```z80
OPTBL:
    DEFW OP_NOP
    DEFW OP_PUSH
    DEFW OP_POP
    DEFW OP_DUP
    DEFW OP_LOAD
    DEFW OP_STORE
    DEFW OP_ADD
    ; ... one entry per opcode, in enum order
```



### Phase 6 — Op handlers, simplest first

Build in tier order.  Test each tier with the harness before adding the next.

#### Tier 1 — Trivial  PROVEN

```z80
OP_NOP:   JP FETCH

OP_PUSH:  ; BC = vm_arg = Val to push
    LD   H, B
    LD   L, C
    CALL LISP_PUSH
    JP   FETCH

OP_HALT:
    CALL LISP_POP
    LD   (VM_RES),   L
    LD   (VM_RES+1), H
    LD   A, 1
    LD   (VM_HALT),  A
    JP   FETCH          ; halt flag checked at top of FETCH — loop exits
```

#### Tier 2 — Arithmetic  PROVEN (ADD)

```z80
OP_ADD:
    CALL LISP_POP       ; HL = b (TOS)
    PUSH HL
    CALL LISP_POP       ; HL = a (second)
    POP  BC             ; BC = b
    ADD  HL, BC         ; HL = a + b (16-bit, carries into tag bits)
    LD   A, H
    AND  0x0F           ; mask tag bits from result
    OR   0x10           ; tag = T_INT (1 << 4)
    LD   H, A
    CALL LISP_PUSH
    JP   FETCH
```

`OP_SUB` is the same pattern with subtraction.  Use
`LD A,L / SUB C / LD L,A / LD A,H / SBC A,B / AND 0x0F / OR 0x10 / LD H,A`
to avoid the `SBC HL, BC` carry complication.

`OP_MUL` needs software multiply (Z80 has no MUL instruction):

```z80
MUL12:
    ; HL = HL * DE, result in HL (12-bit, wraps)
    ; Shift-and-add, 12 iterations
    LD   B, 12
    LD   C, L           ; save multiplicand low
    LD   A, H           ; save multiplicand high (shift DE = multiplier)
    LD   HL, 0          ; accumulator = 0
.loop:
    SRL  A              ; shift multiplier right (D:E --> carry)
    RR   C
    JR   NC, .skip
    PUSH BC             ; save B (loop counter) — ADD HL,DE clobbers nothing but flags
    ADD  HL, DE
    POP  BC
.skip:
    DJNZ .loop
    LD   A, H
    AND  0x0F
    OR   0x10           ; T_INT tag
    LD   H, A
    RET
```

#### Tier 3 — Comparison and boolean

```z80
OP_EQ:
    CALL LISP_POP
    PUSH HL
    CALL LISP_POP
    POP  BC
    LD   A, H \ CP B \ JR NZ, .false
    LD   A, L \ CP C \ JR NZ, .false
    LD   HL, 0x5001     ; BOOL_T = MK(T_BOOL,1)
    JR   .done
.false:
    LD   HL, 0x5000     ; BOOL_F = MK(T_BOOL,0)
.done:
    CALL LISP_PUSH
    JP   FETCH
```

`OP_LT`: extract payloads, sign-extend at bit 11, compare as 16-bit signed.

#### Tier 4 — Jumps

```z80
OP_JMP:
    ; BC = new ip (from vm_arg)
    LD   A, C
    LD   E, A
    LD   D, HIGH(OPS_BASE)
    JP   FETCH

OP_JZ:
    CALL LISP_POP
    CALL VAL_TAG        ; A = tag
    CP   0              ; T_NIL --> falsy
    JR   Z, .jump
    CP   5              ; T_BOOL
    JR   NZ, .nojump
    LD   A, L
    AND  0x0F
    OR   H
    JR   Z, .jump       ; T_BOOL, payload 0 --> #f --> falsy
.nojump:
    JP   FETCH
.jump:
    LD   A, C
    LD   E, A
    LD   D, HIGH(OPS_BASE)
    JP   FETCH
```

#### Tier 5 — CONS / CAR / CDR

These allocate heap cells.  Struct padding makes the address calculation clean:

```z80
; CELL_ADDR: i in HL --> HL = HEAP_BASE + i*16  (Cell padded to 16 bytes)
CELL_ADDR:
    ADD  HL, HL \ ADD HL, HL \ ADD HL, HL \ ADD HL, HL   ; × 16
    LD   BC, HEAP_BASE
    ADD  HL, BC
    RET
```



### Phase 7 — Environment helpers

These are called on every variable reference.  Get them right first.

```asm
; ENV_SIZE = 32  (Env padded to 32 bytes for clean i << 5 indexing)
; Layout at ENVS_BASE + i*32:
;   offset  0..5:  key[6]    (uint8_t sym indices)
;   offset  6..17: val[6]    (uint16_t values, little-endian)
;   offset 18:     n         (uint8_t)
;   offset 19:     parent    (uint8_t, 0xFF = no parent)
;   offset 20..31: pad

; ENV_ADDR: env index in A --> HL = base address of frame
ENV_ADDR:
    LD   L, A
    LD   H, 0
    ADD  HL, HL            ; × 2
    ADD  HL, HL            ; × 4
    ADD  HL, HL            ; × 8
    ADD  HL, HL            ; × 16
    ADD  HL, HL            ; × 32  (= 1 << 5)
    LD   BC, ENVS_BASE
    ADD  HL, BC
    RET

; ENV_GET: look up sym index in C, starting at env frame index in B
;   Returns: HL = value, CF=1 found, CF=0 not found (NIL returned)
ENV_GET:
.frame:
    LD   A, B
    CP   0xFF              ; ENV_NULL?
    JR   Z, .notfound
    CALL ENV_ADDR          ; HL = &envs[B]
    PUSH HL                ; save frame base
    LD   A, (HL)           ; A = n (number of bindings)
    OR   A
    JR   Z, .tryparent
    LD   D, A              ; D = loop count
    INC  HL                ; HL --> key[0]
.scan:
    LD   A, (HL)
    CP   C                 ; match sym index?
    JR   Z, .found
    INC  HL
    DEC  D
    JR   NZ, .scan
.tryparent:
    POP  HL                ; frame base
    LD   BC, 19            ; offset of 'parent' field
    ADD  HL, BC
    LD   B, (HL)           ; B = parent index
    JR   .frame
.found:
    POP  IX                ; IX = frame base (discard saved HL)
    ; key slot i is at frame+1+i, val slot i is at frame+6+i*2
    ; We know HL = &key[i]; HL - frame_base - 1 = i
    ; Simpler: count from frame base in IX
    LD   A, L              ; L = &key[i] low byte
    SUB  IXL               ; A = i + 1  (relative offset from base, +1 for n byte)
    DEC  A                 ; A = i
    ADD  A, A              ; A = i*2
    ADD  A, 6              ; offset of val[i] = 6 + i*2
    LD   L, A
    LD   H, IXH
    LD   A, IXL
    ADD  A, L
    LD   L, A              ; HL = &val[i]  (low byte only; works if frame < 0xFF00)
    LD   E, (HL)
    INC  HL
    LD   D, (HL)
    EX   DE, HL            ; HL = val[i]
    SCF
    RET
.notfound:
    LD   HL, 0             ; NIL
    CCF
    RET
```

The index arithmetic here is the most error-prone section.  Write `ENV_ADDR`
in isolation and test it exhaustively before building `ENV_GET` on top.



### Phase 8 — CALL, RET, MKCLOS

These require all machinery above.  They are the most register-hungry routines.

```asm
; FUN_SIZE = 8  (Fun padded to 8 bytes)
; Layout at FUNS_BASE + fid*8:
;   offset 0: addr        (uint8_t — ip of function body)
;   offset 1: env         (uint8_t — captured env frame)
;   offset 2: argc        (uint8_t — number of parameters)
;   offset 3..6: args[4]  (uint8_t param sym indices)
;   offset 7: pad

FUN_ADDR:   ; A = fid --> HL = &funs[fid]
    LD   L, A
    LD   H, 0
    ADD  HL, HL \ ADD HL, HL \ ADD HL, HL   ; × 8
    LD   BC, FUNS_BASE
    ADD  HL, BC
    RET

OP_CALL:
    ; 1. argc = low byte of vm_arg (BC)
    ; 2. pop args[0..argc-1] from Lisp stack into scratch RAM
    ; 3. pop fn Val; verify TAG == T_FUN; fid = DAT
    ; 4. ne = env_new(funs[fid].env)
    ; 5. bind params in ne
    ; 6. push call frame: {ip=(E-OPS_BASE), env=cur_env, fid=cur_fid}
    ; 7. DE = OPS_BASE + funs[fid].addr
    ; 8. update cur_env = ne, cur_fid = fid
    ; 9. JP FETCH

; Call frame (3 bytes) at CS_BASE + csp*3:
;   byte 0: saved ip   (uint8_t)
;   byte 1: saved env  (uint8_t)
;   byte 2: saved fid  (uint8_t)
```

Z80 has only 6 general-purpose byte registers (A, B, C, D, E, H, L plus IX,
IY as pairs).  OP_CALL needs: argc, fid, ne, saved-ip, arg values.  The
approach: spill temporaries to fixed scratch addresses in RAM.

```asm
SCRATCH_A  EQU  0x4020   ; 4 bytes scratch for OP_CALL / OP_TAILCALL
SCRATCH_B  EQU  0x4024
```

`OP_MKCLOS`: copy 8 bytes from `funs[template_fid]` to `funs[nfuns]`, then
set the `.env` field to `cur_env`.  An 8-byte block copy is 8 × (LD A,(HL);
LD (DE),A; INC HL; INC DE) — explicit but correct and easy to verify.



### Phase 9 — I/O  ✓ PROVEN

#### PUTCHAR / GETCHAR abstraction  ✓ PROVEN

All character I/O in the VM goes through two subroutines at fixed addresses:

```
PUTCHAR  EQU  0x03A0   ; A = char to output; preserves all registers except F
GETCHAR  EQU  0x03C0   ; returns next char in A; 0 when no input
```

Two implementations of each are provided.  Swap which one is assembled at
the fixed address to switch the entire system's I/O model — the VM bytecode
never changes.

**Port variant** (for Z80 hardware with IN/OUT instructions):

```z80
PUTCHAR:  OUT  (0), A   ; port 0 → terminal
          RET

GETCHAR:  IN   A, (0)
          RET
```

**Memory-mapped variant** (for hardware without IN/OUT, e.g. mapped UART):

```z80
; MMIO scratch (in VM state block):
;   0x45F0  MMIO_OUT_PTR  (word) — write pointer into output buffer
;   0x45F2  MMIO_IN_PTR   (word) — read pointer into input buffer
;   0x45F4  MMIO_IN_END   (word) — one-past-end of input buffer
;   0x4800  MMIO_OUT_BUF         — output character buffer

PUTCHAR:
    PUSH HL
    LD   HL, (0x45F0)   ; load output-write pointer
    LD   (HL), A        ; store character
    INC  HL
    LD   (0x45F0), HL   ; advance pointer
    POP  HL
    RET

GETCHAR:
    PUSH HL
    PUSH DE
    LD   HL, (0x45F2)   ; IN_PTR
    LD   DE, (0x45F4)   ; IN_END
    LD   A, H \ CP D \ JR NZ, gcm_rd
    LD   A, L \ CP E \ JR NZ, gcm_rd
    XOR  A              ; buffer empty: return 0
    JR   gcm_done
gcm_rd:
    LD   A, (HL)
    INC  HL
    LD   (0x45F2), HL   ; advance IN_PTR
gcm_done:
    POP  DE
    POP  HL
    RET
```

#### OP_DISPLAY  ✓ PROVEN

Pops TOS, handles INT tag: sign-checks bit 11, outputs '-' if negative,
negates, then divides by 10 in a loop storing digits into a 5-byte scratch
buffer at 0x45E0, and prints them in reverse order.  All character output
goes through `CALL PUTCHAR` (0x03A0).

```z80
OP_DISPLAY:
    PUSH BC             ; B = vm_ip — must survive the handler
    ; pop Val into DE
    DEC  IY \ DEC  IY
    LD   E, (IY+0) \ LD D, (IY+1)
    ; check T_INT tag (high nibble of D = 0x1x)
    LD   A, D \ AND  0xF0 \ CP 0x10
    JR   NZ, odsp_done  ; not INT: silently discard
    ; extract 12-bit payload → HL
    LD   A, D \ AND  0x0F \ LD H, A \ LD L, E
    ; negative if bit 11 (H bit 3) set
    BIT  3, H
    JR   Z, odsp_pos
    LD   A, 0x2D \ CALL PUTCHAR  ; print '-'
    ; negate HL mod 2^12
    XOR  A \ SUB L \ LD L, A
    LD   A, 0 \ SBC A, H \ AND 0x0F \ LD H, A
odsp_pos:
    ; zero?
    LD   A, H \ OR L
    JR   NZ, odsp_nonz
    LD   A, 0x30 \ CALL PUTCHAR  ; print '0'
    JR   odsp_done
odsp_nonz:
    ; divide HL by 10 repeatedly into scratch buffer at 0x45E0
    LD   IX, 0x45E0
    LD   B, 0           ; digit count (vm_ip is saved on stack)
    ; ... (divide loop stores ASCII digits LSB-first; odsp_pr prints in reverse)
    ; reverse-print
    ; ...
odsp_done:
    POP  BC             ; restore vm_ip
    JP   FETCH
```

Both the port and memory-mapped PUTCHAR paths pass all 5 test cases
(INT(42), INT(0), INT(1), INT(2047), INT(-5)) in the harness.



### Phase 10 — String and char ops

Implement `STRLEN`, `STRNCPY`, `STRNCMP` as subroutines first.

```z80
STRLEN:             ; HL = string --> BC = length
    LD   BC, 0
.loop:
    LD   A, (HL)
    OR   A
    RET  Z
    INC  HL \ INC BC
    JR   .loop
```

`CPIR` (compare, increment, repeat) searches for a byte in a block — useful
for `intern` to find end-of-string before comparing.



### Phase 11 — Parser (Level C only)

Use IX as the `src` pointer (current input character).

```z80
SKIP_WS:
    LD   A, (IX+0)
    OR   A   \ RET  Z
    CP   ' ' \ JR Z, .skip
    CP   0x0A\ JR Z, .skip   ; '\n'
    CP   0x09\ JR Z, .skip   ; '\t'
    CP   ';' \ JR NZ, .done
.comment:
    LD   A, (IX+0)
    OR   A   \ RET  Z
    INC  IX
    CP   0x0A\ JR NZ, .comment
    JR   SKIP_WS
.skip:
    INC  IX
    JR   SKIP_WS
.done:
    RET
```

Recursion in `parse_list --> parse_expr --> parse_list` uses the hardware stack.
At ~20 bytes per Z80 frame, 256 bytes of hardware stack supports ~12 nesting
levels — fine for 80s-style programs.  Increase `STACK_INIT` gap if needed.



### Phase 12 — Compiler (Level C only)

`EMIT` is the core:

```z80
EMIT:   ; A = opcode, BC = arg
    LD   HL, OPS_BASE
    LD   E, (NCODE)
    LD   D, 0
    ADD  HL, DE
    LD   (HL), A                ; ops[ncode] = op
    LD   L, E \ LD H, 0
    ADD  HL, HL                 ; 2 * ncode
    LD   DE, ARGS_BASE
    ADD  HL, DE                 ; &args[ncode]
    LD   (HL), C
    INC  HL
    LD   (HL), B                ; args[ncode] = arg (little-endian)
    LD   A, (NCODE)
    INC  A
    LD   (NCODE), A
    RET
```

Backpatching (`args[slot] = ncode`):

```z80
PATCH_ARG:  ; A = slot to patch
    LD   L, A \ LD H, 0
    ADD  HL, HL
    LD   BC, ARGS_BASE
    ADD  HL, BC
    LD   A, (NCODE)
    LD   (HL), A
    INC  HL
    LD   (HL), 0
    RET
```

The compiler's `IS_KW(head, K.xxx)` checks become single `CP` instructions
once keywords have fixed ROM indices (see sym table section below):

```z80
; head in HL — check if it is K_IF
    CALL VAL_TAG
    CP   T_SYM     \ JR NZ, .not_kw
    LD   A, L      ; sym index (payload fits in L for sym indices < 256)
    CP   K_IF      \ JP Z, COMPILE_IF
    CP   K_DEFINE  \ JP Z, COMPILE_DEFINE
    CP   K_LAMBDA  \ JP Z, COMPILE_LAMBDA
    ; ... each check is two instructions
```



### Sym table optimization — ROM keyword indices

*This is the biggest remaining RAM saving available.*

#### The problem

`syms[64]` costs 768 bytes of RAM.  About 40 of those 64 slots are occupied
by fixed keywords (`define`, `lambda`, `if`, `let`, `letrec`, `and`, `or`,
`display`, `write`, `read`, `error`, `apply`, `list`, `append`, and all the
type predicates).  These keywords are *invariant* — their names never change
and their indices are set at startup by `k_init()`.

On a ROM system those 40 entries serve no purpose in RAM: they never change,
they take up 480 bytes of RAM that could be free, and they make the sym table
larger (slower `intern` scans).

#### The fix — two-tier symbol table

Separate the symbol namespace into:

1. *Keyword table* — in ROM, fixed indices, names are literal strings in code.
2. *User symbol table* — in RAM, dynamic, allocated by `intern()`.

In `lisp.c` (C side):

```c
/* ---- ROM keyword table ---- */
/* Replace K struct runtime interns with compile-time constants. */
enum {
    K_QUOTE = 0, K_DEFINE, K_SET, K_IF, K_COND, K_ELSE,
    K_LAMBDA, K_LET, K_LETREC, K_BEGIN, K_AND, K_OR,
    K_ADD, K_SUB, K_MUL, K_EQ, K_LT, K_NOT,
    K_CONS, K_CAR, K_CDR, K_PAIRP, K_NULLP,
    K_CHARP, K_CHAR2INT, K_INT2CHAR,
    K_STRP, K_STRLEN, K_STRREF, K_STRCAT,
    K_STREQ, K_SYM2STR, K_STR2SYM, K_NUM2STR, K_STR2NUM,
    K_DISPLAY, K_NEWLINE, K_WRITE, K_READ, K_ERROR,
    K_APPLY, K_UNQUOTE, K_LIST, K_APPEND,
    NUM_KEYWORDS            /* = ~43 */
};

/* Keyword names in ROM order (matches enum above) */
static const char kw_names[][12] = {
    "quote", "define", "set!", "if", "cond", "else",
    "lambda", "let", "letrec", "begin", "and", "or",
    "+", "-", "*", "=", "<", "not",
    "cons", "car", "cdr", "pair?", "null?",
    "char?", "char->int", "int->char",
    "string?", "str-len", "str-ref", "str-append",
    "str=?", "sym->str", "str->sym", "num->str", "str->num",
    "display", "newline", "write", "read", "error",
    "apply", "unquote", "list", "append",
};

/* User symbol table (RAM only) — indices start at NUM_KEYWORDS */
#define MAX_USER_SYM  24
static char user_syms[MAX_USER_SYM][SYM_LEN];
static uint8_t n_user_syms = 0;
```

Split `intern()` into two functions:

```c
/* intern_kw: search ROM table, O(NUM_KEYWORDS) */
static int intern_kw(const char *name) {
    for (int i = 0; i < NUM_KEYWORDS; i++)
        if (strncmp(name, kw_names[i], SYM_LEN) == 0) return i;
    return -1;
}

/* intern: public — checks keywords first, then user table */
static uint8_t intern(const char *name) {
    int k = intern_kw(name);
    if (k >= 0) return (uint8_t)k;
    /* search user table */
    for (int i = 0; i < n_user_syms; i++)
        if (strncmp(name, user_syms[i], SYM_LEN) == 0)
            return (uint8_t)(NUM_KEYWORDS + i);
    /* allocate new user symbol */
    if (n_user_syms < MAX_USER_SYM) {
        strncpy(user_syms[n_user_syms], name, SYM_LEN);
        return (uint8_t)(NUM_KEYWORDS + n_user_syms++);
    }
    return 0; /* table full — return 'quote' as sentinel */
}
```

`k_init()` is eliminated entirely.  The K struct becomes the enum.
The compiler already does `IS_KW(head, K.xxx)` — that becomes `IS_KW(head,
K_XXX)` (uppercase constant), same semantics, no runtime cost.

#### RAM savings

| Before                     | After                           |
|----------------------------|---------------------------------|
| `syms[64][12]` = 768 bytes | `user_syms[24][12]` = 288 bytes |
| 40 keyword slots (wasted)  | 0 keyword slots in RAM          |
| *Net saving: ~480 bytes*   |                                 |

On Z80 ROM, `kw_names[]` lives in ROM alongside the code — zero extra RAM.
The user symbol table shrinks from 64 to 24 entries, which is ample for
most 80s-style programs (how many different variable names does a Z80 program
really use?).

#### In Z80 assembly

```asm
; ROM keyword table (in ROM, never changes)
KW_NAMES:
    DEFM "quote",      0, 0, 0, 0, 0, 0, 0    ; 12 bytes
    DEFM "define",     0, 0, 0, 0, 0, 0       ; 12 bytes
    DEFM "set!",       0, 0, 0, 0, 0, 0, 0, 0 ; 12 bytes
    ; ...  one 12-byte padded entry per keyword

NUM_KW  EQU  43        ; length of keyword table

; INTERN_KW: name at HL --> A = keyword index, or 0xFF if not found
INTERN_KW:
    LD   DE, KW_NAMES
    LD   B, NUM_KW
    LD   C, 0          ; C = current index
.try:
    PUSH BC
    PUSH HL
    PUSH DE
    LD   B, 12
.cmp:
    LD   A, (DE) \ INC DE
    LD   A, (HL) \ INC HL  ; compare byte by byte
    ; (use LDIR to copy then compare, or just byte loop)
    CP   (DE-1)
    JR   NZ, .nomatch
    DJNZ .cmp
    ; match
    POP  DE \ POP HL \ POP BC
    LD   A, C          ; return keyword index
    RET
.nomatch:
    POP  DE \ POP HL \ POP BC
    INC  C
    LD   DE, 12
    ADD  HL, DE        ; advance to next keyword name
    DJNZ .try
    LD   A, 0xFF       ; not found
    RET
```

`INTERN` then calls `INTERN_KW` first; only if that returns 0xFF does it
search the RAM user-sym table.



### What is genuinely hard

In rough order of difficulty:

1. *×13 / non-power-of-2 struct sizes.*  Fix by padding to 8/16/32 bytes.
   Do this in `lisp.c` first (add pad fields to Cell, Env, Fun), then the
   Z80 index arithmetic is just left-shifts.

2. *env_get with correct register discipline.*  It walks a linked list,
   touching HL, BC, DE, A simultaneously.  Map registers to roles on paper
   before writing a single instruction.  Use `ENV_ADDR` as a separate
   subroutine and test it exhaustively.

3. *op_call / op_tailcall.*  They touch every data structure simultaneously:
   env pool, fun pool, call stack, value stack.  Write them last, test them
   hardest.  Use fixed scratch RAM locations to spill temporaries.

4. *Multiply.*  Write and test `MUL12` in complete isolation before using it
   inside `op_mul`.

5. *The parser's recursive descent.*  Each `parse_expr` call uses ~20 bytes
   of hardware stack.  With 256 bytes of stack (as allocated), that is ~12
   levels.  Fine for 80s programs; tight for deeply quasiquoted code.

6. *`intern` with `CPIR`.*  Z80's `CPIR` instruction (compare, increment,
   repeat until match or count exhausted) can speed up string search in the
   user symbol table.  Combine with the two-tier keyword / user-sym split to
   keep the hot path short.

7. *Closure pool exhaustion.*  `OP_MKCLOS` allocates a new Fun slot each
   time a lambda is evaluated.  With `MAX_FUN = 48`, about 30 runtime closures
   are available after templates.  A mark-and-compact GC pass would recover
   unreachable slots.  Defer until everything else works.

8. *Sym table restructuring.*  The C side must be refactored before the Z80
   side gets the benefit.  The restructuring is straightforward (split intern,
   replace K struct with enum) but touches the compiler and every `IS_KW`
   call.  Do it on the C side first, run the full test suite to confirm,
   *then* translate to Z80.



### Incremental test strategy

At every phase, test with the harness before moving on.

```
Phase 0  ✓  harness: LD A,42 / HALT --> a == 42
Phase 2  ✓  MKVAL / VAL_TAG round-trip: MK(T_INT,42) --> tag=1, payload=42
Phase 3  ✓  LISP_PUSH / LISP_POP: push/pop INT(42), LIFO ordering
Phase 4  ✓  OP_ADD: PUSH INT(3), PUSH INT(4), ADD --> INT(7), 12-bit wrap
Phase 5  ✓  fetch-dispatch: NOP, NOP, HALT --> halted, vm_ip=3
Phase 6  ✓  mini-VM end-to-end: PUSH, PUSH, ADD, HALT --> INT(7) in vm_result
Phase 7  ✓  ENV_ADDR, ENV_NEW, ENV_DEF, ENV_GET, ENV_UPDATE, ENV_CHAIN
Phase 8  ✓  FUN_ADDR; lambda call: (lambda (x) (+ x 1)) applied to 41 --> 42
Phase 9  ✓  OP_DISPLAY: INT→decimal via port PUTCHAR, display 42→"42", -5→"-5"
            MMIO PUTCHAR: same results via memory-mapped output buffer
Phase 9b ✓  OP_DISPLAY extended: NIL→"()", BOOL #t→"#t", BOOL #f→"#f",
            CHAR('A')→"A", STR("hi")→"hi"
Phase 10 ✓  OP_SUB (SBC HL,DE), OP_MUL (12-bit shift-and-add with CB-prefix
            SRL/RR/SLA/RL), OP_JZ (branch on NIL or #f)
            Cross-check: C compiler → bytecode dump → Z80 VM == C VM
            All 78 tests pass
```

The cross-check is the most powerful test: it catches register bugs, flag
bugs, and addressing bugs that targeted unit tests miss.

**Code layout note**: the VM opcode handlers must fit in 0x0000–0x01FF.
The fixed subroutines live at 0x0200–0x03BF (ENV_*, FUN_ADDR, PUTCHAR).
New handlers that would overflow 0x01FF are placed at `ORG 0x0400` and
referenced by label from the jump table — the assembler resolves the
addresses correctly across ORG sections.


### Suggested next steps

Phases 0–10 are proven by the harness (78 tests, all passing).

1. *`config.inc` split* — once the full VM works on the generic layout,
   extract the machine-specific constants (port numbers, base addresses,
   PUTCHAR/GETCHAR variant) into a per-target include file.

2. *OP_NEWLINE* — push NIL return, call PUTCHAR(0x0A).

3. *OP_EQ / OP_LT / OP_NOT* — integer comparison opcodes needed for
   `(cond ...)` and general conditionals.

4. *TAILCALL* — replace the last CALL+RET pair with TAILCALL for proper
   tail-call optimisation; reuse the current call frame.
