#include <cpu/gdt.h>
#include <kernel/tty.h>
#include <serial.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

void kernel_main(void) {
  terminal_initialize();
  initialize_serial();
  initialize_gdt();
  printf("Serial initialized.\n");
  serial_write_string("Hojicha kernel initialized.\n");
  printf("Hojicha kernel initialized.\n");
  char buf[100];
  for (size_t i = 0; i < 100; i++) {
    itoa(i, buf, 16);
    serial_write_string(buf);
    serial_write_string("\n");
  }
}

