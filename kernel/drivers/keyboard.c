#include <drivers/keyboard.h>
#include <drivers/pic.h>
#include <io.h>
#include <kernel/tty.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

char keyboard_characters[] = {
    // Offset 0x02
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '^',
    // TODO: handle backspace
    '\0', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '@', '[',
    '\n',
    // Offset 0x1e
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', ':',
    // Offset 0x2b
    ']', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    // Offset 0x38
    ' '};

void initialize_keyboard() { return; }

// TODO:
char lookup_scancode(uint8_t scancode) {
  if (scancode < 0x1e) {
    return keyboard_characters[scancode - 0x02];
  } else if (scancode < 0x2b) {
    return keyboard_characters[scancode - 0x1e];
  } else if (scancode < 0x38) {
    return keyboard_characters[scancode - 0x2b];
  } else if (scancode == 0x38) {
    return keyboard_characters[scancode];
  } else {
    return '\0';
  }
}

void handle_keyboard() {
  // char buf[100];
  uint8_t status = inb(0x64);
  if (status & 0x1) {
    uint8_t scancode = inb(0x60);
    char character = lookup_scancode(scancode);
    terminal_putchar(character);
    // itoa(scancode, buf, 16);
    // printf("Got scancode: ");
    // printf(buf);
    // printf("\n");
  }
  send_end_of_interrupt(0x1);
}

