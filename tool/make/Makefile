#usage
#make -f tool/make/Makefile all [options]


CLI_D=ext/cli
DIS_D=ext/dis
SRC_D=src
STD_D=std
BIN_D=bin
LIB_D=lib

CLI_C:=$(shell find $(CLI_D) -name '*.c')
DIS_C:=$(shell find $(DIS_D) -name '*.c')
SRC_C:=$(shell find $(SRC_D) -name '*.c')
STD_C:=$(shell find $(STD_D) -name '*.c')

CLI_O=$(CLI_C:.c=.o)
DIS_O=$(DIS_C:.c=.o)
SRC_O=$(SRC_C:.c=.o)
STD_O=$(STD_C:.c=.o)

TARGET_BIN=$(BIN_D)/jml
TARGET_DIS=$(BIN_D)/jbc
TARGET_LIB=$(LIB_D)/libjml.a
TARGET_SO=$(TARGET_LIB:.a=.so)
TARGET_STD=$(STD_C:.c=.so)


CC=gcc
AR=ar
RM=rm -f
MKD=mkdir -p

CFLAGS=-Wall -Wextra -Werror
LFLAGS=-rdynamic

XSHARED=-shared -fpic
LSHARED=

DEFINES=
INCLUDES=-I$(shell pwd)/include
LIBS=-lm -ldl

AUX_SCRIPT=tool/make/aux.py


ifneq (,$(filter $(strip $(CC)),g++ clang++))
	CFLAGS+=-fpermissive -Wno-write-strings
else
	CFLAGS+=-std=c99
endif

ifneq (,$(filter $(strip $(CC)),clang clang++))
	CFLAGS+=-Wno-typedef-redefinition
endif

ifdef NDEBUG
	DEFINES+=-DJML_NDEBUG
	CFLAGS+=-O3 -fno-semantic-interposition
else
	ifdef ASAN
		CFLAGS+=-g3 -fsanitize=address
	else
		CFLAGS+=-g3
	endif
endif

ifdef READLINE
	DEFINES+=-DJML_CLI_READLINE
	LIBS+=-lreadline
endif

ifdef LINK_STD
	LSHARED+=-Llib -ljml
endif

ifdef PROFILE
	CFLAGS+=-pg
endif


all: prelude

all: $(TARGET_BIN) $(TARGET_DIS) $(TARGET_LIB) $(TARGET_SO)

all: $(TARGET_STD)


install: all
	cp $(TARGET_BIN) /usr/local/bin/
	cp $(TARGET_DIS) /usr/local/bin/

prelude:
	$(MKD) $(BIN_D) $(LIB_D)


$(CLI_D)/%.o: $(CLI_D)/%.c
	$(CC) -c $^ -o $@ $(CFLAGS) $(DEFINES) $(INCLUDES)

$(DIS_D)/%.o: $(DIS_D)/%.c
	$(CC) -c $^ -o $@ $(CFLAGS) $(DEFINES) $(INCLUDES)

$(SRC_D)/%.o: $(SRC_D)/%.c
	$(CC) -c $^ -o $@ $(CFLAGS) $(DEFINES) $(INCLUDES) -fpic

$(STD_D)/%.o: $(STD_D)/%.c
	$(CC) -c $^ -o $@ $(CFLAGS) $(DEFINES) $(INCLUDES) -fpic

$(STD_D)/%.so: $(STD_D)/%.o $(TARGET_SO)
	$(CC) $< -o $@ $(CFLAGS) $(XSHARED) $(LSHARED) $(shell ./$(AUX_SCRIPT) $@)

$(TARGET_BIN): $(SRC_O) $(CLI_O)
	$(CC) $^ -o $@ $(CFLAGS) $(LFLAGS) $(INCLUDES) $(LIBS)
	$(info compiling executable)

$(TARGET_DIS): $(SRC_O) $(DIS_O)
	$(CC) $^ -o $@ $(CFLAGS) $(LFLAGS) $(INCLUDES) $(LIBS)
	$(info compiling disassembler)

$(TARGET_LIB): $(SRC_O)
	$(AR) rcs $@ $^
	$(info compiling static library)

$(TARGET_SO): $(SRC_O)
	$(CC) $^ -o $@ $(XSHARED)
	$(info compiling shared library)

$(TARGET_STD): $(STD_O)


.PHONY: rmobj
rmobj:
	$(RM) $(CLI_O) $(DIS_O) $(SRC_O) $(STD_O)

.PHONY: rmbin
rmbin:
	$(RM) $(TARGET_BIN)

.PHONY: rmdis
rmdis:
	$(RM) $(TARGET_DIS)

.PHONY: rmlib
rmlib:
	$(RM) $(TARGET_LIB) $(TARGET_SO)

.PHONY: rmstd
rmstd:
	$(RM) $(TARGET_STD)

.PHONY: clean
clean: rmobj rmbin rmdis rmlib rmstd
