CC      = gcc
CFLAGS  = -std=c11 -O2 -Wall -Wextra -Wpedantic
LIBS    = -lncurses

TARGET  = abc80
SRCS    = abc.c z80.c
OBJS    = $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Header dependencies
abc.o: abc.c abcprom.h z80.h
z80.o: z80.c z80.h

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: clean
