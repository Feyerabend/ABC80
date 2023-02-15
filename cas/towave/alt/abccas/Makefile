CC      = gcc
CFLAGS  = -O2


default: abccas abckonv

abccas: abccas.o 
	${CC} ${CFLAGS} -o abccas abccas.o

abckonv: abckonv.o 
	${CC} ${CFLAGS} -o abckonv abckonv.o

abccas.o: abccas.c

abckonv.o: abckonv.c
