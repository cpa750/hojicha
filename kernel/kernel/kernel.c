#include <kernel/tty.h>
#include <serial.h>
#include <stddef.h>
#include <stdio.h>

void kernel_main(void) {
  terminal_initialize();
  initialize_serial();
  serial_write_string("Kernel initialized\n");
  printf("Kernel initialized\n");
  for (size_t i = 0; i < 26; i++) {
    printf("i: %c\n", (char)i + 0x30);
  }
}

