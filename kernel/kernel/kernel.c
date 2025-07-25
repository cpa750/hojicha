#include <cpu/gdt.h>
#include <cpu/idt.h>
#include <cpu/protected.h>
#include <drivers/pic.h>
#include <drivers/pit.h>
#include <drivers/serial.h>
#include <kernel/tty.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

void kernel_main(void) {
  if (!is_protected_mode()) {
    printf(
        "Fatal: bootloader handed over with protected mode disabled. Abort.");
    abort();
  }
  asm volatile("cli");
  terminal_initialize();
  initialize_serial();
  printf("Serial interface initialized.\n");
  initialize_gdt();
  printf("GDT initialized.\n");
  initialize_idt();
  printf("IDT initialized.\n");
  initialize_pic();
  printf("PICs initialized.\n");
  initialize_pit();
  printf("PIT initialized.\n");

  printf("\n------------------------------------------------------------\n");
  printf("|                Hojicha kernel initialized.               |\n");
  printf("------------------------------------------------------------\n\n");

  asm volatile("sti");

  while (1) asm volatile("hlt");
}

