#include <cpu/idt.h>
#include <cpu/isr.h>
#include <drivers/pic.h>
#include <drivers/pit.h>
#include <haddr.h>
#include <stdlib.h>
#include <string.h>

#define IDT_ENTRIES 256

extern void load_idt();

void create_idt_entry(idt_entry_t* entries, uint8_t index, void* base,
                      uint8_t flags, uint16_t segment) {
  entries[index].flags = flags;
  entries[index].segment = segment;
  entries[index].reserved_low = 0x0;
  entries[index].reserved_high = 0x0;

  entries[index].base_low = (haddr_t)base & 0xFFFF;
  entries[index].base_mid = ((haddr_t)base >> 16) & 0xFFFF;
  entries[index].base_high = ((haddr_t)base >> 32) & 0xFFFFFFFF;
}

void create_isr_entries();
void create_irq_entries();

__attribute__((aligned(0x10))) idt_entry_t entries[IDT_ENTRIES];
idt_pointer_t idt_pointer;
void initialize_idt() {
  idt_pointer.limit = (sizeof(idt_entry_t) * IDT_ENTRIES) - 1;
  idt_pointer.base = (haddr_t)&entries;

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

