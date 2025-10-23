#ifndef KERNEL_TTY_H
#define KERNEL_TTY_H

#include <stddef.h>
#include <stdint.h>

struct tty_state;
typedef struct tty_state tty_state_h;

void terminal_initialize(void);
void terminal_putchar(char c);
void terminal_set_color(uint8_t color);
void terminal_write(const char* data, size_t len);

#endif

