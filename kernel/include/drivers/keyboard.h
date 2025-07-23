#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdbool.h>

typedef struct {
  bool shift_held;
  bool caps_enabled;
} KeyboardStatus;

void initialize_keyboard();
void handle_keyboard();

#endif  // KEYBOARD_H

