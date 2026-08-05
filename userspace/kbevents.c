#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

typedef enum {
  KEYBOARD_KEY_DOWN,
  KEYBOARD_KEY_UP,
} keyboard_event_direction_t;

typedef struct keyboard_event {
  uint8_t scancode;
  keyboard_event_direction_t direction;
  uint64_t timestamp;
  uint64_t sequence;
} keyboard_event_t;

static const char* direction_name(keyboard_event_direction_t direction) {
  return direction == KEYBOARD_KEY_UP ? "up" : "down";
}

int main(void) {
  int fd = open("/dev/kb", O_RDONLY, 0);
  if (fd < 0) {
    printf("kbevents: cannot open /dev/kb: %d\n", errno);
    return 1;
  }

  while (1) {
    keyboard_event_t event;
    int bytes_read = read(fd, &event, sizeof(event));
    if (bytes_read < 0) {
      printf("kbevents: read failed: %d\n", errno);
      close(fd);
      return 1;
    }
    if (bytes_read != sizeof(event)) { continue; }

    printf("seq=%d time=%d scancode=%x direction=%s\n",
           event.sequence,
           event.timestamp,
           event.scancode,
           direction_name(event.direction));
  }
}
