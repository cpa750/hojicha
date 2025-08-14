#include <cpu/idt.h>
#include <cpu/isr.h>
#include <drivers/pic.h>
#include <drivers/pit.h>
#include <stdlib.h>
#include <string.h>

#define IDT_ENTRIES 256

extern void load_idt();

void create_idt_entry(IDTEntry* entries, uint8_t index, void* base,
                      uint8_t flags, uint16_t segment) {
  entries[index].flags = flags;
  entries[index].segment = segment;
  entries[index].reserved = 0x0;

  // Even though the entries are u16, we need to cast to u32 here.
  // This is because casting to u16 would effectively wipe the higher
  // half-word from the address, leading to an invalid IDT entry.
  entries[index].base_low = (uint32_t)base & 0xFFFF;
  entries[index].base_high = ((uint32_t)base >> 16) & 0xFFFF;
}

void create_isr_entries();
void create_irq_entries();

__attribute__((aligned(0x10))) IDTEntry entries[IDT_ENTRIES];
IDTPointer idt_pointer;
void initialize_idt() {
  idt_pointer.limit = (sizeof(IDTEntry) * IDT_ENTRIES) - 1;
  idt_pointer.base = (uint32_t)&entries;

  create_isr_entries();
  create_irq_entries();
  load_idt();
}

extern void* isr_stub_table[];
extern void* irq_stub_table[];

void create_isr_entries() {
  for (size_t vector = 0; vector < 32; vector++) {
    create_idt_entry(entries, vector, isr_stub_table[vector], 0x8E, 0x08);
  }
}

void create_irq_entries() {
  create_idt_entry(entries, 32, irq_stub_table[0], 0x8E, 0x08);  // PIT
  create_idt_entry(entries, 33, irq_stub_table[1], 0x8E, 0x08);  // Keyboard
}

