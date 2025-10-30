#include <drivers/vga.h>
#include <kernel/kernel_state.h>
#include <limine.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 4096
#define KERNEL_VOFFSET 0xFFFFFFFF80000000
#define VIRT_PT_START 0

__attribute__((
    used,
    section(".limine_requests"))) static volatile struct limine_hhdm_request
    hhdm_request = {.id = LIMINE_HHDM_REQUEST, .revision = 0};

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

extern void load_pd(haddr_t* pd_addr);

void check_kernel_size(haddr_t kernel_page_count);
haddr_t* get_page_table(haddr_t idx);
haddr_t idx_to_vaddr(haddr_t directory_idx, haddr_t entry_idx);
uint16_t virt_to_directory_idx(haddr_t virt);
uint16_t virt_to_entry_idx(haddr_t virt);
void _invlpg(haddr_t virt);

haddr_t* page_directory;
haddr_t* virtual_directory;

static haddr_t pml4[512] __attribute__((aligned(0x1000)));
static haddr_t pml3[512] __attribute__((aligned(0x1000)));
static haddr_t pml3_framebuffer[512] __attribute__((aligned(0x1000)));
static haddr_t pd[512] __attribute__((aligned(0x1000)));
static haddr_t pd_framebuffer[512] __attribute__((aligned(0x1000)));
static haddr_t low_identity_pde[512] __attribute__((aligned(0x1000)));
static haddr_t framebuffer_pde_low[512] __attribute__((aligned(0x1000)));
static haddr_t framebuffer_pde_high[512] __attribute__((aligned(0x1000)));
static haddr_t kernel_pde[512] __attribute__((aligned(0x1000)));

void initialize_vmm() {
  struct limine_hhdm_response* hhdm = hhdm_request.response;
  uint64_t offset = hhdm->offset;

  static vmm_state_t vmm = {0};
  memset(pml4, 0, 4096);
  memset(pml3, 0, 4096);
  memset(pd, 0, 4096);
  memset(low_identity_pde, 0, 4096);
  memset(kernel_pde, 0, 4096);

  for (haddr_t addr = 0; addr < 0x100000; addr += 4096) {
    low_identity_pde[pmm_addr_to_page(addr)] =
        addr | PAGE_PRESENT | PAGE_WRITABLE;
  }

  haddr_t kernel_page_count = pmm_state_get_kernel_page_count(g_kernel.pmm);

  check_kernel_size(kernel_page_count);

  haddr_t kernel_start = pmm_state_get_kernel_start(g_kernel.pmm);
  haddr_t kernel_end = pmm_state_get_kernel_end(g_kernel.pmm);
  uint64_t kernel_page = 0;
  for (haddr_t addr = kernel_start; addr <= kernel_end; addr += 4096) {
    kernel_pde[kernel_page++] = addr | PAGE_PRESENT | PAGE_WRITABLE;
  }

  // pml4[0] = ((haddr_t)low_identity_pde - KERNEL_VOFFSET) | 0x03;
  haddr_t kernel_pstart = pmm_state_get_kernel_start(g_kernel.pmm);
  haddr_t kernel_vstart = pmm_state_get_kernel_vstart(g_kernel.pmm);
  pml4[510] = kernel_pstart + ((haddr_t)pml4 - kernel_vstart) | 0x03;
  haddr_t pml4_kernel_entry = kernel_pstart + ((haddr_t)pml3 - kernel_vstart);
  haddr_t pml3_kernel_entry = kernel_pstart + ((haddr_t)pd - kernel_vstart);
  haddr_t pd_kernel_entry =
      kernel_pstart + ((haddr_t)kernel_pde - kernel_vstart);

  pml4[511] = pml4_kernel_entry | 0x03;
  pml3[510] = pml3_kernel_entry | 0x03;
  pd[0] = pd_kernel_entry | 0x03;

  virtual_directory = pml4;

  haddr_t pml4_physical = kernel_pstart + ((haddr_t)pml4 - kernel_vstart);

  // TODO map the framebuffer here
  haddr_t framebuffer_pstart =
      (haddr_t)vga_state_get_framebuffer_addr(g_kernel.vga) - offset;
  haddr_t framebuffer_pend =
      (haddr_t)vga_state_get_framebuffer_end(g_kernel.vga) - offset;
  uint64_t framebuffer_page = 0;
  for (haddr_t addr = framebuffer_pstart; addr <= framebuffer_pend + 4096;
       addr += 4096) {
    if (framebuffer_page < 512) {
      framebuffer_pde_low[framebuffer_page++] =
          addr | PAGE_PRESENT | PAGE_WRITABLE;
    } else {
      framebuffer_pde_high[framebuffer_page - 512] =
          addr | PAGE_PRESENT | PAGE_WRITABLE;
      ++framebuffer_page;
    }
  }
  haddr_t pml4_framebuffer_entry =
      kernel_pstart + ((haddr_t)pml3_framebuffer - kernel_vstart);
  haddr_t pml3_framebuffer_entry =
      kernel_pstart + ((haddr_t)pd_framebuffer - kernel_vstart);
  haddr_t pd_framebuffer_entry_low =
      kernel_pstart + ((haddr_t)framebuffer_pde_low - kernel_vstart);
  haddr_t pd_framebuffer_entry_high =
      kernel_pstart + ((haddr_t)framebuffer_pde_high - kernel_vstart);
  pml4[256] = pml4_framebuffer_entry | 0x03;
  pml3_framebuffer[3] = pml3_framebuffer_entry | 0x03;
  pd_framebuffer[488] = pd_framebuffer_entry_low | 0x03;
  pd_framebuffer[489] = pd_framebuffer_entry_high | 0x03;

  load_pd((haddr_t*)pml4_physical);

  vmm.first_available_vaddr = kernel_end + PAGE_SIZE;
  vmm.last_available_vaddr = idx_to_vaddr(1022, 1023);
  g_kernel.vmm = &vmm;
}

haddr_t vmm_map_at_paddr(haddr_t virt, haddr_t phys, haddr_t flags) {
  haddr_t virt_base = virt & 0xFFFFFFFFFFFFF000;
  uint16_t directory_idx = virt_to_directory_idx(virt);
  uint16_t entry_idx = virt_to_entry_idx(virt);

  haddr_t* pd_entry;
  if (virtual_directory[directory_idx] & PAGE_PRESENT) {
    pd_entry = get_page_table(directory_idx);
    pd_entry[entry_idx] = phys | PAGE_PRESENT | PAGE_WRITABLE;
  } else {
    haddr_t* pd_entry = (haddr_t*)pmm_alloc_frame();
    virtual_directory[directory_idx] =
        (haddr_t)pd_entry | PAGE_PRESENT | PAGE_WRITABLE;
    pd_entry[entry_idx] = phys | PAGE_PRESENT | PAGE_WRITABLE;
  }
  _invlpg(virt_base);
  return virt_base;
}

haddr_t vmm_map_single(haddr_t virt, haddr_t flags) {
  haddr_t new_page = pmm_alloc_frame();
  // OOM
  if (new_page == 0) {
    return 0;
  }
  return vmm_map_at_paddr(int virt, int phys, int flags);
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
  haddr_t virt_base = base & 0xFFFFFFFFFFFFF000;
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
  haddr_t virt_base = virt & 0xFFFFFFFFFFFFF000;
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
  haddr_t phys_base = pd_entry[entry_idx] | 0xFFFFFFFFFFFFF000;  // Clear flags
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

uint16_t virt_to_entry_idx(haddr_t virt) { return (virt >> 12) & 0b1111111111; }

void _invlpg(haddr_t virt) {
  asm volatile("invlpg (%0)" ::"r"(virt) : "memory");
}

