#ifndef KERNEL_TTY_H
#define KERNEL_TTY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct tty_state;
typedef struct tty_state tty_state_t;

struct caret;
typedef struct caret caret_t;

void terminal_initialize(void);
void terminal_putchar(char c);
void terminal_set_fg(uint32_t fg);
void terminal_write(const char* data, size_t len);
void terminal_caret_disable(tty_state_t* t);
void terminal_caret_enable(tty_state_t* t);
void terminal_caret_reset(tty_state_t* t);
void terminal_caret_set_colour(tty_state_t* t, uint32_t colour);
void terminal_caret_set_pos(uint16_t row, uint16_t col,
                            bool put_old_character_back);

#endif

