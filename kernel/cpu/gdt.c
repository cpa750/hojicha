#include <cpu/gdt.h>
#include <cpu/tss.h>
#include <serial.h>
#include <stdio.h>
#include <stdlib.h>

#define GDT_ENTRIES 6

extern void load_gdt();

void create_gdt_entry(GDTEntry* entry, uint8_t index, uint32_t limit,
                      uint32_t base, uint8_t access, uint8_t flags) {
  if (limit > 0xFFFFF) {
    printf("Fatal: GDT cannot encode limits greater than 0xFFFFF. Halt.");
    serial_write_string(
        "Fatal: GDT cannot encode limits greater than 0xFFFFF. Halt.");
    abort();
  }

  entry[index].base_low = base & 0xFFFF;
  entry[index].base_mid = (base >> 16) & 0xFF;
  entry[index].base_high = (base >> 24) & 0xFF;

  entry[index].limit_low = limit & 0xFFFF;

  entry[index].flags = (limit >> 16) & 0x0F;
  entry[index].flags |= flags & 0xF0;

  entry[index].access = access;
}

void initialize_gdt() {
  GDTEntry gdt_entries[GDT_ENTRIES];
  GDTPointer gdt_pointer;

  TSS tss = {0};

  // Null descriptor
  create_gdt_entry(gdt_entries, 0, 0x0, 0x0, 0x0, 0x0);
  // Kernel code segment
  create_gdt_entry(gdt_entries, 1, 0xFFFF, 0x0, 0x9A, 0xC);
  // Kernel data segment
  create_gdt_entry(gdt_entries, 2, 0xFFFF, 0x0, 0x92, 0xC);
  // User code segment
  create_gdt_entry(gdt_entries, 3, 0xFFFF, 0x0, 0xFA, 0xC);
  // User data segment
  create_gdt_entry(gdt_entries, 4, 0xFFFF, 0x0, 0xF2, 0xC);
  // Task state segment
  create_gdt_entry(gdt_entries, 5, sizeof(TSS) - 1, (uint32_t)&tss, 0x89, 0x0);

  gdt_pointer = (GDTPointer){.limit = sizeof(uint8_t) * 7 - 1,
                             .base = (uint32_t)gdt_entries};

  load_gdt();
}

