#include <cpu/gdt.h>
#include <cpu/idt.h>
#include <drivers/keyboard.h>
#include <drivers/pic.h>
#include <drivers/pit.h>
#include <drivers/serial.h>
#include <drivers/tty.h>
#include <drivers/vga.h>
#include <fs/vfs.h>
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

  g_kernel_initialize();
  vga_initialize();
  terminal_initialize();

#if defined(__debug_virtual)
  printf("Hojicha running with virtual debugging enabled.\n");
#endif

  printf("[INFO] Starting Hojicha kernel initialization...\n");
  serial_initialize();
  print_ok("Serial");
  gdt_initialize();
  print_ok("GDT");
  idt_initialize();
  print_ok("IDT");
  pic_initialize();
  print_ok("PIC");
  pit_initialize();
  print_ok("PIT");
  keyboard_initialize();
  print_ok("Keyboard");
  pmm_initialize();
  print_ok("PMM");
  if (!bootmodule_initialize()) {
    printf("Error: Bootmodule initial capture failed");
    abort();
  }
  vmm_initialize();
  print_ok("VMM");
  kmalloc_initialize();
  print_ok("kmalloc");
  if (!bootmodule_cache_finalize()) {
    printf("Error: Bootmodule cache finalization failed");
    abort();
  }
  print_ok("Bootmodules");
  sched_initialize();
  print_ok("Multitasking");
  if (initrd_initalize() == 0) { print_ok("Initrd"); }

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

  vfs_file_t* etc = NULL;
  vfs_open("/etc/", VFS_OPEN_DIRECTORY, &etc);

  vfs_dirent_t* etc_dirent = NULL;
  vfs_readdir(etc, &etc_dirent);
  while (etc_dirent != NULL) {
    hlog_write(HLOG_INFO, "/etc/%s", etc_dirent->name);
    vfs_readdir(etc, &etc_dirent);
  }

  vfs_file_t* usrbin = NULL;
  vfs_open("/usr/bin", VFS_OPEN_DIRECTORY, &usrbin);

  vfs_dirent_t* usrbin_dirent = NULL;
  vfs_readdir(usrbin, &usrbin_dirent);
  while (usrbin_dirent != NULL) {
    hlog_write(HLOG_INFO, "/usr/bin/%s", usrbin_dirent->name);
    vfs_readdir(usrbin, &usrbin_dirent);
  }

  vfs_file_t* test = NULL;
  vfs_open("/etc/test.txt", VFS_OPEN_READ, &test);
  char test1[50];
  vfs_read(test, test1, 50, &bytes_read);
  hlog_write(HLOG_INFO, "test1: %s (%d B)", test1, bytes_read);

  vfs_seek(test, -500, VFS_SEEK_CUR, NULL);
  char test2[50];
  vfs_read(test, test2, 50, &bytes_read);
  hlog_write(HLOG_INFO, "test2: %s (%d B)", test2, bytes_read);

  vfs_seek(test, -15, VFS_SEEK_END, NULL);
  char test3[50];
  vfs_read(test, test3, 50, &bytes_read);
  hlog_write(HLOG_INFO, "test3: %s (%d B)", test3, bytes_read);

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
