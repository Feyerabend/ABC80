CC			= gcc
CFLAGS		= -Wall
LDFLAGS		=
OBJFILES	= wav2bin.o bac2bas.o
TARGET		= wav2bin bac2bas

all: $(TARGET)

wav2bin: wav2bin.o
	$(CC) $(CFLAGS) -o wav2bin wav2bin.o $(LDFLAGS)

bac2bas: bac2bas.c
	$(CC) $(CFLAGS) -std=c89 -pedantic -O2 -o bac2bas bac2bas.c $(LDFLAGS)

clean:
	rm -f $(OBJFILES) $(TARGET) *~
