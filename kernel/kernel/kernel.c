#include <assert.h>
#include <cpu/gdt.h>
#include <cpu/idt.h>
#include <drivers/keyboard.h>
#include <drivers/pic.h>
#include <drivers/pit.h>
#include <drivers/serial.h>
#include <drivers/tty.h>
#include <drivers/vga.h>
#include <kernel/kernel_state.h>
#include <limine.h>
#include <memory/kmalloc.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define STACK_SIZE 16384

void test(void) { printf("hello from the other side"); }

void print_ok(const char* component);

__attribute__((
    used,
    section(".limine_requests"))) static volatile LIMINE_BASE_REVISION(3);

__attribute__((used,
               section(".limine_requests_"
                       "start"))) static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((
    used,
    section(
        ".limine_requests_end"))) static volatile LIMINE_REQUESTS_END_MARKER;

extern haddr_t __stack_start;
haddr_t stack_start_vaddr = (haddr_t)&__stack_start;

void kernel_main() {
  asm volatile("cli");

  if (LIMINE_BASE_REVISION_SUPPORTED == false) { abort(); }

  initialize_g_kernel();
  vga_initialize();
  terminal_initialize();

#if defined(__debug_virtual)
  printf("Hojicha running with virtual debugging enabled.\n");
#endif

  printf("[INFO] Starting Hojicha kernel initialization...\n");
  initialize_serial();
  print_ok("Serial");
  initialize_gdt();
  print_ok("GDT");
  initialize_idt();
  print_ok("IDT");
  initialize_pic();
  print_ok("PIC");
  initialize_pit();
  print_ok("PIT");
  initialize_keyboard();
  print_ok("Keyboard");
  initialize_pmm();
  print_ok("PMM");
  initialize_vmm();
  print_ok("VMM");
  kmalloc_initialize();
  print_ok("kmalloc");
  multitask_initialize();
  print_ok("Multitasking");

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

  process_block_t* kernel_process = g_kernel.current_process;
  process_block_t* new_proc = (process_block_t*)malloc(sizeof(process_block_t));
  kernel_process->next = new_proc;
  new_proc->next = kernel_process;
  new_proc->cr3 = kernel_process->cr3;
  uint8_t* new_stack_end = (uint8_t*)malloc(STACK_SIZE);
  haddr_t stack_base = (haddr_t)(new_stack_end + STACK_SIZE);
  stack_base &= ~0xFULL;
  stack_base -= 8 * 15;
  stack_base -= 8;
  *(haddr_t*)stack_base = (haddr_t)test;

  new_proc->rsp = (void*)stack_base;
  multitask_new(new_proc);

  while (1) asm volatile("hlt");
}

void print_ok(const char* component) {
  printf("[");
  terminal_set_fg(0x00FF00);
  printf("OK");
  terminal_set_fg(0xFFFFFF);
  printf("] %s\n", component);
}
