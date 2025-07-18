#include <cpu/isr.h>
#include <drivers/pic.h>
#include <drivers/pit.h>

void handle_irq(InterruptRegisters r) {
  asm volatile("cli");
  switch (r.interrupt_number) {
    case 32:
      handle_pit();
      break;
  }

  send_end_of_interrupt(0x0);
  asm volatile("sti");
}

