
## abclisp

> *What if..*

So we start with a small Lisp/Scheme interpreter written in C, designed
from the ground up to be transpilable to Z80 assembly.  It illustrates
the route an 80s home computer manufacturer might have taken to put a
real Lisp in ROM.

```
> (define (fact n) (if (= n 0) 1 (* n (fact (- n 1)))))
> (fact 6)
720
> `(the answer is ,(fact 6))
(the answer is 720)
```



### Building and running

```sh
cc -std=c11 -O2 -o lisp lisp.c
./lisp
```

No dependencies beyond a C11 compiler.  Type expressions at the `>` prompt.
Press Ctrl-D to quit.

```sh
## Batch mode (one complete s-expression per line)
printf '(define (square n) (* n n))\n(square 7)\n' | ./lisp
```

Run the test suite:

```sh
bash tests/run_tests.sh
```



### The language

abclisp is a Scheme-flavoured Lisp.  So it might not be period correct,
and ba a historical anachronism.  Every expression returns a value.
The REPL prints non-nil results automatically.

#### Literals

| Form | Type | Example |
|------|------|---------|
| `42`, `-7` | Integer (12-bit signed, −2048..2047) | `(+ 40 2)` --> `42` |
| `#t`, `#f` | Boolean | `(not #f)` --> `#t` |
| `nil`, `()` | Empty list / false-ish | `(null? nil)` --> `#t` |
| `#\A`, `#\space`, `#\newline` | Character | `(char->int #\A)` --> `65` |
| `"hello"` | String (up to 15 chars) | `(str-len "hello")` --> `5` |
| `(quote x)`, `'x` | Quoted symbol/list | `'(a b c)` --> `(a b c)` |

Integers wrap silently at 12 bits — `(+ 2047 1)` gives `-2048`.
This is deliberate: it mirrors Z80 register-pair arithmetic.
Yeah, this wouldn't be practical, but we are exploring hypothesis here.

#### Defining things

```scheme
(define x 42)                        ; variable
(define (square n) (* n n))          ; function (shorthand)
(define square (lambda (n) (* n n))) ; same thing explicitly
(set! x 100)                         ; update existing binding
```

#### Arithmetic and comparison

```scheme
(+ a b)   (- a b)   (* a b)          ; arithmetic (no division — Z80 has none)
(= a b)   (< a b)                    ; comparison --> #t or #f
(not x)                              ; logical not  (#f and nil are falsy)
```

#### Conditionals

```scheme
(if test then)
(if test then else)

(cond ((= x 1) "one")
      ((= x 2) "two")
      (else    "other"))
```

`and` and `or` short-circuit:

```scheme
(and #t expr)   ; evaluates expr only if first is true
(or  #f expr)   ; evaluates expr only if first is false
```

#### Functions and closures

```scheme
(lambda (x y) (+ x y))              ; anonymous function

(define (make-adder n)              ; returns a closure
  (lambda (x) (+ x n)))

(define add5 (make-adder 5))
(add5 10)                           ; --> 15
```

Each `lambda` instantiation gets its own captured environment, so multiple
closures from the same definition are independent.

#### Let and letrec

```scheme
(let ((x 3) (y 4)) (+ x y))         ; --> 7  (bindings evaluated in outer env)

(letrec ((fact (lambda (n)          ; self-reference works
           (if (= n 0) 1 (* n (fact (- n 1)))))))
  (fact 6))                         ; --> 720
```

`letrec` pre-binds all names to `nil` before evaluating the inits, so
mutually recursive functions work:

```scheme
(letrec ((even? (lambda (n) (if (= n 0) #t (odd?  (- n 1)))))
         (odd?  (lambda (n) (if (= n 0) #f (even? (- n 1))))))
  (even? 10))                       ; --> #t
```

#### Tail-call optimisation

Tail calls do not grow the call stack.  Self-tail-calls reuse the existing
environment frame--zero allocation, true iteration:

```scheme
(define (count n) (if (= n 0) 'done (count (- n 1))))
(count 500)                         ; --> done  (stack depth stays at 1)

(define (sum n acc)
  (if (= n 0) acc (sum (- n 1) (+ acc n))))
(sum 100 0)                         ; --> 954  (= 5050 mod 2^12)
```

#### Lists

```scheme
(cons 1 2)                          ; --> (1 2)  — a cell
(car (list 1 2 3))                  ; --> 1
(cdr (list 1 2 3))                  ; --> (2 3)
(pair? (list 1))                    ; --> #t
(null? nil)                         ; --> #t
(list 1 2 3)                        ; --> (1 2 3)
(append (list 1 2) (list 3 4))      ; --> (1 2 3 4)
(apply f (list a b))                ; call f with a b as arguments
```

Lists are represented as flat cells (up to 6 elements each), not
traditional cons chains.  `car`/`cdr` work as expected; `cdr` of a 3+
element list allocates a new shorter cell.

#### Characters

```scheme
(char? #\A)          ; --> #t
(char->int #\A)      ; --> 65
(int->char 65)       ; --> #\A  (use display to print as 'A')
(display #\A)        ; prints:  A
```

#### Strings

```scheme
(str-len "hello")           ; --> 5
(str-ref "hello" 1)         ; --> #\e
(str-append "foo" "bar")    ; --> "foobar"
(str=? "abc" "abc")         ; --> #t
(num->str 42)               ; --> "42"
(str->num "123")            ; --> 123
(sym->str 'hello)           ; --> "hello"
(str->sym "world")          ; --> world  (symbol)
(string? "hi")              ; --> #t
```

Strings are pooled--maximum 15 characters, 32 slots total.

#### Quasiquote

``` `` ` `` `` ` `` is quasiquote; `,` is unquote — expand at parse time:

```scheme
(define x 42)
`(the answer is ,x)                ; --> (the answer is 42)

(define (greet name)
  `(hello ,name you-are ,(str-len (sym->str name)) chars))
(greet 'alice)                     ; --> (hello alice you-are 5 chars)
```

#### I/O

```scheme
(display x)     ; print without quoting — strings appear bare, chars as char
(write x)       ; print in read-back form — strings quoted, chars as #\X
(newline)       ; print a newline
(read)          ; read one expression from stdin, return it as a value
(error n)       ; print E:N and halt the current expression
```

`display` and `write` return `nil`; the REPL suppresses nil results, so
side-effecting calls print cleanly.

#### Begin

```scheme
(begin expr1 expr2 ... exprN)      ; evaluate in sequence, return last
```


### Sample programs

#### Fibonacci

```scheme
(define (fib n)
  (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2)))))
(fib 10)                           ; --> 55
```

#### Map (manual, tail-recursive accumulator)

```scheme
(define (my-map f lst)
  (if (null? lst)
      nil
      (cons (f (car lst)) (my-map f (cdr lst)))))
(my-map (lambda (x) (* x x)) (list 1 2 3 4 5))
; --> (1 4 9 16 25)
```

#### Simple object via closure

```scheme
(define (make-point x y)
  (lambda (msg)
    (cond ((= msg 0) x)
          ((= msg 1) y)
          (else nil))))

(define p (make-point 3 4))
(list (p 0) (p 1))                 ; --> (3 4)
```

#### Data-driven program with read

```scheme
(define n (read))      ; waits for input
; user types: 7
(define (fact n) (if (= n 0) 1 (* n (fact (- n 1)))))
(fact n)               ; --> 5040
```



### Architecture

```
source text
    |
    v  parse_expr()
  AST (heap cells — Val words in flat Cell arrays)
    │
    v  compile()
  bytecode  ops[uint8_t]  +  args[uint16_t]
    │
    v  vm_run() — trampoline dispatch
  result Val
```

#### Values — Val (uint16_t)

Every Lisp value fits in 16 bits — one Z80 register pair:

```
 15 14 13 12 | 11 10  9  8  7  6  5  4  3  2  1  0
 +-----------+------------------------------------+
 |    tag    |              payload (12 bits)     |
 +-----------+------------------------------------+
```

| Tag | Type | Payload               |
|-----|------|-----------------------|
| 0   | NIL  | —                     |
| 1   | INT  | 12-bit signed integer |
| 2   | SYM  | index into syms[]     |
| 3   | PAIR | index into heap[]     |
| 4   | FUN  | index into funs[]     |
| 5   | BOOL | 0=#f, 1=#t            |
| 6   | CHAR | ASCII code (7-bit)    |
| 7   | STR  | index into strs[]     |

Extracting tag and payload on Z80:

```z80
; HL = Val
LD  A, H
RRCA \ RRCA \ RRCA \ RRCA   ; A = tag (upper nibble --> lower nibble)
AND  0x0F                   ; A = tag

LD  A, H
AND  0x0F                   ; A = high 4 bits of payload
; L = low 8 bits of payload
```

#### Instruction encoding

Instructions are stored in two parallel arrays:

```c
uint8_t  ops[255];    // opcode byte — one Z80 register indexes this
uint16_t args[255];   // operand word — loaded as a register pair
```

`ip` is `uint8_t` — the program counter fits in a single Z80 register.
`ncode` is also `uint8_t`; the code buffer holds up to 254 instructions
per session (the REPL accumulates code as definitions are entered).

#### Trampoline dispatch

The VM loop:

```c
while (!vm_halt && vm_ip < ncode) {
    vm_arg = args[vm_ip];
    optbl[ops[vm_ip++]]();
}
```

`optbl[]` is an array of function pointers — one per opcode.  On Z80 this
maps to a JP-table: load opcode into a register, index the table, `JP (HL)`.
No switch, no comparison chain — one jump to the handler.

```z80
fetch:
  LD   A, (DE)       ; fetch opcode (DE = ip pointer into ops[])
  INC  DE
  LD   L, A
  LD   H, 0
  ADD  HL, HL        ; × 2  (each table entry = 2-byte address)
  LD   BC, optbl
  ADD  HL, BC
  LD   A, (HL)
  INC  HL
  LD   H, (HL)
  LD   L, A
  JP   (HL)          ; dispatch — handler returns to fetch with JP fetch
```

#### Environment model

Environments are linked frames in a fixed pool:

```c
typedef struct {
    uint8_t key[6];   // symbol indices
    Val     val[6];   // corresponding values
    uint8_t n;        // number of bindings
    uint8_t parent;   // parent frame index (0xFF = none)
} Env;
```

Lookup walks the parent chain comparing `uint8_t` symbol indices — on Z80
this is a `CP` instruction per slot, no string comparison.

#### Tail calls

`OP_TAILCALL` pops a new call frame instead of pushing one.  For self-calls
(`fid == cur_fid`) it rebinds arguments in the existing env frame — zero
allocation, infinite recursion via iteration.

#### Closure instantiation

`OP_MKCLOS` is executed at runtime each time a `lambda` form is evaluated.
It allocates a fresh slot in `funs[]` copied from the compiled template, then
captures `cur_env`:

```
template funs[0]  (code in ROM)   -->  instance funs[1]  (env captured at call time)
                                  -->  instance funs[2]  (different env, different closure)
```

This means two calls to `(make-adder 5)` and `(make-adder 10)` produce two
independent closures that do not interfere.



### Memory map (RAM)

All pools are fixed-size global arrays — no `malloc`, no heap fragmentation:

| Pool         | Size      | Notes                         |
|--------------|-----------|-------------------------------|
| `ops[255]`   | 255 B     | Bytecode opcode stream        |
| `args[255]`  | 510 B     | Parallel operand stream       |
| `syms[64]`   | 768 B     | Symbol name table (12 B each) |
| `strs[32]`   | 512 B     | String pool (16 B each)       |
| `heap[64]`   | 832 B     | Lisp cells (6 vals × 2 B + 1) |
| `envs[32]`   | 640 B     | Environment frames            |
| `funs[48]`   | 336 B     | Function/closure records      |
| `stk[32]`    | 64 B      | Value stack                   |
| `cs[16]`     | 48 B      | Call frame stack              |
| misc         | ~150 B    | read_buf, counters, globals   |
| *Total*      | *~4165 B* | *~4 KB of 64 KB used*         |

The remaining ~60 KB is available for user-defined data and program storage.



### Error and warning codes

| Code  | Meaning                   |
|-------|---------------------------|
| `E:1` | Syntax / unexpected token |
| `E:2` | Unbound variable          |
| `E:3` | Type error                |
| `E:4` | Stack overflow            |
| `E:5` | Out of memory             |
| `E:6` | Wrong number of arguments |
| `W:1` | Heap cells low            |
| `W:2` | Env frames low            |
| `W:3` | Code slots low            |
| `W:4` | Symbol table low          |
| `W:5` | String pool low           |

`(error N)` raises a user-level error with code N, prints `E:N`, and halts
the current expression.



### Expanding the interpreter

Adding a new primitive takes three steps:

*1. Add an opcode* to the enum in the bytecode section:

```c
enum {
    ...
    OP_MY_OP    /* new opcode */
};
```

*2. Write a handler* (one `void` function, all state via globals):

```c
static void op_my_op(void) {
    Val a = POP();
    /* ... */
    PUSH(result);
}
```

*3. Register it* in the dispatch table (must match enum order):

```c
static const OpFn optbl[] = {
    ..., op_my_op
};
```

Then wire it into the compiler the same way `display` is — either as a
`UNOP`/`BINOP` macro entry, or as an explicit `if (IS_KW(head, K.xxx))` case.

The K struct pre-interns all keyword names at startup (`k_init()`), so the
compiler dispatches via a single `uint8_t` index comparison — one `CP`
instruction on Z80.



### Known limitations

- *No garbage collection.*  Pools fill up over a long session.  The REPL
  prints `W:N` warnings when a pool is near capacity.  Restart to reset.

- *No floating point.*  Z80 has no FPU; implementing floats in software
  would consume ~2 KB of ROM and ~1 KB of RAM — too expensive for a 64 KB
  system.  Integer arithmetic with 12-bit wrap is the intended model.

- *No division.*  Z80 has no `DIV` instruction.  Add it yourself as
  `OP_DIV` using a subtraction loop if needed.

- *No proper tail calls for cross-function calls via `apply`.*  `apply`
  always pushes a call frame.

- *Integers are 12-bit signed.*  Maximum value: 2047.  Values above this
  wrap silently.  This is a deliberate Z80 trade-off, not a bug.

- *No `define-syntax` or macros.*  Quasiquote covers the common case.

- *Single-line REPL input.*  Each line must be a complete s-expression.
  This mirrors the 80s BASIC model where you type one statement per line.

- *Closure pool exhaustion.*  Each `lambda` instantiation allocates a slot
  in `funs[]`.  With `MAX_FUN=48` you can create up to ~30 runtime closures
  (the rest are compiler templates).  A future `gc_funs()` pass could reclaim
  unreachable slots.
