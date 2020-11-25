CC?=gcc
CFLAGS=-Wall -Wextra -Werror -std=c99 -g
LFLAGS=-Wl,--export-dynamic
SFLAGS=-shared -fPIC

DEFINES=
INCLUDES=-I$(shell pwd)/src -I$(shell pwd)/include
LIBS=-lm -ldl

all: jml libjml.a

%.o: src/%.c cli/%.c
	$(CC) -c $^ $(CFLAGS) $(DEFINES) $(INCLUDES) $(LIBS)

jml: *.o
	$(CC) -o bin/$@ $^ $(CFLAGS) $(LFLAGS) $(DEFINES) $(INCLUDES) $(LIBS)

libjml.a: *.o
	ar rcs lib/$@ $^

.PHONY: clean
clean:
	rm -f *.o bin/jml lib/*
