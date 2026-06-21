#ifndef KERNEL_TTY_CONTROL_H
#define KERNEL_TTY_CONTROL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TTY_CONTROL_MAX_PARAMS 8
#define TTY_CONTROL_PARAM_NONE UINT64_MAX

typedef struct {
  char command;
  uint64_t params[TTY_CONTROL_MAX_PARAMS];
  uint64_t param_count;
} tty_control_sequence_t;

void tty_control_reset(void);
void tty_control_write(const char* data, size_t len);

#endif
