#include <cpu/gdt.h>
#include <cpu/idt.h>
#include <cpu/protected.h>
#include <drivers/keyboard.h>
#include <drivers/pic.h>
#include <drivers/pit.h>
#include <drivers/serial.h>
#include <kernel/kernel_state.h>
#include <kernel/multiboot.h>
#include <kernel/tty.h>
#include <memory/pmm.h>
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
  initialize_g_kernel();
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
  initialize_pmm(multiboot_info);
  printf("[OK] PMM\n");

  printf("\n");

  printf("[INFO] Total available memory:\t%d MB (%d B)\n",
         pmm_state_get_total_mem(g_kernel.pmm) >> 20,
         pmm_state_get_total_mem(g_kernel.pmm));
  printf("[INFO] Total free memory:\t\t%d MB (%d B)\n",
         pmm_state_get_free_mem(g_kernel.pmm) >> 20,
         pmm_state_get_free_mem(g_kernel.pmm));
  printf("[INFO] Page size:\t\t\t\t%d B\n",
         pmm_state_get_page_size(g_kernel.pmm));

  printf("\n------------------------------------------------------------\n");
  printf("|                Hojicha kernel initialized.               |\n");
  printf("------------------------------------------------------------\n\n");

  asm volatile("sti");

  while (1) asm volatile("hlt");
}

