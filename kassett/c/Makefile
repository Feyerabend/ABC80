# kassettverktygen i C89
CC ?= cc
CFLAGS ?= -std=c89 -pedantic -Wall -Wextra -O2
PROG = kas kas2wav wav2kas bac2bas abctext

all: $(PROG)

kas: kas.o abckas.o
	$(CC) $(CFLAGS) -o $@ kas.o abckas.o -lm
kas2wav: kas2wav.o abckas.o
	$(CC) $(CFLAGS) -o $@ kas2wav.o abckas.o -lm
wav2kas: wav2kas.o abckas.o
	$(CC) $(CFLAGS) -o $@ wav2kas.o abckas.o -lm

bac2bas: bac2bas.o abckas.o
	$(CC) $(CFLAGS) -o $@ bac2bas.o abckas.o -lm
abctext: abctext.o abckas.o
	$(CC) $(CFLAGS) -o $@ abctext.o abckas.o -lm

kas.o kas2wav.o wav2kas.o bac2bas.o abctext.o abckas.o: abckas.h
bac2bas.o: rom802.h

prov: all
	sh prov.sh

clean:
	rm -f $(PROG) *.o

.PHONY: all prov clean
