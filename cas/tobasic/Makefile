
CC			= gcc
CFLAGS		= -Wall
LDFLAGS		=
OBJFILES	= wav2bin.o
TARGET		= wav2bin

all: $(TARGET)

wav2bin: wav2bin.o
	$(CC) $(CFLAGS) -o wav2bin wav2bin.o $(LDFLAGS)

clean:
	rm -f $(OBJFILES) $(TARGET) *~
