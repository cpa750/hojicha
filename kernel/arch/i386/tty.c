#include <io.h>
#include <kernel/tty.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "vga.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t* const VGA_MEMORY = (uint16_t*)0xB8000;

static uint16_t terminal_row;
static uint16_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer;

void set_cursor_shape(uint8_t start, uint8_t end) {
  outb(0x3d4, 0x0a);
  outb(0x3d5, start);

  outb(0x3d4, 0x0b);
  outb(0x3d5, end);
}

void terminal_initialize(void) {
  terminal_row = 0;
  terminal_column = 0;
  terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  terminal_buffer = VGA_MEMORY;
  for (size_t y = 0; y < VGA_HEIGHT; y++) {
    for (size_t x = 0; x < VGA_WIDTH; x++) {
      const size_t idx = y * VGA_WIDTH + x;
      terminal_buffer[idx] = vga_entry(' ', terminal_color);
    }
  }
  set_cursor_shape(0, 15);
}

void terminal_set_color(uint8_t color) { terminal_color = color; }

void terminal_put_entry_at(unsigned char c, uint8_t color, size_t x, size_t y) {
  if (c == '\n') { return; }

  const size_t idx = y * VGA_WIDTH + x;
  terminal_buffer[idx] = vga_entry(c, color);
}

void scroll() {
  for (int y = 1; y < VGA_HEIGHT; y++) {
    for (int x = 0; x < VGA_WIDTH; x++) {
      terminal_buffer[(y - 1) * VGA_WIDTH + x] =
          terminal_buffer[y * VGA_WIDTH + x];
    }
  }

  for (int x = 0; x < VGA_WIDTH; x++) {
    terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] =
        vga_entry(' ', terminal_color);
  }
}

void delete_last_line() {
  int x, *ptr;

  for (x = 0; x < VGA_WIDTH * 2; x++) {
    ptr = (int*)(0xB8000 + (VGA_WIDTH * 2) * (VGA_HEIGHT - 1) + x);
    *ptr = 0;
  }
}

void terminal_erase() {
  if (terminal_column == 0 && terminal_row > 0) {
    terminal_column = VGA_WIDTH;
    --terminal_row;
  }
  if (terminal_buffer[terminal_row * VGA_WIDTH + terminal_column] != 0) {
    const size_t idx = terminal_row * VGA_WIDTH + terminal_column;
    terminal_buffer[idx - 1] = vga_entry(' ', terminal_color);
  }
  if (terminal_column == 0 && terminal_row == 0) { return; }
  terminal_column--;
}

void terminal_set_caret_pos(uint16_t terminal_row, uint16_t terminal_column) {
  uint16_t idx = terminal_row * VGA_WIDTH + terminal_column;
  outb(0x3d4, 0x0f);
  outb(0x3d5, (uint8_t)(idx & 0xff));

  outb(0x3d4, 0x0e);
  outb(0x3d5, (uint8_t)((idx >> 8) & 0xff));
}

void terminal_putchar(char c) {
  if (c == 0x08) {
    terminal_erase();
    terminal_set_caret_pos(terminal_row, terminal_column);
    return;
  }

  if (c == '\n') {
    terminal_column = 0;
    if (++terminal_row == VGA_HEIGHT) {
      scroll();
      terminal_row--;
    }
    terminal_set_caret_pos(terminal_row, terminal_column);
    return;
  }

  if (c == '\t') {
    for (uint8_t i = terminal_column % 4; i < 4; ++i) { terminal_putchar(' '); }
    return;
  }

  terminal_put_entry_at(c, terminal_color, terminal_column, terminal_row);

  if (++terminal_column == VGA_WIDTH) {
    terminal_column = 0;
    if (++terminal_row == VGA_HEIGHT) {
      scroll();
      terminal_row--;
    }
  }
  terminal_set_caret_pos(terminal_row, terminal_column);
}

void terminal_write(const char* data, size_t len) {
  size_t idx = 0;
  while (len--) { terminal_putchar(data[idx++]); }
}
