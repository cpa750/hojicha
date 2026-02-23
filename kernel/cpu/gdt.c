#include <cpu/gdt.h>
#include <cpu/tss.h>
#include <haddr.h>
#include <kernel/kernel_state.h>
#include <stdio.h>
#include <stdlib.h>

#define GDT_ENTRIES 8  // 5 segments + 1 TSS entry (3x size)

extern void load_gdt();
extern void load_tss();

gdt_pointer_t gdt_pointer;

static gdt_entry_t gdt_entries[GDT_ENTRIES];
static tss_t tss = {0};

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

void create_system_segment(gdt_entry_t* entry,
                           uint8_t index,
                           uint64_t limit,
                           uint64_t base,
                           uint8_t access,
                           uint8_t flags) {
  if (limit > 0xFFFFFFFFFFFFFFFF) {
    printf("Fatal: GDT cannot encode system segments greater than "
           "0xFFFFFFFFFFFFFFFF. "
           "Abort.\n");
    abort();
  }

  uint64_t limit_low = limit & 0xFFFF;
  uint64_t base_low = base & 0xFFFF;
  uint64_t base_mid = (base >> 16) & 0xFF;
  uint64_t base_high = (base >> 24) & 0xFF;

  uint64_t combined_flags = (limit >> 16) & 0x0F;
  combined_flags |= flags & 0xF0;

  uint64_t value = 0;
  value |= limit_low;
  value |= base_low << 16;
  value |= base_mid << 32;
  value |= ((uint64_t)access << 40);
  value |= (combined_flags << 48);
  value |= base_high << 56;

  *(uint64_t*)&entry[index] = value;
  *(uint64_t*)&entry[index + 1] = ((base >> 32) & 0xFF);
}

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
  create_system_segment(
      gdt_entries, 5, sizeof(tss_t) - 1, (haddr_t)&tss, 0x89, 0x0);

  gdt_pointer =
      (gdt_pointer_t){.limit = (sizeof(gdt_entry_t) * GDT_ENTRIES) - 1,
                      .base = (uint64_t)&gdt_entries};

  load_gdt();
  // load_tss();
  g_kernel.tss = &tss;
}
