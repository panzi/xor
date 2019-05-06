CC=gcc
PREFIX=/usr/local
BINEXT=
TARGET=$(shell uname|tr '[A-Z]' '[a-z]')$(shell getconf LONG_BIT)
BUILDDIR=build
BUILDDIR_BIN=$(BUILDDIR)/$(TARGET)
COMMON_CFLAGS=-Wall -Werror -Wextra -std=gnu11
ifeq ($(DEBUG),ON)
	COMMON_CFLAGS+=-g -DDEBUG
else
	COMMON_CFLAGS+=-O2
endif
POSIX_CFLAGS=$(COMMON_CFLAGS) -pedantic -fdiagnostics-color
CFLAGS=$(COMMON_CFLAGS)
ARCH_FLAGS=

OBJ=$(BUILDDIR_BIN)/main.o

ifeq ($(TARGET),win32)
	CC=i686-w64-mingw32-gcc
	ARCH_FLAGS=-m32
	BINEXT=.exe
else
ifeq ($(TARGET),win64)
	CC=x86_64-w64-mingw32-gcc
	ARCH_FLAGS=-m64
	BINEXT=.exe
else
ifeq ($(TARGET),linux32)
	CFLAGS=$(POSIX_CFLAGS)
	ARCH_FLAGS=-m32
else
ifeq ($(TARGET),linux64)
	CFLAGS=$(POSIX_CFLAGS)
	ARCH_FLAGS=-m64
endif
endif
endif
endif

.PHONY: all install uninstall clean xor setup

all: xor

xor: $(BUILDDIR_BIN)/xor$(BINEXT)

setup:
	mkdir -p $(BUILDDIR_BIN)

install: $(BUILDDIR_BIN)/xor$(BINEXT)
	install $< $(PREFIX)/bin/xor$(BINEXT)

uninstall:
	rm $(PREFIX)/bin/xor$(BINEXT)

$(BUILDDIR_BIN)/%.o: src/%.c
	$(CC) $(ARCH_FLAGS) $(CFLAGS) -c $< -o $@

$(BUILDDIR_BIN)/xor$(BINEXT): $(OBJ)
	$(CC) $(ARCH_FLAGS) $(OBJ) -o $@

clean:
	rm -f $(BUILDDIR_BIN)/xor$(BINEXT) $(OBJ)
