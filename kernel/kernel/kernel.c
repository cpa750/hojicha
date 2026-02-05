#include <cpu/gdt.h>
#include <cpu/idt.h>
#include <drivers/keyboard.h>
#include <drivers/pic.h>
#include <drivers/pit.h>
#include <drivers/serial.h>
#include <drivers/tty.h>
#include <drivers/vga.h>
#include <kernel/kernel_state.h>
#include <kernel/multitask.h>
#include <limine.h>
#include <memory/kmalloc.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static process_block_t* kernel_proc;

static bool sleep_awake_1 = false;
static bool sleep_awake_2 = false;

void test1(void) {
  while (1) {
    if (sleep_awake_1) {
      printf("1\n");
      sleep_awake_1 = false;
    }
  }
}

void test2(void) {
  while (1) {
    if (sleep_awake_2) {
      printf("2\n");
      sleep_awake_2 = false;
    }
  }
}

void test3(void) {
  multitask_sleep(7);
  printf("test3, about to die o7\n");
}

void test4(void) {
  multitask_sleep(7);
  printf("test4, about to die o7\n");
}

void test_sleep(void) {
  while (1) {
    multitask_sleep(5);
    printf("Sleep task awake!\n");
    sleep_awake_1 = true;
    sleep_awake_2 = true;
  }
}

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

  kernel_proc = g_kernel.current_process;

  process_block_t* sleep_proc = multitask_proc_new(
      test_sleep, multitask_process_block_get_cr3(kernel_proc));
  multitask_scheduler_add_proc(sleep_proc);

  process_block_t* test1_proc =
      multitask_proc_new(test1, multitask_process_block_get_cr3(kernel_proc));
  multitask_scheduler_add_proc(test1_proc);

  process_block_t* test2_proc =
      multitask_proc_new(test2, multitask_process_block_get_cr3(kernel_proc));
  multitask_scheduler_add_proc(test2_proc);

  process_block_t* test3_proc =
      multitask_proc_new(test3, multitask_process_block_get_cr3(kernel_proc));
  multitask_scheduler_add_proc(test3_proc);

  process_block_t* test4_proc =
      multitask_proc_new(test4, multitask_process_block_get_cr3(kernel_proc));
  multitask_scheduler_add_proc(test4_proc);

  // while (1) asm volatile("hlt");
  multitask_scheduler_lock();
  multitask_schedule();
  multitask_scheduler_unlock();

  uint8_t count = 0;
  while (1) {
    multitask_sleep(1);

    printf("Kernel awake!\n");

    ++count;

    if (count == 15) {
      printf("terminating task 2\n");
      multitask_proc_terminate(test2_proc);
    }

    if (count == 21) {
      printf("terminating task 1\n");
      multitask_proc_terminate(test1_proc);
    }

    multitask_scheduler_lock();
    multitask_schedule();
    multitask_scheduler_unlock();
  }
}

void print_ok(const char* component) {
  printf("[");
  terminal_set_fg(0x00FF00);
  printf("OK");
  terminal_set_fg(0xFFFFFF);
  printf("] %s\n", component);
}
