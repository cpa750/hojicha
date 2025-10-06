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
#include <memory/kmalloc.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  initialize_vmm(multiboot_info);
  printf("[OK] VMM\n");
  kmalloc_initialize();
  printf("[OK] kmalloc\n");

  printf("[INFO] Total available memory:\t%d MB (%d B)\n",
         pmm_state_get_total_mem(g_kernel.pmm) >> 20,
         pmm_state_get_total_mem(g_kernel.pmm));
  printf("[INFO] Total free memory:\t\t%d MB (%d B)\n",
         pmm_state_get_free_mem(g_kernel.pmm) >> 20,
         pmm_state_get_free_mem(g_kernel.pmm));

  printf("\n------------------------------------------------------------\n");
  printf("|                Hojicha kernel initialized.               |\n");
  printf("------------------------------------------------------------\n\n");

  asm volatile("sti");

  char* a = (char*)kmalloc(sizeof(char) * 20);
  strcpy(a, "hello, world!");
  printf("%s\n", a);

  while (1) asm volatile("hlt");
}

