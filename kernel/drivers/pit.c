#include <drivers/pic.h>
#include <drivers/pit.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>

#define TICKS_PER_SECOND 100

uint32_t ticks;

void initialize_pit() {
  ticks = 0;
  // Clock runs at roughtly 1.193180 MHz
  uint16_t divisor = 1193180 / TICKS_PER_SECOND;
  // Ch. 0, hi+lo byte, square number generator mode, 16 bit binary
  outb(0x43, 0x36);
  outb(0x40, divisor & 0xFF);
  outb(0x40, divisor >> 8);
}

void handle_pit() {
  if (ticks > 0 && (ticks % TICKS_PER_SECOND) == 0) {
    printf("One second elapsed.\n");
    ticks = 0;
  } else {
    ticks++;
  }
  send_end_of_interrupt(0x0);
}

