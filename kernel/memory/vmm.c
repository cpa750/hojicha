#include <kernel/kernel_state.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_PRESENT 0x1
#define PAGE_WRITABLE 0x2
#define PAGE_USER_ACCESIBLE 0x4

#define KERNEL_VIRT_START 0xC0000000
#define KERNEL_PD_START_IDX KERNEL_VIRT_START >> 22

extern void load_pd(uint32_t* pd_addr);
extern void enable_paging();

uint16_t virt_to_directory_idx(uint32_t virt);
uint16_t virt_to_entry_idx(uint32_t virt);
void check_kernel_size(uint32_t kernel_page_count);

uint32_t* page_directory;

void initialize_vmm() {
  page_directory = (uint32_t*)pmm_alloc_frame();
  memset(page_directory, 0, 4096);

  uint32_t* pd_identity_entry = (uint32_t*)pmm_alloc_frame();
  for (uint32_t addr = 0; addr < 0x100000; addr += 4096) {
    pd_identity_entry[addr >> 12] = addr | PAGE_PRESENT | PAGE_WRITABLE;
  }

  uint32_t* pd_kernel_entry = (uint32_t*)pmm_alloc_frame();
  memset(pd_kernel_entry, 0, 4096);
  uint32_t kernel_page_count = pmm_state_get_kernel_page_count(g_kernel.pmm);

  check_kernel_size(kernel_page_count);

  uint32_t kernel_start = pmm_state_get_kernel_start(g_kernel.pmm);
  uint32_t kernel_end = pmm_state_get_kernel_end(g_kernel.pmm);
  for (uint32_t addr = kernel_start; addr < kernel_end; ++addr) {
    pd_identity_entry[addr >> 12] = addr | PAGE_PRESENT | PAGE_WRITABLE;
  }

  page_directory[0] = ((uint32_t)pd_identity_entry) | 0x03;
  page_directory[1023] = ((uint32_t)page_directory) | 0x03;

  load_pd(page_directory);
  enable_paging();
}

uint32_t vmm_map(uint32_t virt, uint32_t phys, uint32_t flags) {
  uint16_t directory_idx = virt_to_directory_idx(virt);
  uint32_t* pd_entry;

  if (page_directory[directory_idx] == 0) {
    pd_entry = (uint32_t*)pmm_alloc_frame();
    page_directory[directory_idx] = (uint32_t)pd_entry;
  } else {
    pd_entry = (uint32_t*)page_directory[directory_idx];
  }

  // OOM
  if (pd_entry == 0) {
    return 0;
  }

  uint16_t entry_idx = virt_to_entry_idx(virt);
  pd_entry[entry_idx] = phys | flags;
  uint32_t virt_base = virt & 0xFFFFFF00;
  return virt_base;
}

uint32_t vmm_unmap(uint32_t virt) {
  // TODO
  return 0;
}

uint32_t vmm_to_physical(uint32_t virt) {
  // TODO
  return 0;
}

uint16_t virt_to_directory_idx(uint32_t virt) { return virt >> 22; }

uint16_t virt_to_entry_idx(uint32_t virt) {
  return (virt >> 12) & 0b1111111111;
}

void check_kernel_size(uint32_t kernel_page_count) {
  if (kernel_page_count > 1023 - 256) {
    // TODO: we need support for more than one PDE for the kernel
    printf(
        "Kernels requiring more than one page directory entry are not yet "
        "supported. Halt.\n");
    abort();
  }
}

