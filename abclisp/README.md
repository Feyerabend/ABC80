
## abclisp

> *What if..*

The transition from "hand-crafted code" to "architectural oversight" has
fundamentally changed how we approach historical *counterfactuals*.
In the early 1980s, the microcomputer revolution was defined by the accessibility
of BASIC, leaving the sophisticated, symbolic power of Lisp machines locked
away in expensive research labs. While the Jupiter ACE proved that alternative
paradigms like Forth could fit into a Z80’s constrained memory, a consumer-grade
Lisp machine remained a bridge too far. It wasn't necessarily due to hardware limits,
but because the immense effort required to squeeze a high-level functional
language into 64KB of RAM was economically and mentally prohibitive for a
small team or a solo developer. Would it even gain traction? Who would
actually use it? Well, no one perhaps. But we could explore how it could
have looked like ..

Today, the barrier to entry has collapsed. Large Language Models
allow us to act as high-level architects, delegating the tedious, meticulous
work of instruction-level optimisation. Compiler scaffolding can be given to
an AI that can generate and iterate on Z80 assembly in seconds. What would
have been a grueling, multi-year research project just three years ago can now
be executed as a lean, rapid experiment. By using an LLM to navigate the
"historical anachronism" of bringing Lisp to the Z80, we can explore
the missing link of 1980s computing: a machine that treats code as data,
running on the silicon that started it all.



### Known limitations

To begin, we must acknowledge the inherent limitations. This is not merely
an exercise in pushing hardware to its absolute breaking point--though,
in a sense, it is—but rather a historical experiment in "what could have been."
We are exploring a counterfactual past, testing whether the symbolic power
of Lisp could have survived within the spartan constraints of 1980s
microcomputing if we had possessed the architectural tools we have today.

So:

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
