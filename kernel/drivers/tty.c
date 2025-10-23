#include <drivers/tty.h>
#include <drivers/vga.h>
#include <fonts/inconsolata.h>
#include <io.h>
#include <kernel/kernel_state.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint16_t terminal_row;
static uint16_t terminal_column;
static uint8_t terminal_color;

static uint32_t* framebuffer;
static uint32_t* framebuffer_end;
static uint64_t vga_height;
static uint64_t vga_width;
static uint64_t vga_pitch;
static uint32_t* clear_start;

// TODO chage this once we can malloc again
static uint16_t terminal_buffer[5000];

struct tty_state {
  uint64_t height;
  uint64_t width;
  uint8_t color;
};
typedef struct tty_state tty_state_t;
static tty_state_t tty;

uint64_t tty_pos_to_vga_idx(uint16_t row, uint16_t col);

void terminal_initialize(void) {
  memset(&tty, 0, sizeof(tty_state_t));
  terminal_row = 0;
  terminal_column = 0;
  tty.height = vga_state_get_height(g_kernel.vga) / INCONSOLATA_HEIGHT;
  tty.width = vga_state_get_width(g_kernel.vga) / INCONSOLATA_WIDTH;
  terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  g_kernel.tty = &tty;

  uint32_t* framebuffer_end = vga_state_get_framebuffer_end(g_kernel.vga);

  framebuffer = vga_state_get_framebuffer_addr(g_kernel.vga);
  vga_height = vga_state_get_height(g_kernel.vga);
  vga_width = vga_state_get_width(g_kernel.vga);
  vga_pitch = vga_state_get_pitch(g_kernel.vga);
  clear_start = framebuffer_end - ((vga_pitch * INCONSOLATA_HEIGHT) >> 2);
}

void terminal_set_color(uint8_t color) { tty.color = color; }

void terminal_put_entry_at(unsigned char c, uint8_t color, size_t x, size_t y) {
  if (c == '\n') {
    return;
  }

  uint64_t fb_start_x = x * INCONSOLATA_WIDTH;
  uint64_t fb_start_y = y * INCONSOLATA_HEIGHT;
  vga_position_t pos = {fb_start_x, fb_start_y};
  vga_draw_bitmap_16h8w(&pos, inconsolata_bitmaps[c], 0xFFFFFF);
}

void scroll() {
  // TODO does a generalized version of this belong in the VGA driver?
  for (uint64_t i = 0; i < vga_width * (vga_height - INCONSOLATA_HEIGHT); ++i) {
    if (framebuffer[i + (vga_width * INCONSOLATA_HEIGHT)] != 0) {
      framebuffer[i] = framebuffer[i + (vga_width * INCONSOLATA_HEIGHT)];
    } else {
      framebuffer[i] = 0;
    }
  }
  memset(clear_start, 0, vga_pitch);
}

void terminal_erase() {
  // TODO when we have kerboard interrupt support
  if (terminal_column == 0 && terminal_row > 0) {
    terminal_column = tty.width;
    --terminal_row;
  }
  if (terminal_buffer[terminal_row * tty.width + terminal_column] != 0) {
    const size_t idx = terminal_row * tty.width + terminal_column;
    terminal_buffer[idx - 1] = vga_entry(' ', terminal_color);
  }
  if (terminal_column == 0 && terminal_row == 0) {
    return;
  }
  terminal_column--;
}

void terminal_set_caret_pos(uint16_t terminal_row, uint16_t terminal_column) {
  // TODO implement this with a framebuffer
  return;
}

void terminal_putchar(char c) {
  if (c == 0x08) {
    // terminal_erase();
    terminal_set_caret_pos(terminal_row, terminal_column);
    return;
  }

  if (c == '\n') {
    terminal_column = 0;
    if (++terminal_row == tty.height) {
      scroll();
      terminal_row--;
    }
    terminal_set_caret_pos(terminal_row, terminal_column);
    return;
  }

  if (c == '\t') {
    for (uint8_t i = terminal_column % 4; i < 4; ++i) {
      terminal_putchar(' ');
    }
    return;
  }

  terminal_put_entry_at(c, terminal_color, terminal_column, terminal_row);

  if (++terminal_column == tty.width) {
    terminal_column = 0;
    if (++terminal_row == tty.height) {
      scroll();
      terminal_row--;
    }
  }
  terminal_set_caret_pos(terminal_row, terminal_column);
}

void terminal_write(const char* data, size_t len) {
  size_t idx = 0;
  while (len--) {
    terminal_putchar(data[idx++]);
  }
}

vga_position_t tty_pos_to_vga_pos(uint16_t row, uint16_t col) {
  vga_position_t pos = {
      row * vga_state_get_pitch(g_kernel.vga) * INCONSOLATA_HEIGHT,
      col * INCONSOLATA_WIDTH};
  return pos;
}

