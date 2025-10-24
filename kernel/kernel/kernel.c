#include <cpu/gdt.h>
// #include <cpu/idt.h>
// #include <drivers/keyboard.h>
// #include <drivers/pic.h>
// #include <drivers/pit.h>
#include <drivers/serial.h>
#include <drivers/tty.h>
#include <drivers/vga.h>
#include <kernel/kernel_state.h>
#include <limine.h>
// #include <memory/kmalloc.h>
// #include <memory/pmm.h>
// #include <memory/vmm.h>
// #include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

void print_ok(const char* component);

__attribute__((
    used, section(".limine_requests"))) static volatile LIMINE_BASE_REVISION(3);

__attribute__((used,
               section(".limine_requests_"
                       "start"))) static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((
    used,
    section(
        ".limine_requests_end"))) static volatile LIMINE_REQUESTS_END_MARKER;

// void kernel_main(multiboot_info_t* multiboot_info, uint32_t magic) {
void kernel_main() {
  asm volatile("cli");

  if (LIMINE_BASE_REVISION_SUPPORTED == false) {
    abort();
  }

#if defined(__debug_virtual)
  // printf("Hojicha running with virtual debugging enabled.\n");
#endif

  initialize_g_kernel();
  vga_initialize();
  terminal_initialize();

  printf("[INFO] Starting Hojicha kernel initialization...\n");
  initialize_serial();
  print_ok("Serial");
  initialize_gdt();
  print_ok("GDT");
  // initialize_idt();
  // print_ok("IDT");
  // initialize_pic();
  // print_ok("PICs");
  // initialize_pit();
  // print_ok("PIT");
  // initialize_keyboard();
  // print_ok("Keyboard");
  // initialize_pmm(multiboot_info);
  // print_ok("PMM");
  // initialize_vmm(multiboot_info);
  // print_ok("VMM");
  // kmalloc_initialize();
  // print_ok("kmalloc");

  // printf("\n");

  // printf("[INFO] Total available memory:\t%d MB (%d B)\n",
  //        pmm_state_get_total_mem(g_kernel.pmm) >> 20,
  //        pmm_state_get_total_mem(g_kernel.pmm));
  // printf("[INFO] Total free memory:\t\t%d MB (%d B)\n",
  //        pmm_state_get_free_mem(g_kernel.pmm) >> 20,
  //        pmm_state_get_free_mem(g_kernel.pmm));

  // printf("\n------------------------------------------------------------\n");
  // printf("|                Hojicha kernel initialized.               |\n");
  // printf("------------------------------------------------------------\n\n");

  // asm volatile("sti");
  //     // Ensure the bootloader actually understands our base revision (see
  //     spec).

  terminal_write("hello, world\n", 13);
  terminal_write("this is pretty cool\n", 20);
  terminal_set_fg(0x00FFFF);
  char buf[100];
  for (int i = 0; i < 10000; ++i) {
    itoa(i, buf, 10);
    if (i < 10) {
      terminal_write(buf, 3);
    } else if (i < 100) {
      terminal_write(buf, 4);
    } else {
      terminal_write(buf, 5);
    }
    terminal_write("\n", 1);
  }

  while (1) asm volatile("hlt");
}

void print_ok(const char* component) {
  printf("[");
  terminal_set_fg(0x00FF00);
  printf("OK");
  terminal_set_fg(0xFFFFFF);
  printf("] %s\n", component);
}

