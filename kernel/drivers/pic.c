#include <drivers/pic.h>
#include <io.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

void remap_pics(uint32_t main_offset, uint32_t sub_offset);
void full_mask_pics();

void initialize_pic() {
  remap_pics(0x20, 0x28);
  full_mask_pics();
  // TODO: Shouldn't these be in their respective drivers...?
  enable_irq(0x0);  // Enable only timer interrupts
  enable_irq(0x1);  // ...and the keyboard
}

void remap_pics(uint32_t main_offset, uint32_t sub_offset) {
  outb(0x20, 0x11);  // Initialize init sequence
  outb(0xA0, 0x11);
  outb(0x21, main_offset);
  outb(0xA1, sub_offset);
  outb(0x21, 0b100);  // Inform main PIC of sub at 0x8
  outb(0xA1, 0b10);   // Inform sub PIC it's at IRQ2
  outb(0x21, 0b1);    // Set 8086 mode
  outb(0xA1, 0b1);
}

void full_mask_pics() {
  outb(0x21, 0xff);
  outb(0xA1, 0xff);
}

void enable_irq(uint16_t irq) {
  if (irq < 8) {
    inb(0x21);
    outb(0x21, ~(1 << irq));
  } else {
    inb(0xA1);
    outb(0xA1, ~(1 << (irq % 8)));
  }
}

void disable_irq(uint16_t irq) {
  if (irq < 8) {
    inb(0x21);
    outb(0x21, (1 << irq));
  } else {
    inb(0xA1);
    outb(0xA1, (1 << (irq % 8)));
  }
}

void send_end_of_interrupt(uint8_t irq) {
  if (irq >= 8) outb(0xA0, 0x20);
  outb(0x20, 0x20);
}

