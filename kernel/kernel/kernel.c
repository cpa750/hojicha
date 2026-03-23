#include <cpu/gdt.h>
#include <cpu/idt.h>
#include <drivers/keyboard.h>
#include <drivers/pic.h>
#include <drivers/pit.h>
#include <drivers/serial.h>
#include <drivers/tty.h>
#include <drivers/vga.h>
#include <hlog.h>
#include <kernel/g_kernel.h>
#include <limine.h>
#include <memory/kmalloc.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <multitask/bootmodule.h>
#include <multitask/elf.h>
#include <multitask/scheduler.h>
#include <multitask/semaphore.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "fs/initrd.h"
#include "fs/vfs.h"

static process_block_t* kernel_proc;

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
  if (!bootmodule_capture_early()) {
    printf("Error: Bootmodule initial capture failed");
    abort();
  }
  initialize_vmm();
  print_ok("VMM");
  kmalloc_initialize();
  print_ok("kmalloc");
  if (!bootmodule_finalize_cache()) {
    printf("Error: Bootmodule cache finalization failed");
    abort();
  }
  print_ok("Bootmodules");
  sched_initialize();
  print_ok("Multitasking");

  printf("\n");

  hlog_add(HLOG_INFO,
           "Total available memory:\t%d MB (%d B)",
           pmm_state_get_total_mem(g_kernel.pmm) >> 20,
           pmm_state_get_total_mem(g_kernel.pmm));
  hlog_add(HLOG_INFO,
           "Total free memory:\t\t%d MB (%d B)",
           pmm_state_get_free_mem(g_kernel.pmm) >> 20,
           pmm_state_get_free_mem(g_kernel.pmm));
  hlog_commit();

  printf("\n------------------------------------------------------------\n");
  printf("|                Hojicha kernel initialized.               |\n");
  printf("------------------------------------------------------------\n\n");

  asm volatile("sti");

  kernel_proc = g_kernel.current_process;

  bootmodule_t* initrd_module = bootmodule_get("initrd.tar");
  if (initrd_module == NULL) {
    hlog_write(HLOG_ERROR, "Unable to find cached module initrd.tar");
  } else {
    hlog_write(HLOG_INFO,
               "Loaded cached module %s at %x (%d bytes)",
               initrd_module->name,
               initrd_module->address,
               initrd_module->size);
  }
  vfs_mount_t* initrd = NULL;
  initrd_from_ustar(initrd_module->address, initrd_module->size, &initrd);
  vfs_mount_root(initrd);
  vfs_file_t* f = NULL;
  vfs_open("/usr/bin/bigmaths.elf", VFS_OPEN_READ, &f);
  vfs_stat_t* bigmaths_stat = NULL;
  vfs_fstat(f, &bigmaths_stat);
  unsigned char* bigmath_contents =
      malloc(sizeof(unsigned char) * bigmaths_stat->size);
  uint64_t bytes_read = 0;
  vfs_status_t res =
      vfs_read(f, bigmath_contents, bigmaths_stat->size, &bytes_read);
  if (res != VFS_STATUS_OK) { hlog_write(HLOG_ERROR, "uh oh..."); }

  elf_t* bigmaths = elf_read(bigmath_contents, bigmaths_stat->size);
  process_block_t* elf_proc = sched_uproc_new("bigmaths", bigmaths);
  sched_add_proc(elf_proc);

  elf_t* bigmaths2_electric_boogaloo =
      elf_read(bigmath_contents, bigmaths_stat->size);
  process_block_t* elf_proc2 = sched_uproc_new("bigmaths2_electric_boogaloo",
                                               bigmaths2_electric_boogaloo);
  sched_add_proc(elf_proc2);

  sched_yield();

  while (1) { asm volatile("hlt"); }
}

void print_ok(const char* component) {
  printf("[");
  terminal_set_fg(0x00FF00);
  printf("OK");
  terminal_set_fg(0xFFFFFF);
  printf("] %s\n", component);
}
