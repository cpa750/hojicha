#include <kernel/kernel_state.h>
#include <memory/kmalloc.h>
#include <memory/vmm.h>

uint32_t* free_regions;

void kmalloc_initialize() {
  uint32_t first_available_vaddr =
      vmm_state_get_first_available_vaddr(g_kernel.vmm);
  free_regions = (uint32_t*)vmm_map_single(first_available_vaddr,
                                           PAGE_PRESENT | PAGE_WRITABLE);
  // TODO put struct storing next free block pointer and length info at head of
  // block
}

void* kmalloc(size_t size) {
  // TODO
  return (void*)0;
}

