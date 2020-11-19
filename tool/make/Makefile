CC=gcc
CFLAGS=-g -Wall -Wextra -Werror -std=c99
INCLUDES=-I$(shell pwd)/src -I$(shell pwd)/include
LIBS=-lm -ldl
SHARED=-shared -fPIC
DEFINE=-DJML_NDEBUG

%.o: src/%.c cli/%.c
	$(CC) $(CFLAGS) $(INCLUDES) $(SHARED) -c $^ $(LIBS)

jml: *.o
	$(CC) $(CFLAGS) $(INCLUDES) -o bin/$@ $^ $(LIBS)

libjml.so: *.o
	$(CC) $(CFLAGS) $(INCLUDES) $(SHARED) -o lib/$@ $^ $(LIBS)

libjml.a: *.o
	ar rcs lib/$@ $^

all: jml libjml.so libjml.a

.PHONY: clean
clean:
	rm -f *.o bin/jml lib/libjml.*
