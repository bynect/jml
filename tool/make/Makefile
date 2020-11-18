CC=gcc
CFLAGS=-g -Wall -Wextra -Werror -std=c99
INCLUDES=-I$(shell pwd)/src -I$(shell pwd)/include
LIBS=-lm -ldl
SHARED=-shared -fPIC

%.o: src/%.c cli/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $^ $(LIBS) $(SHARED)

jml: *.o
	$(CC) $(CFLAGS) $(INCLUDES) -o bin/$@ $^ $(LIBS)

libjml.so: *.o
	$(CC) $(CFLAGS) $(INCLUDES) $(SHARED) -o lib/$@.so $^ $(LIBS)

libjml.a: *.o
	ar rcs lib/$@ $^

clean:
	rm -f ./*.o ./*.gch bin/jml lib/libjml.*
