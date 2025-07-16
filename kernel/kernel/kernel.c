#include <cpu/gdt.h>
#include <cpu/idt.h>
#include <cpu/protected.h>
#include <kernel/tty.h>
#include <serial.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

void kernel_main(void) {
  if (!is_protected_mode()) {
    printf(
        "Fatal: bootloader handed over with protected mode disabled. Abort.");
    abort();
  }
  asm volatile("cli");
  terminal_initialize();
  initialize_serial();
  printf("Serial initialized.\n");
  initialize_gdt();
  printf("GDT initialized.\n");
  initialize_idt();
  printf("IDT initialized.\n");

  printf("Hojicha kernel initialized.\n");
  serial_write_string("Hojicha kernel initialized.\n");

  asm volatile("sti");
  asm volatile("int $0x0");
}

