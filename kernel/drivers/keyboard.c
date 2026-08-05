#include <drivers/keyboard.h>
#include <drivers/pic.h>
#include <drivers/pit.h>
#include <drivers/tty.h>
#include <io.h>
#include <kernel/g_kernel.h>
#include <memory/pmm.h>
#include <multitask/spinlock.h>
#include <multitask/wait_queue.h>
#include <stdint.h>
#include <utils/ringbuffer.h>
#include <utils/set_out.h>

#include "hlog.h"

// TODO refactor this mess

keyboard_status_t kb_status;
static ringbuffer_t* keyboard_events;
static keyboard_event_t keyboard_event_pool[KEYBOARD_EVENT_BUFFER_SIZE];
static uint64_t keyboard_event_sequence;
static spinlock_t keyboard_event_lock;
static wait_queue_t keyboard_event_waiters;

char keyboard_characters[] = {
    '1',  '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '^', 0x08,
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '@', '[',
    '\n', 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', ':', ']',
    'z',  'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', ' '};

char shifted_keyboard_characters[] = {
    '!',  '\"', '#', '$', '%', '&', '\'', '(', ')', 0,   '=', '~', 0x08,
    '\t', 'Q',  'W', 'E', 'R', 'T', 'Y',  'U', 'I', 'O', 'P', '`', '{',
    '\n', 'A',  'S', 'D', 'F', 'G', 'H',  'J', 'K', 'L', '+', '*', '}',
    'Z',  'X',  'C', 'V', 'B', 'N', 'M',  '<', '>', '?', ' '};

void keyboard_initialize() {
  kb_status.shift_held = false;
  kb_status.caps_enabled = false;
  keyboard_event_sequence = 0;
  spinlock_init(&keyboard_event_lock);
  wait_queue_init(&keyboard_event_waiters);
  ringbuffer_new(KEYBOARD_EVENT_BUFFER_SIZE,
                 &keyboard_events,
                 NULL,
                 NULL,
                 NULL,
                 NULL,
                 NULL);
}

char lookup_scancode_no_shift(uint8_t scancode) {
  if (scancode < 0x1E) {
    return keyboard_characters[scancode - 0x02];
  } else if (scancode < 0x2B) {
    return keyboard_characters[scancode - 0x03];
  } else if (scancode < 0x38) {
    return keyboard_characters[scancode - 0x05];
  } else if (scancode == 0x39) {
    return keyboard_characters[scancode - 0x08];
  } else if (scancode == 0xF3) {
    return '\\';
  } else {
    return 0;
  }
}

char lookup_scancode_with_shift(uint8_t scancode) {
  if (scancode < 0x1E) {
    return shifted_keyboard_characters[scancode - 0x02];
  } else if (scancode < 0x2B) {
    return shifted_keyboard_characters[scancode - 0x03];
  } else if (scancode < 0x38) {
    return shifted_keyboard_characters[scancode - 0x05];
  } else if (scancode == 0x39) {
    return shifted_keyboard_characters[scancode - 0x08];
  } else if (scancode == 0xF3) {
    return '_';
  } else {
    return 0;
  }
}

char lookup_scancode(uint8_t scancode, keyboard_status_t* status) {
  if (status->shift_held) { return lookup_scancode_with_shift(scancode); }
  return lookup_scancode_no_shift(scancode);
}

static keyboard_event_direction_t keyboard_scancode_direction(
    uint8_t scancode) {
  return (scancode & 0x80) == 0 ? KEYBOARD_KEY_DOWN : KEYBOARD_KEY_UP;
}

static void keyboard_write_event(uint8_t scancode) {
  if (keyboard_events == NULL) { return; }

  uint64_t irq_state = spinlock_lock(&keyboard_event_lock);
  keyboard_event_t* event = &keyboard_event_pool[keyboard_event_sequence %
                                                 KEYBOARD_EVENT_BUFFER_SIZE];
  hlog_write(HLOG_DEBUG,
             "Got kb event addr: %x (pool idx: %d)",
             event,
             keyboard_event_sequence % KEYBOARD_EVENT_BUFFER_SIZE);
  event->scancode = scancode;
  event->direction = keyboard_scancode_direction(scancode);
  event->timestamp = pit_get_ns_elapsed_since_init(g_kernel.pit);
  event->sequence = keyboard_event_sequence++;
  ringbuffer_write(keyboard_events, event);
  spinlock_unlock(&keyboard_event_lock, irq_state);
  wait_queue_wake_all(&keyboard_event_waiters);
}

bool keyboard_read_event(keyboard_event_t* out) {
  if (keyboard_events == NULL) { return false; }

  void* value = NULL;
  uint64_t irq_state = spinlock_lock(&keyboard_event_lock);
  if (!ringbuffer_read(keyboard_events, &value)) {
    spinlock_unlock(&keyboard_event_lock, irq_state);
    return false;
  }
  SET_OUT(out, *((keyboard_event_t*)value));
  spinlock_unlock(&keyboard_event_lock, irq_state);
  return true;
}

bool keyboard_read_event_wait(keyboard_event_t* out) {
  if (keyboard_events == NULL) { return false; }

  while (true) {
    sched_postpone();
    if (keyboard_read_event(out)) {
      sched_resume();
      return true;
    }
    wait_queue_sleep_postponed(&keyboard_event_waiters);
    sched_resume();
  }
}

void handle_scancode(uint8_t scancode) {
  if (scancode == 0x2A) {
    kb_status.shift_held = true;
  } else if (scancode == 0xAA) {
    kb_status.shift_held = false;
  } else {
    char c = lookup_scancode(scancode, &kb_status);
    if (c != 0) tty_receive_char(c);
  }
}

void handle_keyboard() {
  uint8_t status = inb(0x64);
  if (status & 0x1) {
    uint8_t scancode = inb(0x60);
    keyboard_write_event(scancode);
    handle_scancode(scancode);
  }
  send_end_of_interrupt(0x1);
}
