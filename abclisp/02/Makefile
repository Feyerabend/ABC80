# abclisp — build and test
#
# Targets:
#   all         build lisp, test_vm, z80asm  (default)
#   lisp        Lisp REPL / batch interpreter
#   test_vm     Z80 proof harness (single-TU: assembles and runs Z80 code)
#   z80asm      standalone Z80 assembler CLI
#   test        full test suite (REPL tests + Z80 proof)
#   test-lisp   REPL tests only  (does not require test_vm)
#   test-z80    Z80 proof only   (does not require lisp)
#   clean       remove built binaries

CC      ?= cc
CFLAGS  ?= -std=c11 -O2 -Wall

# macOS linker emits a harmless alignment warning for the large Z80 RAM array;
# silence it so test output stays readable.
UNAME := $(shell uname -s)
ifeq ($(UNAME), Darwin)
  LDFLAGS_TEST_VM := -Wl,-w
endif

.PHONY: all test test-lisp test-z80 clean

all: lisp test_vm z80asm

lisp: lisp.c
	$(CC) $(CFLAGS) -o $@ $<

test_vm: test_vm.c z80.c z80.h z80asm.c z80asm.h disasm.c disasm.h z80mem.h vm_config.h lisp.c
	$(CC) $(CFLAGS) -Wno-unused-function $(LDFLAGS_TEST_VM) -o $@ test_vm.c

z80asm: z80asm.c z80asm.h
	$(CC) $(CFLAGS) -o $@ $<

test: lisp test_vm
	bash tests/run_tests.sh

test-lisp: lisp
	LISP=./lisp TEST_VM=/bin/false bash tests/run_tests.sh

test-z80: test_vm
	./test_vm

clean:
	rm -f lisp test_vm z80asm
