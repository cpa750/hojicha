#ifndef IDT_H
#define IDT_H

#include <stdint.h>

typedef struct {
  uint16_t base_low;
  uint16_t segment;
  uint8_t reserved;
  uint8_t flags;
  uint16_t base_high;
} __attribute__((packed)) IDTEntry;

typedef struct {
  uint16_t limit;
  uint32_t base;
} __attribute((packed)) IDTPointer;

void initialize_idt();

#endif  // IDT_H

