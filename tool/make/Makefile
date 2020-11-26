CC=gcc
CFLAGS=-Wall -Wextra -Werror -std=c99 -ggdb
LFLAGS=-Wl,--export-dynamic
XSHARED=-shared -fPIC -rdynamic

DEFINES=
INCLUDES=-I$(shell pwd)/src -I$(shell pwd)/include
LIBS=-lm -ldl

CLI_C=$(addprefix cli/,$(shell ls cli | grep '\.c'))
SRC_C=$(addprefix src/,$(shell ls src | grep '\.c'))
STD_C=$(addprefix std/,$(shell ls std | grep '\.c'))

CLI_O=$(CLI_C:.c=.o)
SRC_O=$(SRC_C:.c=.o)
STD_O=$(STD_C:.c=.o)

TARGET_BIN=bin/jml
TARGET_LIB=lib/libjml.a
TARGET_STD=$(STD_C:.c=.so)

all: $(TARGET_BIN) $(TARGET_LIB) $(TARGET_STD)

src/%.o: src/%.c
	$(CC) -c $^ -o $@ $(CFLAGS) $(DEFINES) $(INCLUDES) #$(LIBS)

cli/%.o: cli/%.c
	$(CC) -c $^ -o $@ $(CFLAGS) $(DEFINES) $(INCLUDES) #$(LIBS)

std/%.o: std/%.c
	$(CC) -c $^ -o $@ $(CFLAGS) $(DEFINES) $(INCLUDES) $(LIBS) $(XSHARED)

std/%.so: std/%.o
	$(CC) $< -o $@ $(CFLAGS) $(DEFINES) $(INCLUDES) $(LIBS) $(XSHARED)

$(TARGET_BIN): $(SRC_O) $(CLI_O)
	$(CC) $^ -o $@ $(CFLAGS) $(LFLAGS) $(DEFINES) $(INCLUDES) $(LIBS)

$(TARGET_LIB): $(SRC_O)
	ar rcs $@ $^

$(TARGET_STD): $(STD_O)

rmobj:
	rm -f $(CLI_O) $(SRC_O) $(STD_O)

rmbin:
	rm -f $(TARGET_BIN)

rmlib:
	rm -f $(TARGET_LIB) $(TARGET_STD)

.PHONY: clean
clean: rmobj rmbin rmlib
