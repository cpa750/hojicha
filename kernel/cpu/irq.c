#include <cpu/isr.h>
#include <drivers/keyboard.h>
#include <drivers/pic.h>
#include <drivers/pit.h>
#include <stdio.h>

// Need to pause interrupts while we handle this.
// If we don't, it causes some sort of race condition
// after the first interrupt, softlocking the kernel.
void handle_irq(InterruptRegisters r) {
  asm volatile("cli");
  switch (r.interrupt_number) {
    case 32:
      handle_pit();
      break;
    case 33:
      handle_keyboard();
      break;
  }
  asm volatile("sti");
}

