#include <drivers/vga.h>
#include <haddr.h>
#include <kernel/kernel_state.h>
#include <limine.h>
#include <memory/kmalloc.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CANONICAL_HIGH         0xFFFFULL << 48
#define KERNEL_VOFFSET         0xFFFFFFFF80000000ULL
#define LAST_VADDR             0xFFFF7AAAAAAAAAAA
#define LOW_REGION_END         0x100000
#define PAGE_SIZE              4096
#define MAPPING_STRUCTURE_MASK 0xFFFFFFFFFFFFF000ULL
#define PD_ENTRIES             511ULL
#define RECURSIVE_IDX          510ULL
#define VIRT_PT_START          0
#define VIRT_SCRATCH_ADDR      LOW_REGION_END
#define VIRT_PML4              VIRT_SCRATCH_ADDR + 0x1000
#define VIRT_SCRATCH_L4        VIRT_PML4 + 0x1000
#define VIRT_SCRATCH_L3        VIRT_SCRATCH_L4 + 0x1000
#define VIRT_SCRATCH_L2        VIRT_SCRATCH_L3 + 0x1000
#define VIRT_SCRATCH_L1        VIRT_SCRATCH_L2 + 0x1000

__attribute__((
    used,
    section(".limine_requests"))) static volatile struct limine_hhdm_request
    hhdm_request = {.id = LIMINE_HHDM_REQUEST, .revision = 0};

struct vmm {
  haddr_t first_available_vaddr;
  haddr_t last_available_vaddr;
  haddr_t kernel_offset;

  haddr_t* pml4;
};
typedef struct vmm vmm_t;
haddr_t vmm_get_kernel_offset(vmm_t* vmm) { return vmm->kernel_offset; }
haddr_t vmm_get_first_available_vaddr(vmm_t* vmm) {
  return vmm->first_available_vaddr;
}
haddr_t vmm_get_last_available_vaddr(vmm_t* vmm) {
  return vmm->last_available_vaddr;
}
void vmm_dump(vmm_t* v) {
  printf("[VMM] Struct addr:\t\t\t\t%x B\n", (haddr_t)v);
  printf("[VMM] First available vaddr:\t%x\n", v->first_available_vaddr);
  printf("[VMM] Last available vaddr:\t\t%x\n", v->last_available_vaddr);
}

extern void load_pd(haddr_t* pd_addr);

void check_kernel_size(haddr_t kernel_page_count);

haddr_t* get_pml4_entry(haddr_t virt);
haddr_t* get_pml3_entry(haddr_t virt);
haddr_t* get_pd_entry(haddr_t virt);
haddr_t* get_pt_entry(haddr_t virt);
uint16_t get_pml4_idx(haddr_t virt);
uint16_t get_pml3_idx(haddr_t virt);
uint16_t get_pd_idx(haddr_t virt);
uint16_t get_pt_idx(haddr_t virt);

void map_framebuffer(vmm_t* vmm);
static haddr_t map_in_current_cr3(haddr_t virt, haddr_t phys, haddr_t flags);
static bool entry_is_present(haddr_t entry);
static haddr_t entry_phys(haddr_t entry);
static haddr_t* scratch_map_table(haddr_t scratch_addr, haddr_t phys);
static haddr_t* walk_non_kernel_to_pt(vmm_t* vmm, haddr_t virt, bool create);

void _invlpg(haddr_t virt);

haddr_t* page_directory;
haddr_t* virtual_directory;

static haddr_t bootstrap_pml4[512] __attribute__((aligned(0x1000)));
static haddr_t bootstrap_pml3[512] __attribute__((aligned(0x1000)));
static haddr_t bootstrap_pd[512] __attribute__((aligned(0x1000)));
static haddr_t low_identity_pde[512] __attribute__((aligned(0x1000)));
static haddr_t kernel_pde[512] __attribute__((aligned(0x1000)));
static vmm_t kernel_vmm = {0};

void initialize_vmm() {
  struct limine_hhdm_response* hhdm = hhdm_request.response;
  kernel_vmm.kernel_offset = hhdm->offset;

  memset(bootstrap_pml4, 0, 4096);
  memset(bootstrap_pml3, 0, 4096);
  memset(bootstrap_pd, 0, 4096);
  memset(low_identity_pde, 0, 4096);
  memset(kernel_pde, 0, 4096);

  for (haddr_t addr = 0; addr < LOW_REGION_END; addr += 4096) {
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
  bootstrap_pml4[RECURSIVE_IDX] =
      (kernel_pstart + ((haddr_t)bootstrap_pml4 - kernel_vstart)) | 0x03;
  haddr_t pml4_kernel_entry =
      kernel_pstart + ((haddr_t)bootstrap_pml3 - kernel_vstart);
  haddr_t pml3_kernel_entry =
      kernel_pstart + ((haddr_t)bootstrap_pd - kernel_vstart);
  haddr_t pd_kernel_entry =
      kernel_pstart + ((haddr_t)kernel_pde - kernel_vstart);

  bootstrap_pml4[511] = pml4_kernel_entry | 0x03;
  bootstrap_pml3[510] = pml3_kernel_entry | 0x03;
  bootstrap_pd[0] = pd_kernel_entry | 0x03;

  virtual_directory = bootstrap_pml4;

  haddr_t pml4_physical =
      kernel_pstart + ((haddr_t)bootstrap_pml4 - kernel_vstart);

  load_pd((haddr_t*)pml4_physical);

  g_kernel.vmm = &kernel_vmm;
  kernel_vmm.pml4 = bootstrap_pml4;
  map_framebuffer(&kernel_vmm);
  pmm_initialize_bitmap();

  kernel_vmm.first_available_vaddr =
      pmm_page_to_addr_base(pmm_addr_to_page(LOW_REGION_END + PAGE_SIZE));
  kernel_vmm.last_available_vaddr = LAST_VADDR;
  kernel_vmm.pml4 = bootstrap_pml4;
}

vmm_t* vmm_new(haddr_t flags) {
  vmm_t* vmm = (vmm_t*)malloc(sizeof(vmm_t));

  haddr_t* pml4 = (haddr_t*)pmm_alloc_frame();
  haddr_t* kernel_pml3 = (haddr_t*)pmm_alloc_frame();
  haddr_t* kernel_pd = (haddr_t*)pmm_alloc_frame();
  haddr_t* kernel_pde = (haddr_t*)pmm_alloc_frame();
  haddr_t* low_identity_pde = (haddr_t*)pmm_alloc_frame();

  vmm_map_at_paddr(g_kernel.vmm,
                   VIRT_SCRATCH_ADDR,
                   (haddr_t)low_identity_pde,
                   PAGE_PRESENT | PAGE_WRITABLE);
  _invlpg(VIRT_SCRATCH_ADDR);
  for (haddr_t addr = 0; addr < LOW_REGION_END; addr += 4096) {
    low_identity_pde[pmm_addr_to_page(addr)] =
        addr | PAGE_PRESENT | PAGE_WRITABLE;
  }

  haddr_t kernel_page_count = pmm_state_get_kernel_page_count(g_kernel.pmm);

  check_kernel_size(kernel_page_count);

  haddr_t kernel_start = pmm_state_get_kernel_start(g_kernel.pmm);
  haddr_t kernel_end = pmm_state_get_kernel_end(g_kernel.pmm);
  vmm->kernel_offset = g_kernel.vmm->kernel_offset;

  vmm_map_at_paddr(g_kernel.vmm,
                   VIRT_SCRATCH_ADDR,
                   (haddr_t)kernel_pde,
                   PAGE_PRESENT | PAGE_WRITABLE);
  _invlpg(VIRT_SCRATCH_ADDR);
  uint64_t kernel_page = 0;
  for (haddr_t addr = kernel_start; addr <= kernel_end; addr += 4096) {
    ((haddr_t*)VIRT_SCRATCH_ADDR)[kernel_page++] =
        addr | PAGE_PRESENT | PAGE_WRITABLE;
  }

  vmm_map_at_paddr(g_kernel.vmm,
                   VIRT_SCRATCH_ADDR,
                   (haddr_t)pml4,
                   PAGE_PRESENT | PAGE_WRITABLE);
  _invlpg(VIRT_SCRATCH_ADDR);
  ((haddr_t*)VIRT_SCRATCH_ADDR)[RECURSIVE_IDX] =
      (haddr_t)pml4 | PAGE_PRESENT | PAGE_WRITABLE | flags;
  ((haddr_t*)VIRT_SCRATCH_ADDR)[511] =
      (haddr_t)kernel_pml3 | PAGE_PRESENT | PAGE_WRITABLE | flags;

  vmm_map_at_paddr(g_kernel.vmm,
                   VIRT_SCRATCH_ADDR,
                   (haddr_t)kernel_pml3,
                   PAGE_PRESENT | PAGE_WRITABLE);
  _invlpg(VIRT_SCRATCH_ADDR);
  ((haddr_t*)VIRT_SCRATCH_ADDR)[510] =
      (haddr_t)kernel_pd | PAGE_PRESENT | PAGE_WRITABLE | flags;

  vmm_map_at_paddr(g_kernel.vmm,
                   VIRT_SCRATCH_ADDR,
                   (haddr_t)kernel_pd,
                   PAGE_PRESENT | PAGE_WRITABLE);
  _invlpg(VIRT_SCRATCH_ADDR);
  ((haddr_t*)VIRT_SCRATCH_ADDR)[0] =
      (haddr_t)kernel_pde | PAGE_PRESENT | PAGE_WRITABLE | flags;

  map_framebuffer(vmm);
  vmm_map_at_paddr(
      vmm, VIRT_PML4, (haddr_t)pml4, PAGE_PRESENT | PAGE_WRITABLE | flags);

  vmm->first_available_vaddr =
      pmm_page_to_addr_base(pmm_addr_to_page(LOW_REGION_END + PAGE_SIZE));
  vmm->last_available_vaddr = LAST_VADDR;
  vmm->pml4 = pml4;
  return vmm;
}

haddr_t vmm_map(vmm_t* vmm, haddr_t virt, haddr_t size, haddr_t flags) {
  haddr_t base = virt;
  while (size-- > 0) {
    haddr_t res = vmm_map_single(vmm, virt, flags);
    //  OOM
    if (res == 0) { return res; }
    virt += PAGE_SIZE;
  }
  haddr_t virt_base = base & MAPPING_STRUCTURE_MASK;
  return virt_base;
}

haddr_t vmm_map_single(vmm_t* vmm, haddr_t virt, haddr_t flags) {
  haddr_t new_page = pmm_alloc_frame();
  // OOM
  if (new_page == 0) { return 0; }
  return vmm_map_at_paddr(vmm, virt, new_page, flags);
}

haddr_t vmm_map_at_paddr(vmm_t* vmm,
                         haddr_t virt,
                         haddr_t phys,
                         haddr_t flags) {
  haddr_t virt_base = virt & MAPPING_STRUCTURE_MASK;
  uint16_t pml4_idx = get_pml4_idx(virt);
  if (pml4_idx == RECURSIVE_IDX) { return 0; }

  if (vmm != g_kernel.vmm) {
    haddr_t* pt = walk_non_kernel_to_pt(vmm, virt, true);
    if (pt == NULL) { return 0; }
    uint16_t pt_idx = get_pt_idx(virt);
    pt[pt_idx] = phys | flags;
    return virt_base;
  }

  haddr_t* pml3 = get_pml4_entry(virt);
  if (!entry_is_present(virtual_directory[pml4_idx])) {
    haddr_t new_pml3_phys = pmm_alloc_frame();
    if (new_pml3_phys == 0) { return 0; }
    virtual_directory[pml4_idx] = new_pml3_phys | PAGE_PRESENT | PAGE_WRITABLE;
    memset(pml3, 0, PAGE_SIZE);
  }

  haddr_t* pd = get_pml3_entry(virt);
  uint16_t pml3_idx = get_pml3_idx(virt);
  if (!entry_is_present(pml3[pml3_idx])) {
    haddr_t new_pd_phys = pmm_alloc_frame();
    if (new_pd_phys == 0) { return 0; }
    pml3[pml3_idx] = new_pd_phys | PAGE_PRESENT | PAGE_WRITABLE;
    memset(pd, 0, PAGE_SIZE);
  }

  haddr_t* pt = get_pd_entry(virt);
  uint16_t pd_idx = get_pd_idx(virt);
  if (!entry_is_present(pd[pd_idx])) {
    haddr_t new_pt_phys = pmm_alloc_frame();
    if (new_pt_phys == 0) { return 0; }
    pd[pd_idx] = new_pt_phys | PAGE_PRESENT | PAGE_WRITABLE;
    memset(pt, 0, PAGE_SIZE);
  }

  uint16_t pt_idx = get_pt_idx(virt);
  pt[pt_idx] = phys | flags;
  _invlpg(virt);
  return virt_base;
}

haddr_t vmm_unmap(vmm_t* vmm, haddr_t virt) {
  haddr_t virt_base = virt & MAPPING_STRUCTURE_MASK;
  uint16_t pml4_idx = get_pml4_idx(virt);
  if (pml4_idx == RECURSIVE_IDX) { return 0; }

  if (vmm != g_kernel.vmm) {
    haddr_t* pt = walk_non_kernel_to_pt(vmm, virt, false);
    if (pt == NULL) { return 0; }
    uint16_t pt_idx = get_pt_idx(virt);
    pt[pt_idx] = 0;
    return virt_base;
  }

  if (!entry_is_present(virtual_directory[pml4_idx])) { return 0; }
  haddr_t* pml3 = get_pml4_entry(virt);
  uint16_t pml3_idx = get_pml3_idx(virt);
  if (!entry_is_present(pml3[pml3_idx])) { return 0; }
  haddr_t* pd = get_pml3_entry(virt);

  uint16_t pd_idx = get_pd_idx(virt);
  if (!entry_is_present(pd[pd_idx])) { return 0; }
  haddr_t* pt = get_pd_entry(virt);

  uint16_t pt_idx = get_pt_idx(virt);
  pt[pt_idx] = 0;
  _invlpg(virt_base);
  return virt_base;
}

void check_kernel_size(haddr_t kernel_page_count) {
  if (kernel_page_count > 1023 - 256) {
    // TODO: we need support for more than one PDE for the kernel
    printf("Kernels requiring more than one page directory entry are not yet "
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

uint16_t virt_to_directory_idx(haddr_t virt) { return virt >> 22; }

uint16_t virt_to_entry_idx(haddr_t virt) { return (virt >> 12) & 0b1111111111; }

void map_framebuffer(vmm_t* vmm) {
  haddr_t framebuffer_pstart =
      (haddr_t)vga_state_get_framebuffer_addr(g_kernel.vga) -
      vmm->kernel_offset;
  haddr_t framebuffer_pend =
      (haddr_t)vga_state_get_framebuffer_end(g_kernel.vga) - vmm->kernel_offset;
  uint64_t framebuffer_offset = 0;
  for (haddr_t addr = framebuffer_pstart; addr <= framebuffer_pend + 4096;
       addr += 4096) {
    map_in_current_cr3((haddr_t)vga_state_get_framebuffer_addr(g_kernel.vga) +
                           framebuffer_offset,
                       addr,
                       PAGE_PRESENT | PAGE_WRITABLE);
    framebuffer_offset += 4096;
  }
}

/*
 * Helper function to map a scratch page table
 */
static haddr_t map_in_current_cr3(haddr_t virt, haddr_t phys, haddr_t flags) {
  haddr_t virt_base = virt & MAPPING_STRUCTURE_MASK;

  haddr_t* pml3 = get_pml4_entry(virt);
  uint16_t pml4_idx = get_pml4_idx(virt);
  if (pml4_idx == RECURSIVE_IDX) { return 0; }
  if (!(virtual_directory[pml4_idx] & (PAGE_PRESENT | PAGE_WRITABLE))) {
    virtual_directory[pml4_idx] =
        pmm_alloc_frame() | PAGE_PRESENT | PAGE_WRITABLE;
    memset(pml3, 0, PAGE_SIZE);
  }

  haddr_t* pd = get_pml3_entry(virt);
  uint16_t pml3_idx = get_pml3_idx(virt);
  if (!(pml3[pml3_idx] & (PAGE_PRESENT | PAGE_WRITABLE))) {
    pml3[pml3_idx] = pmm_alloc_frame() | PAGE_PRESENT | PAGE_WRITABLE;
    memset(pd, 0, PAGE_SIZE);
  }

  haddr_t* pt = get_pd_entry(virt);
  uint16_t pd_idx = get_pd_idx(virt);
  if (!(pd[pd_idx] & (PAGE_PRESENT | PAGE_WRITABLE))) {
    pd[pd_idx] = pmm_alloc_frame() | PAGE_PRESENT | PAGE_WRITABLE;
    memset(pt, 0, PAGE_SIZE);
  }

  uint16_t pt_idx = get_pt_idx(virt);
  pt[pt_idx] = phys | flags;

  _invlpg(virt);
  return virt_base;
}

static bool entry_is_present(haddr_t entry) {
  return (entry & (PAGE_PRESENT | PAGE_WRITABLE)) ==
         (PAGE_PRESENT | PAGE_WRITABLE);
}

static haddr_t entry_phys(haddr_t entry) {
  return entry & MAPPING_STRUCTURE_MASK;
}

static haddr_t* scratch_map_table(haddr_t scratch_addr, haddr_t phys) {
  if (map_in_current_cr3(scratch_addr, phys, PAGE_PRESENT | PAGE_WRITABLE) ==
      0) {
    return NULL;
  }
  _invlpg(scratch_addr);
  return (haddr_t*)scratch_addr;
}

static haddr_t* walk_non_kernel_to_pt(vmm_t* vmm, haddr_t virt, bool create) {
  uint16_t pml4_idx = get_pml4_idx(virt);
  uint16_t pml3_idx = get_pml3_idx(virt);
  uint16_t pd_idx = get_pd_idx(virt);

  if (pml4_idx == RECURSIVE_IDX) { return NULL; }

  haddr_t* pml4 = scratch_map_table(VIRT_SCRATCH_L4, (haddr_t)vmm->pml4);
  if (pml4 == NULL) { return NULL; }

  if (!entry_is_present(pml4[pml4_idx])) {
    if (!create) { return NULL; }
    haddr_t new_pml3_phys = pmm_alloc_frame();
    if (new_pml3_phys == 0) { return NULL; }
    pml4[pml4_idx] = new_pml3_phys | PAGE_PRESENT | PAGE_WRITABLE;

    haddr_t* new_pml3 = scratch_map_table(VIRT_SCRATCH_L3, new_pml3_phys);
    if (new_pml3 == NULL) { return NULL; }
    memset(new_pml3, 0, PAGE_SIZE);
  }

  haddr_t* pml3 =
      scratch_map_table(VIRT_SCRATCH_L3, entry_phys(pml4[pml4_idx]));
  if (pml3 == NULL) { return NULL; }
  if (!entry_is_present(pml3[pml3_idx])) {
    if (!create) { return NULL; }
    haddr_t new_pd_phys = pmm_alloc_frame();
    if (new_pd_phys == 0) { return NULL; }
    pml3[pml3_idx] = new_pd_phys | PAGE_PRESENT | PAGE_WRITABLE;

    haddr_t* new_pd = scratch_map_table(VIRT_SCRATCH_L2, new_pd_phys);
    if (new_pd == NULL) { return NULL; }
    memset(new_pd, 0, PAGE_SIZE);
  }

  haddr_t* pd = scratch_map_table(VIRT_SCRATCH_L2, entry_phys(pml3[pml3_idx]));
  if (pd == NULL) { return NULL; }
  if (!entry_is_present(pd[pd_idx])) {
    if (!create) { return NULL; }
    haddr_t new_pt_phys = pmm_alloc_frame();
    if (new_pt_phys == 0) { return NULL; }
    pd[pd_idx] = new_pt_phys | PAGE_PRESENT | PAGE_WRITABLE;

    haddr_t* new_pt = scratch_map_table(VIRT_SCRATCH_L1, new_pt_phys);
    if (new_pt == NULL) { return NULL; }
    memset(new_pt, 0, PAGE_SIZE);
  }

  return scratch_map_table(VIRT_SCRATCH_L1, entry_phys(pd[pd_idx]));
}

void _invlpg(haddr_t virt) {
  asm volatile("invlpg (%0)" ::"r"(virt) : "memory");
}
