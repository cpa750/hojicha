#include <pic.h>
#include <stdio.h>
#include <stdlib.h>
#include <timer.h>

uint8_t ticks = 0;

void handle_timer() {
  if ((ticks++ % 18) == 0) {
    printf("One second elapsed.");
    ticks = 0;
  }

  send_end_of_interrupt(0x0);
}

