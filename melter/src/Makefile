CFLAGS?=-DDEBUG -g
CFLAGS+=-Wall -ansi -pedantic
CFLAGS+=-D_FILE_OFFSET_BITS=64
CFLAGS+=-I../lib/i386/usr/include/ -I../lib/i386/usr/include/i386-linux-gnu/

LDFLAGS=-L../lib/i386/usr/lib/i386-linux-gnu/
#LDFLAGS+=-static

LDLIBS=-lcrypto



all: main

main: main.c bf_debmelter.o

clean:
	rm -f main *.o
