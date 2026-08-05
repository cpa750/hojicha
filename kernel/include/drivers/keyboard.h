#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdbool.h>
#include <stdint.h>

#define KEYBOARD_EVENT_BUFFER_SIZE 256

typedef struct keyboard_status keyboard_status_t;
struct keyboard_status {
  bool shift_held;
  bool caps_enabled;
};

typedef enum {
  KEYBOARD_KEY_DOWN,
  KEYBOARD_KEY_UP,
} keyboard_event_direction_t;

typedef struct keyboard_event keyboard_event_t;
struct keyboard_event {
  uint8_t scancode;
  keyboard_event_direction_t direction;
  uint64_t timestamp;
  uint64_t sequence;
};

void keyboard_initialize();
void handle_keyboard();
bool keyboard_read_event(keyboard_event_t* out);
bool keyboard_read_event_wait(keyboard_event_t* out);

#endif  // KEYBOARD_H
