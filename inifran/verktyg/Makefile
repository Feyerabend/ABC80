# Verktygen till ABC80 inifrån: assemblern z80asm och emulatorn abc80.
#   make         bygg båda
#   make prov    assemblera exemplen och Forth och jämför med .bin-filerna
#   make clean   ta bort det byggda

CC     ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra

all: z80asm abc80

z80asm: z80asm.c
	$(CC) $(CFLAGS) -o $@ z80asm.c

abc80: abc80.c z80.c z80.h
	$(CC) $(CFLAGS) -o $@ abc80.c z80.c

PROGRAM = ../exempel/e1 ../exempel/e2 ../exempel/e3 ../exempel/e4a \
          ../exempel/e4b ../exempel/e5 ../exempel/e6 \
          ../forth/e7 ../forth/e7r ../forth/e7i

prov: z80asm abc80
	@mkdir -p prov; fel=0; \
	for p in $(PROGRAM); do \
	  n=$$(basename $$p); \
	  ./z80asm -o prov/$$n.bin $$p.asm || fel=1; \
	  if cmp -s prov/$$n.bin $$p.bin; then echo "lika   $$n"; \
	  else echo "OLIKA  $$n"; fel=1; fi; \
	done; \
	./abc80 -r ../disass/abc80/abc80new.rom -t 200 -k 'PRINT 6*7\r' > prov/abc80.ut; \
	if grep -q '^ 42$$' prov/abc80.ut; then echo "lika   abc80 (PRINT 6*7)"; \
	else echo "OLIKA  abc80"; cat prov/abc80.ut; fel=1; fi; \
	exit $$fel

clean:
	rm -rf z80asm abc80 prov

.PHONY: all prov clean
