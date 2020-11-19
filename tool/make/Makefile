CC=gcc
CFLAGS=-Wall -Wextra -Werror -std=c99 -g

DEFINES=
INCLUDES=-I$(shell pwd)/src -I$(shell pwd)/include
LIBS=-lm -ldl

SHARED=-shared -fPIC
EXE=-Wl,--export-dynamic


all: jml libjml.a

%.o: src/%.c cli/%.c
	$(CC) -c $^ $(CFLAGS) $(DEFINES) $(INCLUDES) $(LIBS)

jml: *.o
	$(CC) -o bin/$@ $^ $(CFLAGS) $(DEFINES) $(INCLUDES) $(LIBS) $(EXE)

libjml.a: *.o
	ar rcs lib/$@ $^

.PHONY: clean
clean:
	rm -f *.o bin/jml lib/*
