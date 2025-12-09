#include <cpu/gdt.h>
#include <cpu/tss.h>
#include <haddr.h>
#include <kernel/kernel_state.h>
#include <stdio.h>
#include <stdlib.h>

#define GDT_ENTRIES 6

extern void load_gdt();

void create_gdt_entry(gdt_entry_t* entry,
                      uint8_t index,
                      uint32_t limit,
                      uint32_t base,
                      uint8_t access,
                      uint8_t flags) {
  if (limit > 0xFFFFFFFF) {
    printf("Fatal: GDT cannot encode limits greater than 0xFFFFFFFF. Abort.\n");
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

gdt_entry_t gdt_entries[GDT_ENTRIES];
gdt_pointer_t gdt_pointer;
tss_t tss = {0};

void initialize_gdt() {
  // Null descriptor
  create_gdt_entry(gdt_entries, 0, 0x0, 0x0, 0x0, 0x0);
  // Kernel code segment
  create_gdt_entry(gdt_entries, 1, 0xFFFFFFFF, 0x0, 0x9A, 0x20);
  // Kernel data segment
  create_gdt_entry(gdt_entries, 2, 0xFFFFFFFF, 0x0, 0x92, 0x20);
  // User code segment
  create_gdt_entry(gdt_entries, 3, 0xFFFFFFFF, 0x0, 0xFA, 0x20);
  // User data segment
  create_gdt_entry(gdt_entries, 4, 0xFFFFFFFF, 0x0, 0xF2, 0x20);
  // Task state segment
  create_gdt_entry(gdt_entries, 5, sizeof(tss_t) - 1, (haddr_t)&tss, 0x89, 0x0);

  gdt_pointer =
      (gdt_pointer_t){.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1,
                      .base = (uint64_t)&gdt_entries};

  load_gdt();
  g_kernel.tss = &tss;
}
