#include <cpu/gdt.h>
#include <cpu/idt.h>
#include <dev/chardev.h>
#include <drivers/keyboard.h>
#include <drivers/pic.h>
#include <drivers/pit.h>
#include <drivers/serial.h>
#include <drivers/tty.h>
#include <drivers/vga.h>
#include <fs/devfs.h>
#include <fs/initrd.h>
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

#if defined(__test_chardev)
#include <dev/chardev_test.h>
#endif
#if defined(__test_kmalloc)
#include <memory/kmalloc_test.h>
#endif
#if defined(__test_initrd)
#include <fs/initrd_test.h>
#endif
#if defined(__test_vfs)
#include <fs/vfs_test.h>
#endif
#if defined(__test_ringbuffer)
#include <utils/ringbuffer_test.h>
#endif
#if defined(__ast_scheduler)
#include <multitask/scheduler_ast.h>
#endif

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

#if defined(__test_kmalloc)
  kmalloc_test();
#endif

  if (!bootmodule_cache_finalize()) {
    printf("Error: Bootmodule cache finalization failed");
    abort();
  }
  print_ok("Bootmodules");
  sched_initialize();
  print_ok("Multitasking");
  if (initrd_initalize() == 0) { print_ok("Initrd"); }
  if (devfs_initialize()) {
    print_ok("Devfs");
    chardev_initialize();
    print_ok("Character Devices");
  }

#if defined(__test_initrd)
  initrd_test();
#endif
#if defined(__test_vfs)
  vfs_test();
#endif
#if defined(__test_chardev)
  chardev_test();
#endif
#if defined(__test_ringbuffer)
  ringbuffer_test();
#endif
#if defined(__ast_scheduler)
  ast_scheduler();
#endif

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

  hlog_write(HLOG_DEBUG,
             "Kernel initialization complete, turning off console log output.");
  hlog_disable_console();

  asm volatile("sti");

  vfs_file_t* init_file = NULL;
  vfs_open("/usr/bin/init", VFS_OPEN_READ, &init_file, NULL);
  vfs_stat_t* init_stat = NULL;
  vfs_fstat(init_file, &init_stat);
  unsigned char* init_contents =
      malloc(sizeof(unsigned char) * init_stat->size);
  uint64_t bytes_read = 0;
  vfs_status_t res =
      vfs_read(init_file, init_contents, init_stat->size, &bytes_read);
  if (res != VFS_STATUS_OK) { hlog_write(HLOG_ERROR, "uh oh..."); }

  elf_t* init = elf_read(init_contents, init_stat->size);
  process_block_t* elf_proc = sched_uproc_new("init", init);
  sched_add_proc(elf_proc);

  vfs_close(init_file);

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
