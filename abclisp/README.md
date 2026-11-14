
## abclisp

> *What if..*

The transition from "hand-crafted code" to "architectural oversight" has
fundamentally changed how we approach historical *counterfactuals*.
In the early 1980s, the microcomputer revolution was defined by the
accessibility of BASIC, leaving the sophisticated, symbolic power of
Lisp machines locked away in expensive research labs. While the Jupiter
ACE proved that alternative paradigms like Forth could fit into a
Z80's constrained memory, a consumer-grade Lisp machine remained a
bridge too far. It wasn't necessarily due to hardware limits, but
because the immense effort required to squeeze a high-level functional
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
on the kind of chip that started it all.


### What is here

Everything lives in [`02/`](02/):

- `lisp.c`: the Lisp interpreter in C (parser, bytecode compiler, VM, REPL).
- `test_vm.c`: the same VM rewritten in Z80 assembly. It is assembled and
  run in a Z80 emulator (`z80.c`, `z80asm.c`, `disasm.c`), and its results
  are checked against the C VM.
- [`02/README.md`](02/README.md): what is proven on the Z80 and what is not.
- [`02/LANGUAGE.md`](02/LANGUAGE.md): a guide to the language.
- [`02/TRANSPILE.md`](02/TRANSPILE.md): the step-by-step plan for Z80.

The Z80 code runs only in the emulator for now. There is no ROM image,
and no version for a real machine such as the ABC 80 or ABC 802.

Build and test (needs only a C11 compiler and make):

```sh
cd 02
make
make test
./lisp
```


### The Story

In the early 1970s, two young programmers--Bill Gates and Paul Allen--were
obsessed with a simple idea: putting real computing power into the hands of
individuals. At the time, computers were still massive, institutional machines.
But everything changed in 1975 with the arrival of the Altair 8800.

The Altair, based on the Intel 8080, had no real software ecosystem--just
switches and lights. Allen saw an opportunity immediately. He and Gates
decided to build a version of BASIC, a high-level language, that could run
on this tiny machine with extremely limited memory (as little as 4 KB).[^BASIC]

[^BASIC]: https://en.wikipedia.org/wiki/Microsoft_BASIC.

__The catch: they didn't have an Altair.__

Instead, Gates wrote most of the code on a PDP-10 mainframe at Harvard,
using an emulator that Allen built to simulate the 8080 CPU. This meant
they were effectively cross-developing--writing code for a processor
they couldn't directly test on real hardware. The BASIC interpreter
itself had to be incredibly compact. Every byte mattered. They hand-optimised
routines in assembly, carefully managing memory layout, tokenizing keywords,
and reusing buffers wherever possible.[^off]

[^off]: Monte Davidoff wrote the important floating-point math routines.

The core idea behind their implementation was a tokenized interpreter:
when a user typed a BASIC program, keywords like `PRINT` or `GOTO` were
converted into single-byte tokens. This saved memory and made execution
faster. The interpreter loop would then parse these tokens and dispatch
execution through a tightly packed set of routines.

Paul Allen flew to Albuquerque to demonstrate the system to Ed Roberts
at MITS. On the plane, Allen realised they hadn't written a "loader,"
the small bit of code to tell the Altair how to read the paper tape.
He wrote it by hand on a notepad during the flight.
Arriving, it was the first time their code would run on a real Altair.
Remarkably, it worked on the first try. The prompt appeared:

```
READY
```

That moment effectively launched Microsoft.

Their BASIC became the template for a whole generation of microcomputer
software. Variants of "Microsoft BASIC" were licensed and adapted for
systems based on chips like the MOS Technology 6502 and Zilog Z80,
spreading into machines like the Apple II, Commodore 64, and TRS-80.

Technically, what Gates and Allen built wasn't just a language--it was
a portable software architecture. By rewriting the hardware-dependent
parts (mainly I/O and startup code) and keeping the interpreter core
consistent, they created one of the first widely reused software
products in personal computing.

In a world of kilobytes, they proved something profound: software,
not hardware, would define the future.


### Known limitations

To begin, we must acknowledge the inherent limitations of our project.
This is not merely an exercise in pushing hardware to its absolute
breaking point--though, in a sense, it is--but rather a historical
experiment in "what could have been." We are exploring a counterfactual
past, testing whether the symbolic power of Lisp could have survived
within the spartan constraints of 1980s microcomputing if we had
possessed the architectural tools we have today. It might anyway not
have made it, but due to other concerns .. was there even a market?
Gates and Allen even produced halfway a (maybe even complete) APL
implementation. But it was deemed to not having much of a future.

The limitations of the interpreter itself are listed at the end of
[`02/README.md`](02/README.md) and [`02/LANGUAGE.md`](02/LANGUAGE.md).
