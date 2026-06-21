#ifndef KERNEL_TTY_H
#define KERNEL_TTY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct tty_state;
typedef struct tty_state tty_state_t;

struct caret;
typedef struct caret caret_t;

typedef enum {
  TTY_MODE_CANONICAL = 1,
  TTY_MODE_RAW = 2,
} tty_mode_t;

typedef enum {
  TTY_CLEAR_TO_END = 0,
  TTY_CLEAR_TO_BEGINNING = 1,
  TTY_CLEAR_ALL = 2,
} tty_clear_mode_t;

enum {
  TTY_IOCTL_GET_MODE = 1,
  TTY_IOCTL_SET_MODE,
  TTY_IOCTL_GET_ECHO,
  TTY_IOCTL_SET_ECHO,
};

void terminal_initialize(void);
void terminal_putchar(char c);
uint32_t terminal_get_fg(void);
void terminal_set_fg(uint32_t fg);
void terminal_write(const char* data, size_t len);
uint64_t terminal_get_height(void);
uint64_t terminal_get_width(void);
void terminal_clear_display(tty_clear_mode_t mode);
void terminal_caret_disable(tty_state_t* t);
void terminal_caret_enable(tty_state_t* t);
void terminal_caret_reset(tty_state_t* t);
void terminal_caret_set_colour(tty_state_t* t, uint32_t colour);
void terminal_caret_set_pos(uint16_t row,
                            uint16_t col,
                            bool put_old_character_back);
void terminal_set_cursor_pos(uint16_t row, uint16_t col);

void tty_device_initialize(void);
bool tty_device_ready(void);
void tty_receive_char(char c);
void tty_set_echo(bool echo);
void tty_set_mode(tty_mode_t mode);
tty_mode_t tty_get_mode(void);

#endif
