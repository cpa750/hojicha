#include <drivers/keyboard.h>
#include <drivers/pic.h>
#include <drivers/serial.h>
#include <io.h>
#include <kernel/tty.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// TODO refactor this mess

KeyboardStatus keyboard_status;

char keyboard_characters[] = {
    '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '^',
    // TODO: handle backspace
    0, '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '@', '[', '\n',
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', ':', ']', 'z', 'x', 'c',
    'v', 'b', 'n', 'm', ',', '.', '/', ' '};

char shifted_keyboard_characters[] = {
    '!',  '\"', '#', '$', '%', '&', '\'', '(', ')', 0,   '=', '~', 0,
    '\t', 'Q',  'W', 'E', 'R', 'T', 'Y',  'U', 'I', 'O', 'P', '`', '{',
    '\n', 'A',  'S', 'D', 'F', 'G', 'H',  'J', 'K', 'L', '+', '*', '}',
    'Z',  'X',  'C', 'V', 'B', 'N', 'M',  '<', '>', '?', ' '};

void initialize_keyboard() {
  keyboard_status.shift_held = false;
  keyboard_status.caps_enabled = false;
}

char lookup_scancode_no_shift(uint8_t scancode) {
  if (scancode < 0x1E) {
    return keyboard_characters[scancode - 0x02];
  } else if (scancode < 0x2B) {
    return keyboard_characters[scancode - 0x03];
  } else if (scancode < 0x38) {
    return keyboard_characters[scancode - 0x05];
  } else if (scancode == 0x39) {
    return keyboard_characters[scancode - 0x07];
  } else {
    return 0;
  }
}

char lookup_scancode_with_shift(uint8_t scancode) {
  if (scancode < 0x1E) {
    return shifted_keyboard_characters[scancode - 0x02];
  } else if (scancode < 0x2B) {
    return shifted_keyboard_characters[scancode - 0x03];
  } else if (scancode < 0x38) {
    return shifted_keyboard_characters[scancode - 0x05];
  } else if (scancode == 0x39) {
    serial_write_string("Shifted char: ");
    serial_write_char(shifted_keyboard_characters[scancode - 0x07]);
    serial_write_string("\n");
    return shifted_keyboard_characters[scancode - 0x08];
  } else {
    return 0;
  }
}

char lookup_scancode(uint8_t scancode, KeyboardStatus* status) {
  if (status->shift_held) {
    return lookup_scancode_with_shift(scancode);
  }
  return lookup_scancode_no_shift(scancode);
}

void handle_scancode(uint8_t scancode) {
  char buf[100];
  itoa(scancode, buf, 16);
  serial_write_string("Got scancode: ");
  serial_write_string(buf);
  serial_write_string("\n");
  if (scancode == 0x2A) {
    keyboard_status.shift_held = true;
  } else if (scancode == 0xAA) {
    keyboard_status.shift_held = false;
  } else {
    char c = lookup_scancode(scancode, &keyboard_status);
    if (c != 0) terminal_putchar(c);
  }
}

void handle_keyboard() {
  // char buf[100];
  uint8_t status = inb(0x64);
  if (status & 0x1) {
    uint8_t scancode = inb(0x60);
    handle_scancode(scancode);
  }
  send_end_of_interrupt(0x1);
}

