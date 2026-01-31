#include <cpu/isr.h>
#include <drivers/keyboard.h>
#include <drivers/pic.h>
#include <drivers/pit.h>

void handle_irq(interrupt_frame_t* frame) {
  switch (frame->int_no) {
    case 32:
      pit_handle();
      break;
    case 33:
      handle_keyboard();
      break;
  }
}
