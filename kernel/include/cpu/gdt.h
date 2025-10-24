#ifndef GDT_H
#define GDT_H

#include <stdint.h>

struct gdt_pointer {
  uint16_t limit;
  uint64_t base;
} __attribute__((packed));
typedef struct gdt_pointer gdt_pointer_t;

struct gdt_entry {
  uint16_t limit_low;
  uint16_t base_low;
  uint8_t base_mid;
  uint8_t access;
  uint8_t flags;  // Also contains limit high
  uint8_t base_high;
} __attribute__((packed));
typedef struct gdt_entry gdt_entry_t;

void initialize_gdt();

#endif  // GDT_H
