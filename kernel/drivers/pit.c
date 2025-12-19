#include <drivers/pic.h>
#include <drivers/pit.h>
#include <io.h>
#include <kernel/kernel_state.h>

#define NS_DIVISOR       1000000000000ULL
#define TICKS_PER_SECOND 1000ULL

struct pit_state {
  uint64_t ticks_since_init;
  uint64_t ticks_per_second;
};

static uint32_t ticks;
static pit_state_t pit = {0};

void initialize_pit() {
  pit.ticks_per_second = TICKS_PER_SECOND;
  ticks = 0;
  // Clock runs at roughtly 1.193180 MHz
  uint16_t divisor = 1193180 / TICKS_PER_SECOND;
  // Ch. 0, hi+lo byte, square number generator mode, 16 bit binary
  outb(0x43, 0x36);
  outb(0x40, divisor & 0xFF);
  outb(0x40, divisor >> 8);
  g_kernel.pit = &pit;
}

void handle_pit() {
  g_kernel.pit->ticks_since_init++;
  if (ticks > 0 && (ticks % TICKS_PER_SECOND) == 0) {
    ticks = 0;
  } else {
    ticks++;
  }
  send_end_of_interrupt(0x0);
}

uint64_t pit_get_ns_elapsed_since_init(pit_state_t* pit) {
  return pit->ticks_since_init * (NS_DIVISOR / pit->ticks_per_second);
}
