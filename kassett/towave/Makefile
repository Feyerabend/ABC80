
CC			= gcc
CFLAGS		= -Wall
LDFLAGS		=
OBJFILES	= abc2wav.o
TARGET		= abc2wav

all: $(TARGET)

abc2wav: abc2wav.o
	$(CC) $(CFLAGS) -o abc2wav abc2wav.o $(LDFLAGS)

clean:
	rm -f $(OBJFILES) $(TARGET) *~
