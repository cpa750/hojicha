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
static uint32_t terminal_color;

static uint32_t* framebuffer;
static uint32_t* framebuffer_end;
static uint64_t vga_height;
static uint64_t vga_width;
static uint64_t vga_pitch;
static uint32_t* clear_start;

// TODO chage this once we can malloc again
static uint16_t terminal_buffer[5000];

static uint32_t under_caret[INCONSOLATA_WIDTH * INCONSOLATA_HEIGHT];

struct caret {
  uint32_t colour;
  uint64_t column;
  uint64_t row;
  uint32_t* bits_under;
  bool enabled;
};
typedef struct caret caret_t;

struct tty_state {
  uint64_t height;
  uint64_t width;
  uint32_t fg;
  uint32_t bg;
  caret_t* caret;
};
typedef struct tty_state tty_state_t;
static tty_state_t tty;
static caret_t caret;

uint64_t tty_pos_to_vga_idx(uint16_t row, uint16_t col);

void terminal_initialize(void) {
  memset(&tty, 0, sizeof(tty_state_t));
  memset(&caret, 0, sizeof(caret_t));
  terminal_row = 0;
  terminal_column = 0;
  tty.height = vga_state_get_height(g_kernel.vga) / INCONSOLATA_HEIGHT;
  tty.width = vga_state_get_width(g_kernel.vga) / INCONSOLATA_WIDTH;
  tty.fg = 0xFFFFFF;
  tty.bg = 0x0;
  caret.bits_under = under_caret;
  caret.column = 0;
  caret.row = 0;
  caret.enabled = true;
  caret.colour = tty.fg;
  tty.caret = &caret;
  g_kernel.tty = &tty;

  uint32_t* framebuffer_end = vga_state_get_framebuffer_end(g_kernel.vga);

  framebuffer = vga_state_get_framebuffer_addr(g_kernel.vga);
  vga_height = vga_state_get_height(g_kernel.vga);
  vga_width = vga_state_get_width(g_kernel.vga);
  vga_pitch = vga_state_get_pitch(g_kernel.vga);
  clear_start = framebuffer_end - ((vga_pitch * INCONSOLATA_HEIGHT) >> 2);
  terminal_caret_enable(&tty);
}

void terminal_set_fg(uint32_t fg) { g_kernel.tty->fg = fg; }

void terminal_put_entry_at(unsigned char c, uint32_t fg, size_t x, size_t y) {
  if (c == '\n') {
    return;
  }

  uint64_t fb_start_x = x * INCONSOLATA_WIDTH;
  uint64_t fb_start_y = y * INCONSOLATA_HEIGHT;
  vga_position_t pos = {fb_start_x, fb_start_y};
  vga_draw_bitmap_16h8w(&pos, inconsolata_bitmaps[c], fg);
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
  memset(clear_start, 0, vga_pitch * INCONSOLATA_HEIGHT);
}

void terminal_erase() {
  terminal_caret_disable(g_kernel.tty);

  if (terminal_column == 0 && terminal_row > 0) {
    terminal_column = g_kernel.tty->width;
    --terminal_row;
  }
  terminal_put_entry_at(' ', g_kernel.tty->fg, terminal_column - 1,
                        terminal_row);
  if (terminal_column == 0 && terminal_row == 0) {
    return;
  }
  terminal_column--;

  terminal_caret_reset(g_kernel.tty);
  terminal_caret_enable(g_kernel.tty);
  terminal_caret_set_pos(terminal_row, terminal_column, true);
}

void terminal_caret_set_pos(uint16_t row, uint16_t col,
                            bool put_old_character_back) {
  if (!g_kernel.tty->caret->enabled) {
    return;
  }
  vga_position_t top_left = {g_kernel.tty->caret->column * INCONSOLATA_WIDTH,
                             g_kernel.tty->caret->row * INCONSOLATA_HEIGHT};
  vga_position_t bottom_right = {
      g_kernel.tty->caret->column * INCONSOLATA_WIDTH + INCONSOLATA_WIDTH - 1,
      g_kernel.tty->caret->row * INCONSOLATA_HEIGHT + INCONSOLATA_HEIGHT - 1};
  if (put_old_character_back) {
    vga_copy_buffer_to_region(&top_left, &bottom_right,
                              g_kernel.tty->caret->bits_under);
  }

  g_kernel.tty->caret->column = col;
  g_kernel.tty->caret->row = row;

  vga_position_t new_top_left = {col * INCONSOLATA_WIDTH,
                                 row * INCONSOLATA_HEIGHT};
  vga_position_t new_bottom_right = {
      col * INCONSOLATA_WIDTH + INCONSOLATA_WIDTH - 1,
      row * INCONSOLATA_HEIGHT + INCONSOLATA_HEIGHT - 1};
  vga_copy_region_to_buffer(&new_top_left, &new_bottom_right,
                            g_kernel.tty->caret->bits_under);

  vga_draw_rect_solid(&new_top_left, &new_bottom_right,
                      g_kernel.tty->caret->colour);

  return;
}

void terminal_putchar(char c) {
  if (c == 0x08) {
    terminal_erase();
    return;
  }

  uint32_t caret_old_colour = g_kernel.tty->caret->colour;

  if (c == '\n') {
    terminal_column = 0;
    if (++terminal_row == g_kernel.tty->height) {
      terminal_caret_set_colour(g_kernel.tty, g_kernel.tty->bg);
      terminal_caret_reset(g_kernel.tty);
      scroll();
      terminal_row--;
      terminal_caret_set_colour(g_kernel.tty, caret_old_colour);
      terminal_caret_reset(g_kernel.tty);
    }
    terminal_caret_set_pos(terminal_row, terminal_column, true);
    return;
  }

  if (c == '\t') {
    for (uint8_t i = terminal_column % 4; i < 4; ++i) {
      terminal_putchar(' ');
    }
    return;
  }

  terminal_put_entry_at(c, g_kernel.tty->fg, terminal_column, terminal_row);

  if (++terminal_column == g_kernel.tty->width) {
    terminal_column = 0;
    if (++terminal_row == g_kernel.tty->height) {
      terminal_caret_set_colour(g_kernel.tty, g_kernel.tty->bg);
      terminal_caret_reset(g_kernel.tty);
      scroll();
      terminal_row--;
      terminal_caret_set_colour(g_kernel.tty, caret_old_colour);
      terminal_caret_reset(g_kernel.tty);
    }
  }
  terminal_caret_set_pos(terminal_row, terminal_column, false);
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

void terminal_caret_disable(tty_state_t* t) { t->caret->enabled = false; }
void terminal_caret_enable(tty_state_t* t) {
  vga_position_t top_left = {t->caret->column * INCONSOLATA_WIDTH,
                             t->caret->row * INCONSOLATA_HEIGHT};
  vga_position_t bottom_right = {
      t->caret->column * INCONSOLATA_WIDTH + INCONSOLATA_WIDTH - 1,
      t->caret->row * INCONSOLATA_HEIGHT + INCONSOLATA_HEIGHT - 1};
  terminal_caret_set_pos(t->caret->row, t->caret->column, false);
  t->caret->enabled = true;
}
void terminal_caret_reset(tty_state_t* t) {
  if (t->caret->enabled) {
    terminal_caret_enable(t);
  }
}
void terminal_caret_set_colour(tty_state_t* t, uint32_t colour) {
  t->caret->colour = colour;
}

