#include <cpu/gdt.h>
#include <serial.h>
#include <stdio.h>

#define GDT_ENTRIES 6

extern void load_gdt();

void create_gdt_entry(GDTEntry* entry, uint8_t index, uint32_t limit,
                      uint32_t base, uint8_t access, uint8_t flags) {
  if (limit > 0xFFFFF) {
    printf("Fatal: GDT cannot encode limits greater than 0xFFFFF. Halt.");
    serial_write_string(
        "Fatal: GDT cannot encode limits greater than 0xFFFFF. Halt.");
  }

  entry[index].base_low = base & 0xFFFF;
  entry[index].base_mid = (base >> 16) & 0xFF;
  entry[index].base_high = (base >> 24) & 0xFF;

  entry[index].limit_low = limit & 0xFFFF;

  entry[index].limit_high = (limit >> 16) & 0x0F;
  entry[index].limit_high |= granularity & 0xF0;

  entry[index].access = access;
}

void initialize_gdt() {
  GDTEntry gdt_entries[GDT_ENTRIES];
  GDTPointer gdt_pointer;

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
  // TODO: Task state segment

  gdt_pointer = (GDTPointer) {
    .limit = sizeof(uint8_t * 7) - 1;
    .base = gdt_entries;
  }
}

