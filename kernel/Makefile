DEFAULT_HOST!=../default_host.sh
HOST?=DEFAULT_HOST
HOSTARCH!=../target_triplet_to_arch.sh $(HOST)

CFLAGS?=-O3
CPPFLAGS?=
LDFLAGS?=
LIBS?=

DESTDIR?=
PREFIX?=/usr/local
EXEC_PREFIX?=$(PREFIX)
BOOTDIR?=$(EXEC_PREFIX)/boot
INCLUDEDIR?=$(PREFIX)/include

CFLAGS:=$(CFLAGS) -ffreestanding -Wall -Wextra
CPPFLAGS:=$(CPPFLAGS) -D__is_kernel -Iinclude
LDFLAGS:=$(LDFLAGS)
LIBS:=$(LIBS) -nostdlib -lk -lgcc
#LIBS:=$(LIBS) -L$(SYSROOT)/usr/lib -nostdlib -lk -lgcc

ifeq ($(DEBUG_QEMU),1)
	CFLAGS :=$(CFLAGS) -g -D__debug_virtual
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

KERNEL_OBJS=\
$(KERNEL_ARCH_OBJS) \
drivers/keyboard.o \
drivers/tty.o \
drivers/serial.o \
fonts/inconsolata.o \
kernel/kernel.o \
kernel/kernel_state.o \
utils/io.o \

OBJS=\
$(ARCHDIR)/crti.o \
$(ARCHDIR)/crtbegin.o \
$(KERNEL_OBJS) \
$(ARCHDIR)/crtend.o \
$(ARCHDIR)/crtn.o \

LINK_LIST=\
$(LDFLAGS) \
$(ARCHDIR)/crti.o \
$(ARCHDIR)/crtbegin.o \
$(KERNEL_OBJS) \
$(LIBS) \
$(ARCHDIR)/crtend.o \
$(ARCHDIR)/crtn.o \

.PHONY: all clean install install-headers install-kernel
.SUFFIXES: .o .c .s .asm

all: hojicha.kernel

hojicha.kernel: $(OBJS) $(ARCHDIR)/linker.ld
	$(CC) $(CFLAGS) -T $(ARCHDIR)/linker.ld -o $@ $(LINK_LIST)

$(ARCHDIR)/crtbegin.o $(ARCHDIR)/crtend.o:
	OBJ=`$(CC) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$OBJ" $@

.c.o:
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

.s.o:
	$(CC) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

.asm.o:
	nasm -Wall -f elf64 $< -o $@

clean:
	rm -f hojicha.kernel
	rm -f $(OBJS) *.o */*.o */*/*.o
	rm -f $(OBJS:.o=.d) *.d */*.d */*/*.d

install: install-headers install-kernel

install-headers:
	mkdir -p $(DESTDIR)$(INCLUDEDIR)
	cp -R --preserve=timestamps include/. $(DESTDIR)$(INCLUDEDIR)/.

install-kernel: hojicha.kernel
	mkdir -p $(DESTDIR)$(BOOTDIR)
	cp hojicha.kernel $(DESTDIR)$(BOOTDIR)

-include $(OBJS:.o=.d)
