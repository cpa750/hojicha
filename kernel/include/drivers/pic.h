#ifndef PIC_H
#define PIC_H

#include <stdint.h>

void pic_initialize();
void enable_irq(uint16_t irq);
void disable_irq(uint16_t irq);
void send_end_of_interrupt(uint8_t irq);

#endif  // PIC_H
