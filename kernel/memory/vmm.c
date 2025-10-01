#include <kernel/kernel_state.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 4096

struct vmm_state {
  uint32_t first_available_vaddr;
  uint32_t last_available_vaddr;
};
typedef struct vmm_state vmm_state_t;
uint32_t vmm_state_get_first_available_vaddr(vmm_state_t* vmm_state) {
  return vmm_state->first_available_vaddr;
}
uint32_t vmm_state_get_last_available_vaddr(vmm_state_t* vmm_state) {
  return vmm_state->last_available_vaddr;
}
void vmm_state_dump(vmm_state_t* v) {
  printf("[VMM] Struct addr:\t\t\t\t%x B\n", (uint32_t)v);
  printf("[VMM] First available vaddr:\t%x\n", v->first_available_vaddr);
  printf("[VMM] Last available vaddr:\t\t%x\n", v->last_available_vaddr);
}

extern void load_pd(uint32_t* pd_addr);
extern void enable_paging();
uint16_t virt_to_directory_idx(uint32_t virt);
uint16_t virt_to_entry_idx(uint32_t virt);
uint32_t idx_to_vaddr(uint32_t directory_idx, uint32_t entry_idx);
void check_kernel_size(uint32_t kernel_page_count);

uint32_t* page_directory;

void initialize_vmm() {
  static vmm_state_t vmm = {0};
  page_directory = (uint32_t*)pmm_alloc_frame();
  memset(page_directory, 0, 4096);

  uint32_t* pd_identity_entry = (uint32_t*)pmm_alloc_frame();
  for (uint32_t addr = 0; addr < 0x100000; addr += 4096) {
    pd_identity_entry[pmm_addr_to_page(addr)] =
        addr | PAGE_PRESENT | PAGE_WRITABLE;
  }

  uint32_t* pd_kernel_entry = (uint32_t*)pmm_alloc_frame();
  memset(pd_kernel_entry, 0, 4096);
  uint32_t kernel_page_count = pmm_state_get_kernel_page_count(g_kernel.pmm);

  check_kernel_size(kernel_page_count);
  // ok

  uint32_t kernel_start = pmm_state_get_kernel_start(g_kernel.pmm);
  uint32_t kernel_end = pmm_state_get_kernel_end(g_kernel.pmm);
  for (uint32_t addr = kernel_start; addr <= kernel_end; ++addr) {
    pd_identity_entry[pmm_addr_to_page(addr)] =
        addr | PAGE_PRESENT | PAGE_WRITABLE;
  }

  page_directory[0] = ((uint32_t)pd_identity_entry) | 0x03;
  page_directory[1023] = ((uint32_t)page_directory) | 0x03;

  // ok

  load_pd(page_directory);
  enable_paging();

  // ok

  // uint32_t lav = idx_to_vaddr(1022, 1023);
  uint32_t lav = 1022 << 21;
  printf("lav: %d (%x, %b)\n", lav, lav, lav);
  vmm.first_available_vaddr = kernel_end + PAGE_SIZE;
  vmm.last_available_vaddr = idx_to_vaddr(1022, 1023);
  g_kernel.vmm = &vmm;

  g_kernel_dump();

  // printf("Total available memory (in vmm init): %d B\n",
  //        pmm_state_get_total_mem(g_kernel.pmm));
}

uint32_t vmm_map_single(uint32_t virt, uint32_t flags) {
  uint32_t new_page = pmm_alloc_frame();
  // OOM
  if (new_page == 0) {
    return 0;
  }

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

  pd_entry[entry_idx] = pmm_page_to_addr_base(new_page) | flags;
  // TODO: check this is the write mask to clear flags
  uint32_t virt_base = virt & 0xFFFFFF00;
  return virt_base;
}

uint32_t vmm_map(uint32_t virt, uint32_t size, uint32_t flags) {
  while (size-- > 0) {
    uint32_t res = vmm_map_single(virt, flags);
    // OOM
    if (res == 0) {
      return res;
    }
    virt += PAGE_SIZE;
  }
  uint32_t virt_base = virt & 0xFFFFFF00;
  return virt_base;
}

uint32_t vmm_unmap(uint32_t virt) {
  uint16_t directory_idx = virt_to_directory_idx(virt);
  uint32_t* pd_entry;

  if (page_directory[directory_idx] == 0) {
    // Don't need to free something that doesn't exist
    return 0;
  } else {
    pd_entry = (uint32_t*)page_directory[directory_idx];
  }

  uint16_t entry_idx = virt_to_entry_idx(virt);
  pd_entry[entry_idx] = 0;
  // TODO: check this is the write mask to clear flags
  uint32_t virt_base = virt & 0xFFFFFF00;
  return virt_base;
}

uint32_t vmm_to_physical(uint32_t virt) {
  uint16_t directory_idx = virt_to_directory_idx(virt);
  uint32_t* pd_entry;

  if (page_directory[directory_idx] == 0) {
    return 0;
  } else {
    pd_entry = (uint32_t*)page_directory[directory_idx];
  }

  uint16_t entry_idx = virt_to_entry_idx(virt);
  uint32_t phys_base = pd_entry[entry_idx] | 0xFFFFF000;  // Clear flags
  return phys_base;
}

uint16_t virt_to_directory_idx(uint32_t virt) { return virt >> 22; }

uint16_t virt_to_entry_idx(uint32_t virt) {
  return (virt >> 12) & 0b1111111111;
}

uint32_t idx_to_vaddr(uint32_t directory_idx, uint32_t entry_idx) {
  return (directory_idx << 21) | (entry_idx << 12);
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

