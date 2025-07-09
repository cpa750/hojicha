#ifndef GDT_H
#define GDT_H

#include <stdint.h>

typedef struct {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed)) GDTPointer;

typedef struct {
  uint16_t limit_low;
  uint16_t base_low;
  uint8_t base_mid;
  uint8_t access;
  uint8_t flags;  // Also contains limit high
  uint8_t base_high;
} __attribute__((packed)) GDTEntry;

void initialize_gdt();

#endif  // GDT_H
