## abclisp

A small Lisp/Scheme interpreter written in portable C, designed from the ground
up to be transpiled to Z80 assembly — the kind of Lisp that could have lived in
the ROM of an 80s home computer.

The experiment has two parts that work together:

1. __A self-contained Lisp interpreter__ (`lisp.c`) that compiles source text to
   a simple bytecode and runs it.  Every data structure fits in 16-bit values
   (one Z80 register pair); the VM is a trampoline dispatch loop that maps
   directly onto Z80 jump tables.

2. __A Z80 proof__ (`test_vm.c`) that re-implements the same VM in real Z80
   assembly, assembled and executed inside a Z80 emulator, and shows that both
   VMs produce identical results for the same programs.

```
> (define (fact n) (if (= n 0) 1 (* n (fact (- n 1)))))
> (fact 6)
720
> `(the answer is ,(fact 6))
(the answer is 720)
```



### Why this is interesting

Most Lisp-to-Z80 stories end at "here is a design sketch."  This one ends with
a running proof: the C compiler emits bytecode, the Z80 VM executes it, and both
return the same value.  The path from `(+ 3 4)` to a Z80 `HALT` instruction is
fully traced and tested.

A few deliberate constraints make the result credible on real hardware:

- __12-bit integers__ — values wrap at 2047, mirroring Z80 register-pair
  arithmetic.  There is no silent promotion to a wider type.
- __Fixed-size pools__ — no `malloc`, no heap fragmentation.  Every pool is a
  static array; the total RAM footprint is under 4 KB.
- __One-jump dispatch__ — the VM loop indexes an opcode table and jumps directly
  to the handler, no comparison chain.  On Z80 this is four instructions.
- __Tail-call optimisation__ — self-tail-calls reuse the existing environment
  frame.  `(count 500)` stays at stack depth 1.

See [TRANSPILE.md](TRANSPILE.md) for the full phase-by-phase build-up, from
"does a HALT assemble?" through environment lookup, closures, and cross-checks.



### Building and running

No dependencies beyond a C11 compiler.

```sh
# Build the REPL
cc -std=c11 -O2 -o lisp lisp.c

# Start the interactive REPL
./lisp

# Batch mode (one complete s-expression per line)
printf '(define (square n) (* n n))\n(square 7)\n' | ./lisp
```

```sh
# Build and run the Z80 proof (78 tests)
cc -std=c11 -O2 -Wno-unused-function -o test_vm test_vm.c
./test_vm
```

```sh
# Full test suite (REPL tests + Z80 proof)
bash tests/run_tests.sh
```



### The language

abclisp is Scheme-flavoured.  Every expression returns a value; the REPL prints
non-nil results automatically.

#### Literals

| Form | Type | Example |
|------|------|---------|
| `42`, `-7` | Integer (12-bit signed, −2048..2047) | `(+ 40 2)` → `42` |
| `#t`, `#f` | Boolean | `(not #f)` → `#t` |
| `nil`, `()` | Empty list / false-ish | `(null? nil)` → `#t` |
| `#\A`, `#\space`, `#\newline` | Character | `(char->int #\A)` → `65` |
| `"hello"` | String (up to 15 chars) | `(str-len "hello")` → `5` |
| `(quote x)`, `'x` | Quoted symbol/list | `'(a b c)` → `(a b c)` |

Integers wrap silently at 12 bits — `(+ 2047 1)` gives `-2048`.

#### Core forms

```scheme
(define x 42)                        ; variable
(define (square n) (* n n))          ; function
(set! x 100)                         ; update binding

(if test then else)
(cond ((= x 1) "one") (else "other"))
(and expr ...) (or expr ...)         ; short-circuit

(lambda (x y) (+ x y))
(let ((x 3) (y 4)) (+ x y))
(letrec ((fact (lambda (n) ...))) (fact 6))
(begin e1 e2 ... eN)
```

#### Arithmetic and comparison

```scheme
(+ a b)   (- a b)   (* a b)         ; no division — Z80 has none
(= a b)   (< a b)                   ; → #t or #f
(not x)
```

#### Lists

```scheme
(cons 1 2)  (car lst)  (cdr lst)
(list 1 2 3)
(pair? x)  (null? x)
(append (list 1 2) (list 3 4))      ; → (1 2 3 4)
(apply f (list a b))
```

#### Tail-call optimisation

```scheme
(define (count n) (if (= n 0) 'done (count (- n 1))))
(count 500)    ; → done  (stack depth stays at 1)
```

#### I/O

```scheme
(display x)    ; print without quoting
(write x)      ; print in read-back form
(newline)
(read)         ; read one expression from stdin
(error n)      ; print E:N and halt
```



### How the Z80 proof works

The proof lives in `test_vm.c`.  It is a single C file that:

1. Pulls in a Z80 emulator (`z80.c`) and assembler (`z80asm.c`).
2. Pulls in the Lisp interpreter (`lisp.c`) in embedded mode — the compiler and
   bytecode arrays become available as C functions and globals.
3. Assembles the Z80 VM (handler by handler) into the emulator's 64 KB RAM.
4. Copies the compiled bytecode into the agreed memory layout.
5. Runs the emulator and reads back the result Val.

Each cross-check test compiles an expression, runs it on the C VM, runs it on
the Z80 VM, and asserts both answers match:

```
(+ 3 4)
  ip  op  arg
   0   1  0x1003   PUSH  INT(3)
   1   1  0x1004   PUSH  INT(4)
   2   6  0x0000   ADD
   3  21  0x8000   STORE → m[0x8000]
   4  23  0x0000   HALT
PASS  (+ 3 4) → INT(7) [C VM]
PASS  (+ 3 4) → INT(7) [Z80 VM]
PASS  (+ 3 4) → INT(7) [C==Z80]
```

#### Memory layout

All addresses are defined in `vm_config.h` and can be changed to retarget the
VM to a different machine:

| Address  | Contents |
|----------|----------|
| `0x0000` | VM code (fetch-dispatch loop + opcode handlers) |
| `0x0200` | Subroutines (ENV_ADDR, ENV_NEW, ENV_DEF, ENV_GET, FUN_ADDR) |
| `0x03A0` | PUTCHAR (port variant: `OUT (0),A`) |
| `0x03C0` | GETCHAR |
| `0x4000` | Environment frames (32 B × 32) |
| `0x4400` | Function/closure records (8 B × 48) |
| `0x4580` | String pool (16 B × 32) |
| `0x45A0` | VM scalars: CUR\_ENV, CUR\_FID, CSP, NFUNS, NENVS |
| `0x4600` | Lisp value stack (IY base, grows up) |
| `0x4700` | Opcode dispatch table (2 B × 37 entries) |
| `0x4900` | Bytecode opcode stream |
| `0x4A00` | Bytecode operand stream (2 B each) |

#### Val encoding

Every Lisp value is a 16-bit word — one Z80 register pair:

```
 15 14 13 12 |  11 10  9  8  7  6  5  4  3  2  1  0  |  
 +-----------+---------------------------------------+
 |    tag    |              payload (12 bits)        |
 +-----------+---------------------------------------+
```

| Tag | Type | Payload |
|-----|------|---------|
| 0   | NIL  | —       |
| 1   | INT  | 12-bit signed integer |
| 2   | SYM  | symbol table index |
| 3   | PAIR | heap cell index |
| 4   | FUN  | function record index |
| 5   | BOOL | 0 = #f, 1 = #t |
| 6   | CHAR | ASCII code |
| 7   | STR  | string pool index |

#### Opcode dispatch

The fetch loop loads the opcode, indexes the jump table, and jumps — no
`switch`, no comparisons:

```z80
fetch:
  LD   HL, 0x4900      ; OPS_BASE
  LD   E, B
  LD   D, 0
  ADD  HL, DE
  LD   A, (HL)         ; A = opcode
  ...
  LD   BC, 0x4700      ; OPTBL
  ADD  HL, BC
  LD   C, (HL)
  INC  HL
  LD   H, (HL)
  LD   L, C
  JP   (HL)            ; one jump to handler
```



### What is proven, what is not

__Proven__ (78 tests, all passing):

- Val tag/payload encoding and extraction on Z80
- Lisp stack push/pop via IY
- INT add, subtract, multiply (shift-and-add)
- Conditional jump (OP\_JZ for `if`)
- Environment lookup, definition, update across parent frames
- Closure instantiation and call (including captured environment)
- OP\_DISPLAY for INT, BOOL, NIL, CHAR, STR — both port and memory-mapped I/O
- Full end-to-end cross-check: C compiler → Z80 VM, for `(+ 3 4)`, `(* 6 7)`,
  `(- 10 3)`, `(if #t 1 2)`, `(if #f 1 2)`, `(let ...)`, `((lambda ...) ...)`

__Not yet proven on Z80__ (C VM handles these; Z80 handlers are stubs):

- `=`, `<`, `not`, `and`, `or` (comparison and boolean ops)
- `cons`, `car`, `cdr`, `list`, `pair?`, `null?`, `append` (list ops)
- `OP_TAILCALL` (tail recursion — implemented in C VM, Z80 stub)
- `OP_NEWLINE`, `OP_READ`, `OP_WRITE` (more I/O)



### Going further

The logical next steps, in order:

1. __OP\_EQ / OP\_LT / OP\_NOT__ — straightforward Z80 flag comparisons.
2. __OP\_TAILCALL__ — reuse the existing env frame for self-calls; the
   call-stack pop and IP redirect are already sketched in the subroutines.
3. __List ops__ — cons/car/cdr on the heap cells already defined in `lisp.c`.
4. __Full cross-check suite__ — run the Lisp test suite against the Z80 VM.
5. __ROM image__ — assemble the VM to a flat binary, load it into an ABC80 or
   ZX Spectrum emulator, and type Lisp at the keyboard.

The assembler (`z80asm.c`) and disassembler (`disasm.c`) are already present and
used by the test harness — no additional tooling needed for steps 1–4.

To retarget to a different machine, edit `vm_config.h` and swap in a new
PUTCHAR/GETCHAR subroutine string.



### File map

| File | Role |
|------|------|
| `lisp.c` | Lisp interpreter: parser, compiler, C VM, REPL |
| `test_vm.c` | Z80 proof: assembles VM, cross-checks C and Z80 results |
| `vm_config.h` | Machine-specific layout constants (addresses, I/O port) |
| `z80.c` / `z80.h` | Z80 emulator |
| `z80asm.c` / `z80asm.h` | Z80 assembler (used by test harness) |
| `disasm.c` / `disasm.h` | Z80 disassembler (used for debug dumps) |
| `z80mem.h` | Glue header — declares `z80_read_mem()` for disasm |
| `TRANSPILE.md` | Phase-by-phase design and implementation notes |
| `tests/run_tests.sh` | Full test suite (REPL + Z80 proof) |



### Known limitations

- __No garbage collection.__  Pools fill up over a long session; restart to reset.
- __No floating point.__  12-bit integer arithmetic with silent wrap.
- __No division.__  Z80 has no `DIV` instruction.
- __Integers are 12-bit signed.__  Maximum value: 2047.
- __No `define-syntax` or macros.__  Quasiquote covers the common case.
- __Single-line REPL input.__  Each line must be a complete s-expression.
