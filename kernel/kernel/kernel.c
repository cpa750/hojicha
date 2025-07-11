#include <cpu/gdt.h>
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
  asm volatile("cli");  // Disable interrupts
  terminal_initialize();
  initialize_serial();
  printf("Serial initialized.\n");
  initialize_gdt();
  printf("GDT initialized.\n");
  serial_write_string("GDT initialized.\n");

  printf("Hojicha kernel initialized.\n");
  serial_write_string("Hojicha kernel initialized.\n");
  char buf[100];
  for (size_t i = 0; i < 100; i++) {
    itoa(i, buf, 16);
    serial_write_string(buf);
    serial_write_string("\n");
  }
  // asm volatile("sti");  // Enable interrupts
  // TODO: Re-enable this once we've implemented the IDT

  while (1) {
    asm("");
  }
}

