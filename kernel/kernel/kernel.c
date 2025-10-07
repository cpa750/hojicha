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

void print_ok(const char* component);

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
  print_ok("Serial");
  initialize_gdt();
  print_ok("GDT");
  initialize_idt();
  print_ok("IDT");
  initialize_pic();
  print_ok("PICs");
  initialize_pit();
  print_ok("PIT");
  initialize_keyboard();
  print_ok("Keyboard");
  initialize_pmm(multiboot_info);
  print_ok("PMM");
  initialize_vmm(multiboot_info);
  print_ok("VMM");
  kmalloc_initialize();
  print_ok("kmalloc");

  printf("\n");

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

  char* a = (char*)malloc(sizeof(char) * 20);
  strcpy(a, "hello, world!");
  printf("%s\n", a);

  char* b = (char*)malloc(sizeof(char) * 30);
  strcpy(b, "this is so cool!!!");
  printf("%s\n", b);

  char* c = (char*)malloc(sizeof(char) * 40);
  strcpy(c, "look at me ma, no stack!");
  printf("%s\n", c);

  printf("a=%x, b=%x, c=%x\n", (uint32_t)a, (uint32_t)b, (uint32_t)c);

  while (1) asm volatile("hlt");
}

void print_ok(const char* component) {
  printf("[");
  terminal_set_color(2);
  printf("OK");
  terminal_set_color(7);
  printf("] %s\n", component);
}

