#include <cpu/gdt.h>
#include <cpu/idt.h>
#include <cpu/protected.h>
#include <drivers/keyboard.h>
#include <drivers/pic.h>
#include <drivers/pit.h>
#include <drivers/serial.h>
#include <kernel/multiboot.h>
#include <kernel/tty.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

void kernel_main(multiboot_info_t* multiboot_info, uint32_t magic) {
  if (!is_protected_mode()) {
    printf(
        "Fatal: bootloader handed over with protected mode disabled. Abort.");
    abort();
  }
  asm volatile("cli");
  terminal_initialize();
  initialize_serial();
  printf("[OK] Serial\n");
  initialize_gdt();
  printf("[OK] GDT\n");
  initialize_idt();
  printf("[OK] IDT\n");
  initialize_pic();
  printf("[OK] PICs\n");
  initialize_pit();
  printf("[OK] PIT\n");
  initialize_keyboard();
  printf("[OK] Keyboard\n");

  printf("\n------------------------------------------------------------\n");
  printf("|                Hojicha kernel initialized.               |\n");
  printf("------------------------------------------------------------\n\n");

  asm volatile("sti");

  while (1) asm volatile("hlt");
}

