DEFAULT_HOST!=../default_host.sh
HOST?=$(DEFAULT_HOST)
CC?=$(HOST)-gcc

CFLAGS?=-O2 -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector -fno-pie
CPPFLAGS?=
LDFLAGS?=-nostdlib -static -no-pie -Wl,-melf_x86_64 -Wl,-e,_start
LIBS?=-lgcc

BUILDDIR?=bin
SOURCES:=$(wildcard *.c)
TARGETS:=$(patsubst %.c,$(BUILDDIR)/%.elf,$(SOURCES))
CRT_SRC:=crt0.s
CRT_OBJ:=$(BUILDDIR)/crt0.o

.PHONY: all clean

all: $(TARGETS)

$(BUILDDIR):
	mkdir -p $@

$(CRT_OBJ): $(CRT_SRC) | $(BUILDDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/%.elf: %.c $(CRT_OBJ) | $(BUILDDIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(CRT_OBJ) -o $@ $(LDFLAGS) $(LIBS)

clean:
	rm -rf $(BUILDDIR)
