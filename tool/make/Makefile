CC=gcc
AR=ar
RM=rm -f

CFLAGS=-Wall -Wextra -Werror -std=c99 -g
LFLAGS=-Wl,--export-dynamic
XSHARED=-shared -fPIC -rdynamic

DEFINES=
INCLUDES=-I$(shell pwd)/src -I$(shell pwd)/include
LIBS=-lm -ldl

CLI_D=cli
SRC_D=src
STD_D=std

CLI_C=$(shell find $(CLI_D) -name '*.c')
SRC_C=$(shell find $(SRC_D) -name '*.c')
STD_C=$(shell find $(STD_D) -name '*.c')

CLI_O=$(CLI_C:.c=.o)
SRC_O=$(SRC_C:.c=.o)
STD_O=$(STD_C:.c=.o)

TARGET_BIN=bin/jml
TARGET_LIB=lib/libjml.a
TARGET_STD=$(STD_C:.c=.so)

all:: $(TARGET_BIN) $(TARGET_LIB)
all:: $(TARGET_STD)

$(CLI_D)/%.o: $(CLI_D)/%.c
	$(CC) -c $^ -o $@ $(CFLAGS) $(DEFINES) $(INCLUDES)

$(SRC_D)/%.o: $(SRC_D)/%.c
	$(CC) -c $^ -o $@ $(CFLAGS) $(DEFINES) $(INCLUDES)

$(STD_D)/%.o: $(STD_D)/%.c
	$(CC) -c $^ -o $@ $(CFLAGS) $(DEFINES) $(INCLUDES) $(XSHARED)

$(STD_D)/%.so: $(STD_D)/%.o
	$(CC) $< -o $@ $(CFLAGS) $(DEFINES) $(INCLUDES) $(XSHARED)

$(TARGET_BIN): $(SRC_O) $(CLI_O)
	$(CC) $^ -o $@ $(CFLAGS) $(LFLAGS) $(DEFINES) $(INCLUDES) $(LIBS)

$(TARGET_LIB): $(SRC_O)
	$(AR) rcs $@ $^

$(TARGET_STD): $(STD_O)

rmobj:
	$(RM) $(CLI_O) $(SRC_O) $(STD_O)

rmbin:
	$(RM) $(TARGET_BIN)

rmlib:
	$(RM) $(TARGET_LIB)

rmstd:
	$(RM) $(TARGET_STD)

.PHONY: clean
clean: rmobj rmbin rmlib rmstd
