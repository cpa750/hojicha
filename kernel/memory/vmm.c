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
#define KERNEL_VOFFSET 0xFFFFFFFF80000000ULL
#define VIRT_PT_START 0
#define RECURSIVE_IDX 510ULL
#define PD_ENTRIES 511ULL
#define CANONICAL_HIGH 0xFFFFULL << 48
#define MAPPING_STRUCTURE_MASK 0xFFFFFFFFFFFFF000ULL
#define LAST_VADDR 0xFFFF7AAAAAAAAAAA

__attribute__((
    used,
    section(".limine_requests"))) static volatile struct limine_hhdm_request
    hhdm_request = {.id = LIMINE_HHDM_REQUEST, .revision = 0};

struct vmm_state {
  haddr_t first_available_vaddr;
  haddr_t last_available_vaddr;
  haddr_t kernel_offset;
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

haddr_t* get_pml4_entry(haddr_t virt);
haddr_t* get_pml3_entry(haddr_t virt);
haddr_t* get_pd_entry(haddr_t virt);
haddr_t* get_pt_entry(haddr_t virt);
uint16_t get_pml4_idx(haddr_t virt);
uint16_t get_pml3_idx(haddr_t virt);
uint16_t get_pd_idx(haddr_t virt);
uint16_t get_pt_idx(haddr_t virt);

void map_framebuffer();

void _invlpg(haddr_t virt);

haddr_t* page_directory;
haddr_t* virtual_directory;

static haddr_t pml4[512] __attribute__((aligned(0x1000)));
static haddr_t pml3[512] __attribute__((aligned(0x1000)));
static haddr_t pd[512] __attribute__((aligned(0x1000)));
static haddr_t low_identity_pde[512] __attribute__((aligned(0x1000)));
static haddr_t kernel_pde[512] __attribute__((aligned(0x1000)));
static vmm_state_t vmm = {0};

void initialize_vmm() {
  struct limine_hhdm_response* hhdm = hhdm_request.response;
  vmm.kernel_offset = hhdm->offset;

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

  haddr_t kernel_pstart = pmm_state_get_kernel_start(g_kernel.pmm);
  haddr_t kernel_vstart = pmm_state_get_kernel_vstart(g_kernel.pmm);
  pml4[RECURSIVE_IDX] =
      (kernel_pstart + ((haddr_t)pml4 - kernel_vstart)) | 0x03;
  haddr_t pml4_kernel_entry = kernel_pstart + ((haddr_t)pml3 - kernel_vstart);
  haddr_t pml3_kernel_entry = kernel_pstart + ((haddr_t)pd - kernel_vstart);
  haddr_t pd_kernel_entry =
      kernel_pstart + ((haddr_t)kernel_pde - kernel_vstart);

  pml4[511] = pml4_kernel_entry | 0x03;
  pml3[510] = pml3_kernel_entry | 0x03;
  pd[0] = pd_kernel_entry | 0x03;

  virtual_directory = pml4;

  haddr_t pml4_physical = kernel_pstart + ((haddr_t)pml4 - kernel_vstart);

  load_pd((haddr_t*)pml4_physical);

  vmm_map(0xFFFFFEDCBA987000, 1, PAGE_PRESENT | PAGE_WRITABLE);
  uint64_t* test = (uint64_t*)0xFFFFFEDCBA987000;
  *test = 0xABCD;

  map_framebuffer();
  pmm_initialize_bitmap();

  vmm.first_available_vaddr = 0;
  vmm.last_available_vaddr = LAST_VADDR;
  g_kernel.vmm = &vmm;
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
  haddr_t virt_base = base & MAPPING_STRUCTURE_MASK;
  return virt_base;
}

haddr_t vmm_map_single(haddr_t virt, haddr_t flags) {
  haddr_t new_page = pmm_alloc_frame();
  // OOM
  if (new_page == 0) {
    return 0;
  }
  return vmm_map_at_paddr(virt, new_page, flags);
}

haddr_t vmm_map_at_paddr(haddr_t virt, haddr_t phys, haddr_t flags) {
  haddr_t virt_base = virt & MAPPING_STRUCTURE_MASK;

  haddr_t* pml3 = get_pml4_entry(virt);
  uint16_t pml4_idx = get_pml4_idx(virt);
  if (pml4_idx == RECURSIVE_IDX) {
    return 0;
  }
  if (!(virtual_directory[pml4_idx] & (PAGE_PRESENT | PAGE_WRITABLE))) {
    virtual_directory[pml4_idx] =
        pmm_alloc_frame() | PAGE_PRESENT | PAGE_WRITABLE;
  }

  haddr_t* pd = get_pml3_entry(virt);
  uint16_t pml3_idx = get_pml3_idx(virt);
  if (!(pml3[pml3_idx] & (PAGE_PRESENT | PAGE_WRITABLE))) {
    pml3[pml3_idx] = pmm_alloc_frame() | PAGE_PRESENT | PAGE_WRITABLE;
  }

  haddr_t* pt = get_pd_entry(virt);
  uint16_t pd_idx = get_pd_idx(virt);
  if (!(pd[pd_idx] & (PAGE_PRESENT | PAGE_WRITABLE))) {
    pd[pd_idx] = pmm_alloc_frame() | PAGE_PRESENT | PAGE_WRITABLE;
  }

  uint16_t pt_idx = get_pt_idx(virt);
  pt[pt_idx] = phys | flags;

  _invlpg(virt);
  return virt_base;
}

haddr_t vmm_unmap(haddr_t virt) {
  haddr_t virt_base = virt & MAPPING_STRUCTURE_MASK;

  haddr_t* pml3 = get_pml4_entry(virt);
  if (!((haddr_t)pml3 & (PAGE_PRESENT | PAGE_WRITABLE))) {
    uint16_t pml4_idx = get_pml4_idx(virt);
    if (pml4_idx == RECURSIVE_IDX) {
      return 0;
    }
  }

  haddr_t* pd = get_pml3_entry(virt);
  if (!((haddr_t)pd & (PAGE_PRESENT | PAGE_WRITABLE))) {
    return 0;
  }

  haddr_t* pt = get_pd_entry(virt);
  if (!((haddr_t)pt & (PAGE_PRESENT | PAGE_WRITABLE))) {
    return 0;
  }

  uint16_t pt_idx = get_pt_idx(virt);
  pt[pt_idx] = 0;

  _invlpg(virt_base);
  return virt_base;
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

haddr_t* get_pml4_entry(haddr_t virt) {
  uint32_t pml4_idx = get_pml4_idx(virt);

  haddr_t entry = CANONICAL_HIGH | (RECURSIVE_IDX << 39) |
                  (RECURSIVE_IDX << 30) | (RECURSIVE_IDX << 21) |
                  (pml4_idx << 12);
  return (haddr_t*)entry;
}

haddr_t* get_pml3_entry(haddr_t virt) {
  uint32_t pml4_idx = get_pml4_idx(virt);
  uint32_t pml3_idx = get_pml3_idx(virt);

  haddr_t entry = CANONICAL_HIGH | (RECURSIVE_IDX << 39) |
                  (RECURSIVE_IDX << 30) | (pml4_idx << 21) | (pml3_idx << 12);
  return (haddr_t*)entry;
}

haddr_t* get_pd_entry(haddr_t virt) {
  uint64_t pml4_idx = get_pml4_idx(virt);
  uint32_t pml3_idx = get_pml3_idx(virt);
  uint32_t pd_idx = get_pd_idx(virt);

  haddr_t entry = (CANONICAL_HIGH | (RECURSIVE_IDX << 39) | (pml4_idx << 30) |
                   (pml3_idx << 21) | (pd_idx << 12));
  return (haddr_t*)entry;
}

haddr_t* get_pt_entry(haddr_t virt) {
  uint16_t pd_idx = get_pt_idx(virt);

  haddr_t* pt = get_pd_entry(virt);
  return (haddr_t*)pt[pd_idx];
}

uint16_t get_pml4_idx(haddr_t virt) { return (virt >> 39) & PD_ENTRIES; }
uint16_t get_pml3_idx(haddr_t virt) { return (virt >> 30) & PD_ENTRIES; }
uint16_t get_pd_idx(haddr_t virt) { return (virt >> 21) & PD_ENTRIES; }
uint16_t get_pt_idx(haddr_t virt) { return (virt >> 12) & PD_ENTRIES; }

haddr_t* get_page_table(haddr_t idx) {
  return (haddr_t*)VIRT_PT_START + (idx << 12);
}

haddr_t idx_to_vaddr(haddr_t directory_idx, haddr_t entry_idx) {
  return (directory_idx << 22) | (entry_idx << 12);
}

uint16_t virt_to_directory_idx(haddr_t virt) { return virt >> 22; }

uint16_t virt_to_entry_idx(haddr_t virt) { return (virt >> 12) & 0b1111111111; }

void map_framebuffer() {
  haddr_t framebuffer_pstart =
      (haddr_t)vga_state_get_framebuffer_addr(g_kernel.vga) - vmm.kernel_offset;
  haddr_t framebuffer_pend =
      (haddr_t)vga_state_get_framebuffer_end(g_kernel.vga) - vmm.kernel_offset;
  uint64_t framebuffer_offset = 0;
  for (haddr_t addr = framebuffer_pstart; addr <= framebuffer_pend + 4096;
       addr += 4096) {
    vmm_map_at_paddr((haddr_t)vga_state_get_framebuffer_addr(g_kernel.vga) +
                         framebuffer_offset,
                     addr, PAGE_PRESENT | PAGE_WRITABLE);
    framebuffer_offset += 4096;
  }
}

void _invlpg(haddr_t virt) {
  asm volatile("invlpg (%0)" ::"r"(virt) : "memory");
}

