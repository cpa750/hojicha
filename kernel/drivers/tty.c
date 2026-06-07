#include <drivers/tty.h>
#include <drivers/vga.h>
#include <fonts/inconsolata.h>
#include <fs/devfs.h>
#include <io.h>
#include <kernel/g_kernel.h>
#include <multitask/mutex.h>
#include <multitask/spinlock.h>
#include <multitask/wait_queue.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <utils/ringbuffer.h>
#include <utils/set_out.h>

#define TTY_INPUT_BUFFER_SIZE 1024
#define TTY_LINE_BUFFER_SIZE 256

static uint16_t terminal_row;
static uint16_t terminal_column;
static uint32_t terminal_color;

static uint32_t* framebuffer;
static uint64_t vga_height;
static uint64_t vga_width;
static uint64_t vga_pitch;

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
  bool device_initialized;
  bool echo;
  tty_mode_t mode;
  ringbuffer_t* input;
  mutex_t* input_lock;
  wait_queue_t read_waiters;
  char line_buffer[TTY_LINE_BUFFER_SIZE];
  uint64_t line_len;
  devfs_device_t* dev;
};
typedef struct tty_state tty_state_t;
static tty_state_t tty;
static caret_t caret;
static spinlock_t terminal_output_lock;

uint64_t tty_pos_to_vga_idx(uint16_t row, uint16_t col);
static uint64_t terminal_lock(void);
static void terminal_unlock(uint64_t irq_state);
static void terminal_putchar_unlocked(char c);
static void tty_buffer_input_char(tty_state_t* t, char c);
static void tty_emit_output(const char* data, uint64_t len);
static vfs_status_t tty_read(vfs_file_t* file,
                             void* buffer,
                             uint64_t len,
                             uint64_t* bytes_read_out);
static vfs_status_t tty_write(vfs_file_t* file,
                              void* buffer,
                              uint64_t len,
                              uint64_t* bytes_written_out);
static vfs_status_t tty_close(vfs_file_t* file);
static vfs_status_t tty_stat(vfs_node_t* vnode, vfs_stat_t** out);
static void tty_mutex_lock(void* lock);
static void tty_mutex_unlock(void* lock);

void terminal_initialize(void) {
  memset(&tty, 0, sizeof(tty_state_t));
  memset(&caret, 0, sizeof(caret_t));
  spinlock_init(&terminal_output_lock);
  terminal_row = 0;
  terminal_column = 0;
  tty.height = vga_state_get_height(g_kernel.vga) / INCONSOLATA_HEIGHT;
  tty.width = vga_state_get_width(g_kernel.vga) / INCONSOLATA_WIDTH;
  tty.fg = 0xFFFFFF;
  tty.bg = 0x0;
  tty.echo = true;
  tty.mode = TTY_MODE_CANONICAL;
  caret.bits_under = under_caret;
  caret.column = 0;
  caret.row = 0;
  caret.enabled = true;
  caret.colour = tty.fg;
  tty.caret = &caret;
  g_kernel.tty = &tty;

  framebuffer = vga_state_get_framebuffer_addr(g_kernel.vga);
  vga_height = vga_state_get_height(g_kernel.vga);
  vga_width = vga_state_get_width(g_kernel.vga);
  vga_pitch = vga_state_get_pitch(g_kernel.vga);
  terminal_caret_enable(&tty);
}

void tty_device_initialize(void) {
  if (g_kernel.tty == NULL || g_kernel.tty->device_initialized) { return; }

  vfs_file_ops_t* file_ops = calloc(1, sizeof(vfs_file_ops_t));
  vfs_node_ops_t* node_ops = calloc(1, sizeof(vfs_node_ops_t));
  mutex_t* lock = mutex_create();
  ringbuffer_t* input = NULL;
  if (lock != NULL) {
    ringbuffer_new(TTY_INPUT_BUFFER_SIZE,
                   &input,
                   lock,
                   tty_mutex_lock,
                   tty_mutex_unlock,
                   tty_mutex_lock,
                   tty_mutex_unlock);
  }

  if (file_ops == NULL || node_ops == NULL || lock == NULL || input == NULL) {
    free(file_ops);
    free(node_ops);
    if (input != NULL) { ringbuffer_free(input); }
    if (lock != NULL) { mutex_destroy(lock); }
    return;
  }

  file_ops->read = tty_read;
  file_ops->write = tty_write;
  file_ops->close = tty_close;
  node_ops->stat = tty_stat;

  devfs_device_t* dev = devfs_device_new(file_ops, node_ops);
  if (dev == NULL) {
    free(file_ops);
    free(node_ops);
    ringbuffer_free(input);
    mutex_destroy(lock);
    return;
  }

  if (devfs_register(DEVFS_CHARDEV, 3, dev, "tty0", 4) != VFS_STATUS_OK) {
    free(dev);
    free(file_ops);
    free(node_ops);
    ringbuffer_free(input);
    mutex_destroy(lock);
    return;
  }

  g_kernel.tty->input_lock = lock;
  g_kernel.tty->input = input;
  g_kernel.tty->dev = dev;
  g_kernel.tty->line_len = 0;
  wait_queue_init(&g_kernel.tty->read_waiters);
  g_kernel.tty->device_initialized = true;
}

bool tty_device_ready(void) {
  return g_kernel.tty != NULL && g_kernel.tty->device_initialized;
}

void tty_receive_char(char c) {
  tty_state_t* t = g_kernel.tty;
  if (t == NULL) { return; }

  if (!t->device_initialized) {
    terminal_putchar(c);
    return;
  }

  if (t->mode == TTY_MODE_RAW) {
    if (t->echo) { tty_emit_output(&c, 1); }
    tty_buffer_input_char(t, c);
    wait_queue_wake_all(&t->read_waiters);
    return;
  }

  if (c == 0x08) {
    if (t->line_len > 0) {
      t->line_len--;
      if (t->echo) { tty_emit_output(&c, 1); }
    }
    return;
  }

  if (t->line_len < TTY_LINE_BUFFER_SIZE) {
    t->line_buffer[t->line_len++] = c;
    if (t->echo) { tty_emit_output(&c, 1); }
  }

  if (c == '\n' || t->line_len == TTY_LINE_BUFFER_SIZE) {
    for (uint64_t i = 0; i < t->line_len; ++i) {
      tty_buffer_input_char(t, t->line_buffer[i]);
    }
    t->line_len = 0;
    wait_queue_wake_all(&t->read_waiters);
  }
}

void tty_set_echo(bool echo) {
  if (g_kernel.tty != NULL) { g_kernel.tty->echo = echo; }
}

void tty_set_mode(tty_mode_t mode) {
  if (g_kernel.tty == NULL) { return; }
  if (mode != TTY_MODE_CANONICAL && mode != TTY_MODE_RAW) { return; }

  g_kernel.tty->mode = mode;
  g_kernel.tty->line_len = 0;
}

tty_mode_t tty_get_mode(void) {
  if (g_kernel.tty == NULL) { return TTY_MODE_CANONICAL; }
  return g_kernel.tty->mode;
}

uint32_t terminal_get_fg(void) {
  uint64_t irq_state = terminal_lock();
  uint32_t fg = g_kernel.tty->fg;
  terminal_unlock(irq_state);
  return fg;
}
void terminal_set_fg(uint32_t fg) {
  uint64_t irq_state = terminal_lock();
  g_kernel.tty->fg = fg;
  terminal_unlock(irq_state);
}

void terminal_put_entry_at(unsigned char c, uint32_t fg, size_t x, size_t y) {
  if (c == '\n') { return; }

  uint64_t fb_start_x = x * INCONSOLATA_WIDTH;
  uint64_t fb_start_y = y * INCONSOLATA_HEIGHT;
  vga_position_t pos = {fb_start_x, fb_start_y};
  vga_draw_bitmap_16h8w(&pos, inconsolata_bitmaps[c], fg);
}

void scroll() {
  uint64_t row_stride = vga_pitch / sizeof(uint32_t);
  uint64_t text_scanlines = g_kernel.tty->height * INCONSOLATA_HEIGHT;
  uint64_t scroll_scanlines = INCONSOLATA_HEIGHT;

  if (text_scanlines <= scroll_scanlines) { return; }

  uint64_t text_rows = g_kernel.tty->height;
  for (uint64_t row = 0; row < text_rows - 1; ++row) {
    uint32_t* dst = framebuffer + (row * scroll_scanlines * row_stride);
    uint32_t* src = framebuffer + ((row + 1) * scroll_scanlines * row_stride);
    memcpy(dst, src, scroll_scanlines * vga_pitch);
  }

  uint32_t* clear_start =
      framebuffer + ((text_scanlines - scroll_scanlines) * row_stride);
  memset(clear_start, 0, scroll_scanlines * vga_pitch);
}

void terminal_erase() {
  terminal_caret_disable(g_kernel.tty);

  if (terminal_column == 0 && terminal_row > 0) {
    terminal_column = g_kernel.tty->width;
    --terminal_row;
  }
  terminal_put_entry_at(
      ' ', g_kernel.tty->fg, terminal_column - 1, terminal_row);
  if (terminal_column == 0 && terminal_row == 0) { return; }
  terminal_column--;

  terminal_caret_reset(g_kernel.tty);
  terminal_caret_enable(g_kernel.tty);
  terminal_caret_set_pos(terminal_row, terminal_column, true);
}

void terminal_caret_set_pos(uint16_t row,
                            uint16_t col,
                            bool put_old_character_back) {
  if (!g_kernel.tty->caret->enabled) { return; }
  vga_position_t top_left = {g_kernel.tty->caret->column * INCONSOLATA_WIDTH,
                             g_kernel.tty->caret->row * INCONSOLATA_HEIGHT};
  vga_position_t bottom_right = {
      g_kernel.tty->caret->column * INCONSOLATA_WIDTH + INCONSOLATA_WIDTH - 1,
      g_kernel.tty->caret->row * INCONSOLATA_HEIGHT + INCONSOLATA_HEIGHT - 1};
  if (put_old_character_back) {
    vga_copy_buffer_to_region(
        &top_left, &bottom_right, g_kernel.tty->caret->bits_under);
  }

  g_kernel.tty->caret->column = col;
  g_kernel.tty->caret->row = row;

  vga_position_t new_top_left = {col * INCONSOLATA_WIDTH,
                                 row * INCONSOLATA_HEIGHT};
  vga_position_t new_bottom_right = {
      col * INCONSOLATA_WIDTH + INCONSOLATA_WIDTH - 1,
      row * INCONSOLATA_HEIGHT + INCONSOLATA_HEIGHT - 1};
  vga_copy_region_to_buffer(
      &new_top_left, &new_bottom_right, g_kernel.tty->caret->bits_under);

  vga_draw_rect_solid(
      &new_top_left, &new_bottom_right, g_kernel.tty->caret->colour);

  return;
}

void terminal_putchar(char c) {
  uint64_t irq_state = terminal_lock();
  terminal_putchar_unlocked(c);
  terminal_unlock(irq_state);
}

static void terminal_putchar_unlocked(char c) {
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
      terminal_putchar_unlocked(' ');
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
    }
  }
  terminal_caret_set_pos(terminal_row, terminal_column, false);
}

void terminal_write(const char* data, size_t len) {
  uint64_t irq_state = terminal_lock();
  size_t idx = 0;
  while (len--) { terminal_putchar_unlocked(data[idx++]); }
  terminal_unlock(irq_state);
}

static vfs_status_t tty_read(vfs_file_t* file,
                             void* buffer,
                             uint64_t len,
                             uint64_t* bytes_read_out) {
  (void)file;
  if (buffer == NULL) {
    SET_OUT(bytes_read_out, 0);
    return VFS_STATUS_INVALID_ARG;
  }
  if (!tty_device_ready()) {
    SET_OUT(bytes_read_out, 0);
    return VFS_STATUS_NOT_IMPLEMENTED;
  }

  char* out = (char*)buffer;
  uint64_t bytes_read = 0;
  while (bytes_read < len) {
    char c = 0;
    sched_postpone();
    if (ringbuffer_read(g_kernel.tty->input, &c)) {
      sched_resume();
      out[bytes_read++] = c;
      if (g_kernel.tty->mode == TTY_MODE_CANONICAL && c == '\n') { break; }
      continue;
    }

    if (bytes_read > 0) {
      sched_resume();
      break;
    }
    wait_queue_sleep(&g_kernel.tty->read_waiters);
    sched_resume();
  }

  SET_OUT(bytes_read_out, bytes_read);
  return VFS_STATUS_OK;
}

static vfs_status_t tty_write(vfs_file_t* file,
                              void* buffer,
                              uint64_t len,
                              uint64_t* bytes_written_out) {
  (void)file;
  if (buffer == NULL) {
    SET_OUT(bytes_written_out, 0);
    return VFS_STATUS_INVALID_ARG;
  }

  tty_emit_output((const char*)buffer, len);
  SET_OUT(bytes_written_out, len);
  return VFS_STATUS_OK;
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
  if (t->caret->enabled) { terminal_caret_enable(t); }
}
void terminal_caret_set_colour(tty_state_t* t, uint32_t colour) {
  t->caret->colour = colour;
}

static void tty_buffer_input_char(tty_state_t* t, char c) {
  ringbuffer_write(t->input, c);
}

static void tty_emit_output(const char* data, uint64_t len) {
  terminal_write(data, len);
}

static uint64_t terminal_lock(void) {
  return spinlock_lock_irqsave(&terminal_output_lock);
}

static void terminal_unlock(uint64_t irq_state) {
  spinlock_unlock_irqrestore(&terminal_output_lock, irq_state);
}

static vfs_status_t tty_close(vfs_file_t* file) {
  if (file == NULL) { return VFS_STATUS_INVALID_ARG; }

  file->fs_data = NULL;
  return VFS_STATUS_OK;
}

static vfs_status_t tty_stat(vfs_node_t* vnode, vfs_stat_t** out) {
  if (vnode == NULL || out == NULL) { return VFS_STATUS_INVALID_ARG; }

  vfs_stat_t* stat = calloc(1, sizeof(vfs_stat_t));
  if (stat == NULL) { return VFS_STATUS_NOMEM; }

  stat->type = VFS_NODE_DEVICE;
  stat->size = 0;
  SET_OUT(out, stat);
  return VFS_STATUS_OK;
}

static void tty_mutex_lock(void* lock) { mutex_lock((mutex_t*)lock); }

static void tty_mutex_unlock(void* lock) { mutex_unlock((mutex_t*)lock); }
