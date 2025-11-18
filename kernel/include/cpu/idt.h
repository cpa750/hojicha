#ifndef IDT_H
#define IDT_H

#include <stdint.h>

struct idt_entry {
  uint16_t base_low;
  uint16_t segment;
  uint8_t reserved_low;
  uint8_t flags;
  uint16_t base_mid;
  uint32_t base_high;
  uint32_t reserved_high;
} __attribute__((packed));
typedef struct idt_entry idt_entry_t;

struct idt_pointer {
  uint16_t limit;
  uint64_t base;
} __attribute((packed));
typedef struct idt_pointer idt_pointer_t;

void initialize_idt();

#endif  // IDT_H
