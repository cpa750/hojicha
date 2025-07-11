#include <cpu/idt.h>
#include <string.h>

#define IDT_ENTRIES 256

// TODO: define in loadidt.asm
extern void load_idt(uint32_t);

void create_idt_entry(IDTEntry* entries, uint8_t index, uint64_t base,
                      uint8_t flags, uint16_t segment) {
  entries[index].flags = flags;
  entries[index].segment = segment;
  entries[index].reserved = 0x0;
  entries[index].base_low = base & 0xFFFF;
  entries[index].base_high = (base >> 16) & 0xFFFF;
}

void initialize_gdt() {
  IDTEntry entries[IDT_ENTRIES];
  IDTPointer idt_pointer;

  idt_pointer.limit = (sizeof(IDTEntry) * IDT_ENTRIES) - 1;
  idt_pointer.base = &entries;

  memset(&entries, 0x0, sizeof(IDTEntry) * IDT_ENTRIES);

  load_idt();
}

