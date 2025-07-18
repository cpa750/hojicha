#include <drivers/serial.h>
#include <io.h>
#include <string.h>

#define SERIAL_PORT 0x3f8

// TODO
int initialize_serial() {
  outb(SERIAL_PORT + 1, 0x00);
  outb(SERIAL_PORT + 3, 0x80);
  outb(SERIAL_PORT + 0, 0x03);
  outb(SERIAL_PORT + 1, 0x00);
  outb(SERIAL_PORT + 3, 0x03);
  outb(SERIAL_PORT + 2, 0xC7);
  outb(SERIAL_PORT + 4, 0x0B);
  outb(SERIAL_PORT + 4, 0x1E);
  outb(SERIAL_PORT + 0, 0xAE);

  if (inb(SERIAL_PORT + 0) != 0xAE) {
    return 1;
  }

  outb(SERIAL_PORT + 4, 0x0F);
  serial_write_string("Serial initialized.\n");
  return 0;
}

int is_serial_empty() { return inb(SERIAL_PORT + 5) & 0x20; }

void serial_write_char(const char c) {
  while (!is_serial_empty()) {
  }
  outb(SERIAL_PORT, c);
}

void serial_write_string(const char* c) {
  size_t len = strlen(c);
  for (size_t i = 0; i < len; i++) {
    serial_write_char(c[i]);
  }
}

