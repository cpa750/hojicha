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

  printf("sum=%d\n", 30 - (10 + 13 + 4));

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

  free(b);

  // Should be allocated in old b
  char* d = (char*)malloc(sizeof(char) * 10);
  strcpy(d, "test");
  printf("%s\n", d);
  printf("%x\n", (uint32_t)d);

  // free(a);

  kmalloc_print_free_blocks();

  // force us to grow the heap
  // for (int i = 0; i < 500; ++i) {
  //  char* n = (char*)malloc(sizeof(char) * 2000);
  //  printf("\n\nnew iteration i=%d n=%x\n", i, n);
  //  strcpy(n, "grow");
  //  memset(n, 0xF, 1999);
  //}

  printf("c: %s\n", c);

  char *e, *f, *g, *h, *i, *j, *k, *l, *m, *n, *o, *p;
  e = malloc(sizeof(char) * 10);
  f = malloc(sizeof(char) * 10);
  g = malloc(sizeof(char) * 10);
  h = malloc(sizeof(char) * 10);
  i = malloc(sizeof(char) * 10);
  j = malloc(sizeof(char) * 10);
  k = malloc(sizeof(char) * 10);
  l = malloc(sizeof(char) * 10);
  m = malloc(sizeof(char) * 10);
  n = malloc(sizeof(char) * 10);
  o = malloc(sizeof(char) * 10);
  p = malloc(sizeof(char) * 10);

  printf("e=%x, f=%x, g=%x, h=%x\n", (uint32_t)e, (uint32_t)f, (uint32_t)g,
         (uint32_t)h);
  printf("i=%x, j=%x, k=%x, l=%x\n", (uint32_t)i, (uint32_t)j, (uint32_t)k,
         (uint32_t)l);
  printf("m=%x, n=%x, o=%x, p=%x\n", (uint32_t)m, (uint32_t)n, (uint32_t)o,
         (uint32_t)p);

  free(f);
  free(e);

  free(h);
  free(j);
  free(i);

  free(l);
  free(m);

  free(o);
  free(p);

  kmalloc_print_free_blocks();

  while (1) asm volatile("hlt");
}

void print_ok(const char* component) {
  printf("[");
  terminal_set_color(2);
  printf("OK");
  terminal_set_color(7);
  printf("] %s\n", component);
}

