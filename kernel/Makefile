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

CFLAGS:=$(CFLAGS) -ffreestanding -Wall -Wextra -Dh32
CPPFLAGS:=$(CPPFLAGS) -D__is_kernel -Iinclude
LDFLAGS:=$(LDFLAGS)
LIBS:=$(LIBS) -nostdlib -lk -lgcc

ifeq ($(DEBUG_QEMU),1)
	CFLAGS :=$(CFLAGS) -g -D__debug_virtual
endif
ifeq ($(H_64),1)
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
						-mno-red-zone
	CPPFLAGS:=$(CPPFLAGS) \
						-DLIMINE_API_REVISION=3 \
						-MMD \
						-MP
	LDFLAGS:=$(CPPFLAGS) \
						-m elf_x86_64 \
						-nostdlib \
						-static \
						-z max-page-size=0x1000 \
						--gc-sections \
						-T linker.ld
endif

ARCHDIR=arch/$(HOSTARCH)

include $(ARCHDIR)/make.config

CFLAGS:=$(CFLAGS) $(KERNEL_ARCH_CFLAGS)
CPPFLAGS:=$(CPPFLAGS) $(KERNEL_ARCH_CPPFLAGS)
LDFLAGS:=$(LDFLAGS) $(KERNEL_ARCH_LDFLAGS)
LIBS:=$(LIBS) $(KERNEL_ARCH_LIBS)

KERNEL_OBJS=\
$(KERNEL_ARCH_OBJS) \
cpu/gdt.o \
cpu/isr.o \
cpu/irq.o \
cpu/idt.o \
drivers/keyboard.o \
drivers/pic.o \
drivers/pit.o \
drivers/serial.o \
kernel/kernel_state.o \
kernel/kernel.o \
memory/kmalloc.o \
memory/pmm.o \
memory/vmm.o \
utils/io.o \

#OBJS=\
#$(ARCHDIR)/crti.o \
#$(ARCHDIR)/crtbegin.o \
#$(KERNEL_OBJS) \
#$(ARCHDIR)/crtend.o \
#$(ARCHDIR)/crtn.o \

OBJS=\
$(KERNEL_OBJS) \

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
	$(CC) -T $(ARCHDIR)/linker.ld -o $@ $(CFLAGS) $(LINK_LIST)
	grub2-file --is-x86-multiboot hojicha.kernel

$(ARCHDIR)/crtbegin.o $(ARCHDIR)/crtend.o:
	OBJ=`$(CC) $(CFLAGS) $(LDFLAGS) -print-file-name=$(@F)` && cp "$$OBJ" $@

.c.o:
	$(CC) -MD -c $< -o $@ -std=gnu11 $(CFLAGS) $(CPPFLAGS)

.s.o:
	$(CC) -MD -c $< -o $@ $(CFLAGS) $(CPPFLAGS)

.asm.o:
	nasm -Wall -f elf32 $< -o $@

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
