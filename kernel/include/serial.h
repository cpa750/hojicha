#ifndef SERIAL_H
#define SERIAL_H
#include <stddef.h>

int initialize_serial();
int is_serial_empty();
void serial_write_char(const char c);
void serial_write_string(const char* c);

#endif  // SERIAL_H

