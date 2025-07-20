#include <drivers/keyboard.h>
#include <drivers/pic.h>
#include <io.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void initialize_keyboard() { return; }

void handle_keyboard() {
  char buf[100];
  uint8_t status = inb(0x64);
  if (status & 0x1) {
    uint8_t scancode = inb(0x60);
    itoa(scancode, buf, 16);
    printf("Got scancode: ");
    printf(buf);
    printf("\n");
  }
  send_end_of_interrupt(0x1);
}

