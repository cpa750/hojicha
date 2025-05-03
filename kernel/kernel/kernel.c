#include <kernel/tty.h>
#include <stddef.h>
#include <stdio.h>

void kernel_main(void) {
  terminal_initialize();
  printf("Kernel initialized\n");
  for (size_t i = 0; i < 26; i++) {
    printf("i: %c\n", (char)i + 0x31);
  }
}

