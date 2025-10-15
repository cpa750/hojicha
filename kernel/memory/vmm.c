#include <kernel/kernel_state.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 4096
#define VIRT_PD_START 0xFFFFF000
#define VIRT_PT_START 0xFFC00000

struct vmm_state {
  haddr_t first_available_vaddr;
  haddr_t last_available_vaddr;
};
typedef struct vmm_state vmm_state_t;
haddr_t vmm_state_get_first_available_vaddr(vmm_state_t* vmm_state) {
  return vmm_state->first_available_vaddr;
}
haddr_t vmm_state_get_last_available_vaddr(vmm_state_t* vmm_state) {
  return vmm_state->last_available_vaddr;
}
void vmm_state_dump(vmm_state_t* v) {
  printf("[VMM] Struct addr:\t\t\t\t%x B\n", (haddr_t)v);
  printf("[VMM] First available vaddr:\t%x\n", v->first_available_vaddr);
  printf("[VMM] Last available vaddr:\t\t%x\n", v->last_available_vaddr);
}

extern void enable_paging();
extern void load_pd(haddr_t* pd_addr);

void check_kernel_size(haddr_t kernel_page_count);
haddr_t* get_page_table(haddr_t idx);
haddr_t idx_to_vaddr(haddr_t directory_idx, haddr_t entry_idx);
uint16_t virt_to_directory_idx(haddr_t virt);
uint16_t virt_to_entry_idx(haddr_t virt);
void _invlpg(haddr_t virt);

haddr_t* page_directory;
haddr_t* virtual_directory;

void initialize_vmm() {
  static vmm_state_t vmm = {0};
  page_directory = (haddr_t*)pmm_alloc_frame();
  memset(page_directory, 0, 4096);

  haddr_t* pd_identity_entry = (haddr_t*)pmm_alloc_frame();
  for (haddr_t addr = 0; addr < 0x100000; addr += 4096) {
    pd_identity_entry[pmm_addr_to_page(addr)] =
        addr | PAGE_PRESENT | PAGE_WRITABLE;
  }

  haddr_t* pd_kernel_entry = (haddr_t*)pmm_alloc_frame();
  memset(pd_kernel_entry, 0, 4096);
  haddr_t kernel_page_count = pmm_state_get_kernel_page_count(g_kernel.pmm);

  check_kernel_size(kernel_page_count);

  haddr_t kernel_start = pmm_state_get_kernel_start(g_kernel.pmm);
  haddr_t kernel_end = pmm_state_get_kernel_end(g_kernel.pmm);
  for (haddr_t addr = kernel_start; addr <= kernel_end; ++addr) {
    pd_identity_entry[pmm_addr_to_page(addr)] =
        addr | PAGE_PRESENT | PAGE_WRITABLE;
  }

  page_directory[0] = ((haddr_t)pd_identity_entry) | 0x03;
  page_directory[1023] = (haddr_t)page_directory | 0x03;
  load_pd(page_directory);
  enable_paging();

  virtual_directory = (haddr_t*)VIRT_PD_START;

  vmm.first_available_vaddr = kernel_end + PAGE_SIZE;
  vmm.last_available_vaddr = idx_to_vaddr(1022, 1023);
  g_kernel.vmm = &vmm;
}

haddr_t vmm_map_single(haddr_t virt, haddr_t flags) {
  haddr_t virt_base = virt & 0xFFFFF000;
  haddr_t new_page = pmm_alloc_frame();
  // OOM
  if (new_page == 0) {
    return 0;
  }

  uint16_t directory_idx = virt_to_directory_idx(virt);
  uint16_t entry_idx = virt_to_entry_idx(virt);

  haddr_t* pd_entry;
  if (virtual_directory[directory_idx] & PAGE_PRESENT) {
    pd_entry = get_page_table(directory_idx);
    pd_entry[entry_idx] = new_page | PAGE_PRESENT | PAGE_WRITABLE;
  } else {
    haddr_t* pd_entry = (haddr_t*)pmm_alloc_frame();
    virtual_directory[directory_idx] =
        (haddr_t)pd_entry | PAGE_PRESENT | PAGE_WRITABLE;
    pd_entry[entry_idx] = new_page | PAGE_PRESENT | PAGE_WRITABLE;
  }
  _invlpg(virt_base);
  return virt_base;
}

haddr_t vmm_map(haddr_t virt, haddr_t size, haddr_t flags) {
  haddr_t base = virt;
  while (size-- > 0) {
    haddr_t res = vmm_map_single(virt, flags);
    //  OOM
    if (res == 0) {
      return res;
    }
    virt += PAGE_SIZE;
  }
  haddr_t virt_base = base & 0xFFFFF000;
  return virt_base;
}

haddr_t vmm_unmap(haddr_t virt) {
  uint16_t directory_idx = virt_to_directory_idx(virt);

  haddr_t* pd_entry = get_page_table(directory_idx);
  if (virtual_directory[directory_idx] == 0) {
    // Don't need to free something that doesn't exist
    return 0;
  }
  uint16_t entry_idx = virt_to_entry_idx(virt);
  pd_entry[entry_idx] = 0;
  haddr_t virt_base = virt & 0xFFFFF000;
  _invlpg(virt_base);
  return virt_base;
}

haddr_t vmm_to_physical(haddr_t virt) {
  uint16_t directory_idx = virt_to_directory_idx(virt);
  haddr_t* pd_entry;

  if (page_directory[directory_idx] == 0) {
    return 0;
  } else {
    pd_entry = (haddr_t*)page_directory[directory_idx];
  }

  uint16_t entry_idx = virt_to_entry_idx(virt);
  haddr_t phys_base = pd_entry[entry_idx] | 0xFFFFF000;  // Clear flags
  return phys_base;
}

void check_kernel_size(haddr_t kernel_page_count) {
  if (kernel_page_count > 1023 - 256) {
    // TODO: we need support for more than one PDE for the kernel
    printf(
        "Kernels requiring more than one page directory entry are not yet "
        "supported. Halt.\n");
    abort();
  }
}

haddr_t* get_page_table(haddr_t idx) {
  return (haddr_t*)VIRT_PT_START + (idx << 12);
}

haddr_t idx_to_vaddr(haddr_t directory_idx, haddr_t entry_idx) {
  return (directory_idx << 22) | (entry_idx << 12);
}

uint16_t virt_to_directory_idx(haddr_t virt) { return virt >> 22; }

uint16_t virt_to_entry_idx(haddr_t virt) {
  return (virt >> 12) & 0b1111111111;
}

void _invlpg(haddr_t virt) {
  asm volatile("invlpg (%0)" ::"r"(virt) : "memory");
}

