#include <kernel/tty.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "vga.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t* const VGA_MEMORY = (uint16_t*)0xB8000;

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer;

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
}

void terminal_set_color(uint8_t color) { terminal_color = color; }

void terminal_put_entry_at(unsigned char c, uint8_t color, size_t x, size_t y) {
  if (c == '\n') {
    return;
  }

  const size_t idx = y * VGA_WIDTH + x;
  terminal_buffer[idx] = vga_entry(c, color);
}

void scroll(int line) {
  int loop;
  char c;

  for (loop = line * (VGA_WIDTH * 2) + 0xB8000; loop < VGA_WIDTH * 2; loop++) {
    c = *((char*)loop);
    *((char*)loop - (VGA_WIDTH * 2)) = c;
  }
}

void delete_last_line() {
  int x, *ptr;

  for (x = 0; x < VGA_WIDTH * 2; x++) {
    ptr = (int*)(0xB8000 + (VGA_WIDTH * 2) * (VGA_HEIGHT - 1) + x);
    *ptr = 0;
  }
}

void terminal_putchar(char c) {
  int line;
  unsigned char uc = c;

  terminal_put_entry_at(uc, terminal_color, terminal_column, terminal_row);
  // TODO fix terminal scrolling
  if (++terminal_column == VGA_WIDTH || c == '\n') {
    terminal_column = 0;
    if (++terminal_row == VGA_HEIGHT) {
      for (line = 1; line <= VGA_HEIGHT - 1; line++) {
        scroll(line);
      }
      delete_last_line();
      terminal_row = VGA_HEIGHT - 1;
    }
  }
}

void terminal_write(const char* data, size_t len) {
  size_t idx = 0;
  while (len--) {
    terminal_putchar(data[idx++]);
  }
}

