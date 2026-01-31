#include <drivers/pic.h>
#include <drivers/pit.h>
#include <io.h>
#include <kernel/kernel_state.h>

#include "stddef.h"

#define NS_DIVISOR       1000000000ULL
#define TICKS_PER_SECOND 1000ULL

struct pit_state {
  uint64_t ticks_since_init;
  uint64_t ticks_per_second;
  pit_callback_t* callbacks_start;
  pit_callback_t* callbacks_end;
};

static uint32_t ticks;
static pit_state_t pit = {0};

void call_callbacks(pit_callback_t* callbacks);

void initialize_pit() {
  pit.ticks_per_second = TICKS_PER_SECOND;
  ticks = 0;
  // Clock runs at roughtly 1.193180 MHz
  uint16_t divisor = 1193180 / TICKS_PER_SECOND;
  // Ch. 0, hi+lo byte, square number generator mode, 16 bit binary
  outb(0x43, 0x36);
  outb(0x40, divisor & 0xFF);
  outb(0x40, divisor >> 8);
  pit.callbacks_start = NULL;
  pit.callbacks_end = NULL;
  g_kernel.pit = &pit;
}

void pit_handle() {
  g_kernel.pit->ticks_since_init++;
  if (ticks > 0 && (ticks % TICKS_PER_SECOND) == 0) {
    ticks = 0;
  } else {
    ticks++;
  }
  send_end_of_interrupt(0x0);
  call_callbacks(g_kernel.pit->callbacks_start);
}

uint64_t pit_get_ns_elapsed_since_init(pit_state_t* pit) {
  return pit->ticks_since_init * (NS_DIVISOR / pit->ticks_per_second);
}

void pit_register_callback(pit_callback_t* callback) {
  callback->next = NULL;
  if (g_kernel.pit->callbacks_start == NULL) {
    g_kernel.pit->callbacks_start = callback;
    g_kernel.pit->callbacks_end = callback;
    return;
  }

  g_kernel.pit->callbacks_end->next = callback;
  g_kernel.pit->callbacks_end = callback;
}

void call_callbacks(pit_callback_t* callbacks) {
  if (callbacks == NULL) { return; }
  if (callbacks->callback_func != NULL) {
    callbacks->callback_func(g_kernel.pit->ticks_since_init *
                             (NS_DIVISOR / g_kernel.pit->ticks_per_second));
  }
  if (callbacks->next != NULL) { call_callbacks(callbacks->next); }
}
