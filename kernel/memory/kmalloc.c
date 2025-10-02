#include <kernel/kernel_state.h>
#include <memory/kmalloc.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <stdio.h>
#include <stdlib.h>

// TODO: Does hard capping this here make sense?
#define MAX_GROW_SIZE 160

typedef struct free_region free_region_t;
struct free_region {
  uint32_t size_bytes;
  free_region_t* next;
};

free_region_t* free_regions;
uint32_t kernel_heap_grow_size;

void kmalloc_initialize() {
  kernel_heap_grow_size = 5;  // Allocate 5 pages to start
  uint32_t first_available_vaddr =
      vmm_state_get_first_available_vaddr(g_kernel.vmm);
  free_regions =
      (free_region_t*)vmm_map(first_available_vaddr, kernel_heap_grow_size,
                              PAGE_PRESENT | PAGE_WRITABLE);

  if (free_regions == 0) {
    printf("OOM initializing kmalloc. Halt.");
    abort();
  }

  free_regions->size_bytes =
      pmm_page_to_addr_base(kernel_heap_grow_size) - first_available_vaddr;
  free_regions->next = NULL;

  if (kernel_heap_grow_size < MAX_GROW_SIZE) {
    kernel_heap_grow_size <<= 1;
  }
}

void* kmalloc(size_t size) {
  // TODO
  return (void*)0;
}

