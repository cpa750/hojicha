#ifndef KERNEL_TTY_H
#define KERNEL_TTY_H

#include <stddef.h>
#include <stdint.h>

void terminal_initialize(void);
void terminal_putchar(char c);
void terminal_set_color(uint8_t color);
void terminal_write(const char* data, size_t len);
void terminal_write_string(const char* data);

#endif

