CC=gcc
CFLAGS=-Wall -g -I/usr/include/libftdi1/
LIBS=-L/usr/lib/ -lftdi1 -lusb-1.0

default: program
all: program

program: program.o
	gcc program.o -o program $(LIBS)
program.o: bitbang_ft2232.c
	gcc -c bitbang_ft2232.c -o program.o $(CFLAGS)

clean:
	rm -f program program.o
