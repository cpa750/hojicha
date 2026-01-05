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
BUILDDIR=$(BUILDDIR_BASE)/$(HOSTARCH)/$(BUILD_TYPE)/
DESTDIR?=
PREFIX?=/usr/local
EXEC_PREFIX?=$(PREFIX)
BOOTDIR?=$(EXEC_PREFIX)/boot
INCLUDEDIR?=$(PREFIX)/include

CFLAGS:=$(CFLAGS) -ffreestanding -Wall -Wextra
CPPFLAGS:=$(CPPFLAGS) -D__is_kernel -Iinclude
LDFLAGS:=$(LDFLAGS)
LIBS:=$(LIBS) -nostdlib -lk -lgcc

ifeq ($(DEBUG_QEMU),1)
	CFLAGS :=$(CFLAGS) -g -D__debug_virtual
endif

ifeq ($(KMALLOC_TEST),1)
	CFLAGS :=$(CFLAGS) -D__kmalloc_test
endif

CFLAGS:=$(CFLAGS) \
					-std=gnu11 \
					-ffreestanding \
					-fno-stack-protector \
					-fno-stack-check \
					-fPIE \
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
					--sysroot=$(SYSROOT)

CPPFLAGS:=$(CPPFLAGS) \
					-DLIMINE_API_REVISION=3 \
					-MMD \
					-MP

LDFLAGS:=$(LDFLAGS) \
					-Wl,-melf_x86_64 \
					-Wl,-nostdlib \
					-Wl,-static \
					-Wl,-zmax-page-size=0x1000 \
					-Wl,--gc-sections \
					-Wl,--sysroot=$(SYSROOT) \

ARCHDIR=arch/$(HOSTARCH)

include $(ARCHDIR)/make.config

CFLAGS:=$(CFLAGS) $(KERNEL_ARCH_CFLAGS)
CPPFLAGS:=$(CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS)
LDFLAGS:=$(LDFLAGS) $(KERNEL_ARCH_LDFLAGS)
LIBS:=$(LIBS) $(KERNEL_ARCH_LIBS)

KERNEL_SRC=\
cpu/gdt.c \
cpu/idt.c \
cpu/irq.c \
cpu/isr.c \
drivers/keyboard.c \
drivers/pic.c \
drivers/pit.c \
drivers/tty.c \
drivers/serial.c \
fonts/inconsolata.c \
kernel/kernel.c \
kernel/kernel_state.c \
kernel/multitask.c \
memory/kmalloc.c \
memory/kmalloc_test.c \
memory/pmm.c \
memory/vmm.c \
utils/io.c \

CRT_BEGIN=\
$(ARCHDIR)/crti.o \
$(ARCHDIR)/crtbegin.o \

CRT_END=\
$(ARCHDIR)/crtend.o \
$(ARCHDIR)/crtn.o \

ARCH_SRC_C=$(KERNEL_ARCH_SRC_C:.c=.o)
ARCH_SRC_ASM=$(KERNEL_ARCH_SRC_ASM:.asm=.o)

ARCH_SRC=$(ARCH_SRC_ASM) $(ARCH_SRC_C)

OBJDIRS := $(sort $(dir $(addprefix $(BUILDDIR)/,$(KERNEL_SRC:.c=.o))))
OBJS:=$(addprefix $(BUILDDIR)/,$(CRT_BEGIN)) \
			$(addprefix $(BUILDDIR)/,$(KERNEL_SRC:.c=.o)) \
			$(addprefix $(BUILDDIR)/,$(ARCH_SRC)) \
			$(addprefix $(BUILDDIR)/,$(CRT_END))

LINK_LIST=\
$(LDFLAGS) \
$(OBJS) \
$(LIBS)

.PHONY: all clean install install-headers install-kernel
.SUFFIXES: .o .c .s .asm

all: $(BUILDDIR)/hojicha.kernel

$(BUILDDIR)/hojicha.kernel: $(OBJS) $(ARCHDIR)/linker.ld | $(BUILDDIR)
	$(CC) $(CFLAGS) -T $(ARCHDIR)/linker.ld -o $@ $(LINK_LIST)

$(OBJDIRS):
	mkdir -p $@

$(BUILDDIR)/$(ARCHDIR)/crtbegin.o $(BUILDDIR)/$(ARCHDIR)/crtend.o:
	@mkdir -p $(dir $@)
	OBJ=`$(CC) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$OBJ" $@

$(BUILDDIR)/%.o: %.c | $(OBJDIRS)
	@mkdir -p $(dir $@)
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

$(BUILDDIR)/%.o: %.s | $(OBJDIRS)
	@mkdir -p $(dir $@)
	$(CC) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

$(BUILDDIR)/%.o: %.asm | $(OBJDIRS)
	@mkdir -p $(dir $@)
	nasm -Wall -f elf64 -g $< -o $@

clean:
	rm -rf $(BUILDDIR_BASE)

install: install-headers install-kernel

install-headers:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	cp -R --preserve=timestamps include/. $(DESTDIR)$(INCLUDEDIR)/.

install-kernel: $(BUILDDIR)/hojicha.kernel
	mkdir -p $(DESTDIR)$(BOOTDIR)
	cp $(BUILDDIR)/hojicha.kernel $(DESTDIR)$(BOOTDIR)

-include $(OBJS:.o=.d)
