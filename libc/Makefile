$(info CC is $(CC))

DEFAULT_HOST!=../default_host.sh
HOST?=DEFAULT_HOST
HOSTARCH!=../target_triplet_to_arch.sh $(HOST)

CFLAGS?=-O3
CPPFLAGS?=
LDFLAGS?=
LIBS?=

BUILD_TYPE=release
ifeq ($(DEBUG_QEMU),1)
	BUILD_TYPE=debug
endif

BUILDDIR_BASE=bin
BUILDDIR=$(BUILDDIR_BASE)/$(HOSTARCH)/$(BUILD_TYPE)
DESTDIR?=
PREFIX?=/usr/local
EXEC_PREFIX?=$(PREFIX)
INCLUDEDIR?=$(PREFIX)/include
LIBDIR?=$(EXEC_PREFIX)/lib

ifeq ($(DEBUG_QEMU),1)
	CFLAGS := $(CFLAGS) -g
endif

CFLAGS := $(CFLAGS) \
					-std=gnu11 \
					-ffreestanding \
					-fno-stack-protector \
					-fno-stack-check \
					-fno-pie \
					-fno-pic \
					-m64 \
					-march=x86-64 \
					-mno-80387 \
					-mno-mmx \
					-mno-sse \
					-mno-sse2 \
					-fno-omit-frame-pointer \
					-mno-red-zone \
					-mcmodel=large \
					-Dh64 \
					--sysroot=$(SYSROOT) \
					-Wall \
					-Wextra

CPPFLAGS := $(CPPFLAGS) -D__is_libc -Iinclude
LIBK_CFLAGS := $(CFLAGS) -D__printf_serial
LIBK_CPPFLAGS := $(CPPFLAGS) -D__is_libk -D__printf_serial

ARCHDIR=arch/$(HOSTARCH)
include $(ARCHDIR)/make.config

CFLAGS := $(CFLAGS) $(ARCH_CFLAGS)
CPPFLAGS := $(CPPFLAGS) $(ARCH_CPPFLAGS)
LIBK_CFLAGS := $(LIBK_CFLAGS) $(KERNEL_ARCH_CFLAGS)
LIBK_CPPFLAGS := $(LIBK_CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS)

LIBC_SRC = \
stdio/printf.c \
stdio/putchar.c \
stdio/puts.c \
stdio/vprintf.c \
stdio/vsnprintf.c \
stdlib/abort.c \
stdlib/assert.c \
stdlib/free.c \
stdlib/itoa.c \
stdlib/utoa.c \
stdlib/malloc.c \
string/memcmp.c \
string/memmove.c \
string/memset.c \
string/strcpy.c \
string/strlen.c

# Build directory versions
OBJDIRS := $(sort $(dir $(addprefix $(BUILDDIR)/,$(LIBC_SRC:.c=.o))))

FREEOBJS   = $(addprefix $(BUILDDIR)/,$(LIBC_SRC:.c=.o)) \
						 $(addprefix $(BUILDDIR)/,$(ARCH_FREE_SRC_ASM:.asm=.o))
LIBK_OBJS  = $(FREEOBJS:.o=.libk.o)
LIBC_OBJS  = $(FREEOBJS:.o=.libc.o)

BINARIES = $(BUILDDIR)/libk.a

.PHONY: all clean install install-headers install-libs
.SUFFIXES: .o .libk.o .c .s .asm

all: $(BINARIES)

$(BUILDDIR)/libc.a: $(LIBC_OBJS) | $(BUILDDIR)
	$(AR) rcs $@ $(LIBC_OBJS)

$(BUILDDIR)/libk.a: $(LIBK_OBJS) | $(BUILDDIR)
	$(AR) rcs $@ $(LIBK_OBJS)

$(OBJDIRS):
	mkdir -p $@

$(BUILDDIR)/%.libc.o: %.c | $(OBJDIRS)
	@mkdir -p $(dir $@)
	$(CC) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

$(BUILDDIR)/%.libc.o: %.s | $(OBJDIRS)
	@mkdir -p $(dir $@)
	$(CC) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

$(BUILDDIR)/%.libc.o: %.asm | $(OBJDIRS)
	@mkdir -p $(dir $@)
	nasm -Wall -f elf64 -g $< -o $@

$(BUILDDIR)/%.libk.o: %.c | $(OBJDIRS)
	@mkdir -p $(dir $@)
	$(CC) -MD -c $< -o $@ $(LIBK_CFLAGS) $(LIBK_CPPFLAGS)

$(BUILDDIR)/%.libk.o: %.s | $(OBJDIRS)
	@mkdir -p $(dir $@)
	$(CC) -MD -c $< -o $@ $(LIBK_CFLAGS) $(LIBK_CPPFLAGS)

$(BUILDDIR)/%.libk.o: %.asm | $(OBJDIRS)
	@mkdir -p $(dir $@)
	nasm -Wall -f elf64 -g $< -o $@

clean:
	rm -rf $(BUILDDIR_BASE)

install: install-headers install-libs

install-headers:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	cp -R --preserve=timestamps include/. $(DESTDIR)$(INCLUDEDIR)/.

install-libs: $(BINARIES)
	mkdir -p $(DESTDIR)$(LIBDIR)
	cp $(BINARIES) $(DESTDIR)$(LIBDIR)

-include $(FREEOBJS:.o=.d)
-include $(LIBC_OBJS:.o=.d)
-include $(LIBK_OBJS:.o=.d)
