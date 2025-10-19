//#include <cpu/gdt.h>
//#include <cpu/idt.h>
//#include <cpu/protected.h>
//#include <drivers/keyboard.h>
//#include <drivers/pic.h>
//#include <drivers/pit.h>
#include <drivers/serial.h>
//#include <kernel/kernel_state.h>
//#include <kernel/multiboot.h>
//#include <kernel/tty.h>
//#include <memory/kmalloc.h>
//#include <memory/pmm.h>
//#include <memory/vmm.h>
//#include <stddef.h>
//#include <stdio.h>
//#include <stdlib.h>

// void print_ok(const char* component);

// void kernel_main(multiboot_info_t* multiboot_info, uint32_t magic) {
void kernel_main() {
  //if (!is_protected_mode()) {
  //  printf(
  //      "Fatal: bootloader handed over with protected mode disabled. Abort.");
  //  abort();
  //}
  asm volatile("cli");

//#if defined (__debug_virtual)
//  printf("Hojicha running with virtual debugging enabled.\n");
//#endif


  //initialize_g_kernel();
  initialize_serial();
  //terminal_initialize();
  //print_ok("Serial");
  //initialize_gdt();
  //print_ok("GDT");
  //initialize_idt();
  //print_ok("IDT");
  //initialize_pic();
  //print_ok("PICs");
  //initialize_pit();
  //print_ok("PIT");
  //initialize_keyboard();
  //print_ok("Keyboard");
  //initialize_pmm(multiboot_info);
  //print_ok("PMM");
  //initialize_vmm(multiboot_info);
  //print_ok("VMM");
  //kmalloc_initialize();
  //print_ok("kmalloc");

  //printf("\n");

  //printf("[INFO] Total available memory:\t%d MB (%d B)\n",
  //       pmm_state_get_total_mem(g_kernel.pmm) >> 20,
  //       pmm_state_get_total_mem(g_kernel.pmm));
  //printf("[INFO] Total free memory:\t\t%d MB (%d B)\n",
  //       pmm_state_get_free_mem(g_kernel.pmm) >> 20,
  //       pmm_state_get_free_mem(g_kernel.pmm));

  //printf("\n------------------------------------------------------------\n");
  //printf("|                Hojicha kernel initialized.               |\n");
  //printf("------------------------------------------------------------\n\n");

  //asm volatile("sti");

  while (1) asm volatile("hlt");
}

//void print_ok(const char* component) {
//  printf("[");
//  terminal_set_color(2);
//  printf("OK");
//  terminal_set_color(7);
//  printf("] %s\n", component);
//}

