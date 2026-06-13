#include <drivers/vga.h>
#include <haddr.h>
#include <kernel/g_kernel.h>
#include <limine.h>
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
#define ENTRY_PHYS_MASK        0x000FFFFFFFFFF000ULL
#define PD_ENTRIES             511ULL
#define RECURSIVE_IDX          510ULL
#define KERNEL_PML4_FIRST      256ULL
#define KERNEL_PML4_LAST       511ULL
#define VIRT_PT_START          0
#define VIRT_SCRATCH_ADDR      0xFFFFFFFF70000000ULL
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

  haddr_t* pml4_phys;
};
typedef struct vmm vmm_t;
typedef bool (*vmm_mapping_callback_t)(vmm_t* vmm,
                                       haddr_t virt,
                                       haddr_t entry,
                                       void* ctx);
haddr_t vmm_get_kernel_offset(vmm_t* vmm) { return vmm->kernel_offset; }
haddr_t vmm_get_first_available_vaddr(vmm_t* vmm) {
  return vmm->first_available_vaddr;
}
haddr_t* vmm_get_cr3(vmm_t* vmm) { return vmm->pml4_phys; }
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
static haddr_t* get_current_pml4(void);
uint16_t get_pml4_idx(haddr_t virt);
uint16_t get_pml3_idx(haddr_t virt);
uint16_t get_pd_idx(haddr_t virt);
uint16_t get_pt_idx(haddr_t virt);

void map_framebuffer(vmm_t* vmm);
static haddr_t map_in_current_cr3(haddr_t virt, haddr_t phys, haddr_t flags);
static bool entry_is_present(haddr_t entry);
static haddr_t entry_phys(haddr_t entry);
static haddr_t entry_flags(haddr_t entry);
static haddr_t* scratch_map_table(haddr_t scratch_addr, haddr_t phys);
static haddr_t* scratch_map_page(haddr_t scratch_addr, haddr_t phys);
static haddr_t* walk_to_child_table(haddr_t* table,
                                    uint16_t idx,
                                    haddr_t scratch_addr,
                                    haddr_t virt,
                                    haddr_t flags,
                                    bool create);
static haddr_t flags_to_mapping_request(haddr_t flags);
static haddr_t map_at_paddr_with_leaf_flags(vmm_t* vmm,
                                            haddr_t virt,
                                            haddr_t phys,
                                            haddr_t leaf_flags);
static bool walk_lower_mappings(vmm_t* vmm,
                                vmm_mapping_callback_t callback,
                                void* ctx,
                                bool free_tables);
static bool vmm_copy_mappings(vmm_t* dst, vmm_t* src);
static void vmm_destroy_mappings(vmm_t* vmm);
static bool copy_mapping(vmm_t* src, haddr_t virt, haddr_t entry, void* ctx);
static bool destroy_mapping(vmm_t* vmm, haddr_t virt, haddr_t entry, void* ctx);
static haddr_t* walk_non_kernel_to_pt(vmm_t* vmm,
                                      haddr_t virt,
                                      haddr_t flags,
                                      bool create);
static haddr_t normalize_leaf_flags(haddr_t virt, haddr_t flags);
static haddr_t normalize_table_flags(haddr_t virt, haddr_t flags);
static bool is_higher_half(haddr_t virt);
static void clear_nx(haddr_t* entry, haddr_t flags);
static void set_user_access(haddr_t* entry, haddr_t virt, haddr_t flags);

void _invlpg(haddr_t virt);

haddr_t* page_directory;
haddr_t* virtual_directory;

static haddr_t bootstrap_pml4[512] __attribute__((aligned(0x1000)));
static haddr_t bootstrap_pml3[512] __attribute__((aligned(0x1000)));
static haddr_t bootstrap_pd[512] __attribute__((aligned(0x1000)));
static haddr_t low_identity_pde[512] __attribute__((aligned(0x1000)));
static haddr_t kernel_pde[512] __attribute__((aligned(0x1000)));
static vmm_t kernel_vmm = {0};

void vmm_initialize() {
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
  kernel_vmm.pml4_phys = (haddr_t*)pml4_physical;
  map_framebuffer(&kernel_vmm);
  pmm_initialize_bitmap();

  for (haddr_t idx = KERNEL_PML4_FIRST; idx <= KERNEL_PML4_LAST; ++idx) {
    if (idx == RECURSIVE_IDX) { continue; }
    if (entry_is_present(bootstrap_pml4[idx])) { continue; }

    haddr_t new_pml3_phys = pmm_alloc_frame();
    if (new_pml3_phys == 0) {
      printf("OOM initializing kernel shared upper-half mappings. Halt.");
      abort();
    }

    bootstrap_pml4[idx] = new_pml3_phys | normalize_table_flags((idx << 39), 0);
    haddr_t* new_pml3 = scratch_map_table(VIRT_SCRATCH_L3, new_pml3_phys);
    if (new_pml3 == NULL) {
      printf("Unable to map shared upper-half PML3. Halt.");
      abort();
    }
    memset(new_pml3, 0, PAGE_SIZE);
  }

  kernel_vmm.first_available_vaddr =
      kernel_vstart + (kernel_end - kernel_start) + pmm_page_to_addr_base(1);
  kernel_vmm.last_available_vaddr = (haddr_t)virtual_directory;
  kernel_vmm.pml4_phys = (haddr_t*)pml4_physical;
}

vmm_t* vmm_new(haddr_t flags) {
  vmm_t* vmm = (vmm_t*)malloc(sizeof(vmm_t));
  if (vmm == NULL) { return NULL; }
  memset(vmm, 0, sizeof(vmm_t));

  haddr_t pml4 = pmm_alloc_frame();
  if (pml4 == 0) {
    free(vmm);
    return NULL;
  }

  vmm->kernel_offset = g_kernel.vmm->kernel_offset;
  vmm->pml4_phys = (haddr_t*)pml4;

  haddr_t* new_pml4 = scratch_map_table(VIRT_SCRATCH_L4, pml4);
  haddr_t* kernel_pml4 =
      scratch_map_table(VIRT_SCRATCH_L3, (haddr_t)g_kernel.vmm->pml4_phys);
  if (new_pml4 == NULL || kernel_pml4 == NULL) {
    pmm_free_frame(pml4);
    free(vmm);
    return NULL;
  }

  memset(new_pml4, 0, PAGE_SIZE);
  for (haddr_t idx = KERNEL_PML4_FIRST; idx <= KERNEL_PML4_LAST; ++idx) {
    if (idx == RECURSIVE_IDX) { continue; }
    new_pml4[idx] = kernel_pml4[idx];
  }

  new_pml4[RECURSIVE_IDX] =
      pml4 | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER_ACCESIBLE);

  vmm->first_available_vaddr = LOW_REGION_END;
  vmm->last_available_vaddr = LAST_VADDR;
  vmm->pml4_phys = (haddr_t*)pml4;
  return vmm;
}

vmm_t* vmm_copy(vmm_t* src) {
  if (src == NULL) { return NULL; }

  haddr_t* src_pml4 =
      scratch_map_table(VIRT_SCRATCH_L4, (haddr_t)src->pml4_phys);
  if (src_pml4 == NULL) { return NULL; }

  haddr_t recursive_flags = entry_flags(src_pml4[RECURSIVE_IDX]);
  vmm_t* dst = vmm_new(recursive_flags);
  if (dst == NULL) { return NULL; }

  dst->first_available_vaddr = src->first_available_vaddr;
  dst->last_available_vaddr = src->last_available_vaddr;
  dst->kernel_offset = src->kernel_offset;

  if (!vmm_copy_mappings(dst, src)) {
    vmm_free(dst);
    return NULL;
  }

  return dst;
}

haddr_t vmm_map(vmm_t* vmm, haddr_t virt, haddr_t page_count, haddr_t flags) {
  haddr_t base = virt;
  while (page_count-- > 0) {
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
  haddr_t leaf_flags = normalize_leaf_flags(virt, flags);
  haddr_t table_flags = normalize_table_flags(virt, flags);
  uint16_t pml4_idx = get_pml4_idx(virt);
  if (pml4_idx == RECURSIVE_IDX) { return 0; }

  if (vmm != g_kernel.vmm) {
    haddr_t* pt = walk_non_kernel_to_pt(vmm, virt, flags, true);
    if (pt == NULL) { return 0; }
    uint16_t pt_idx = get_pt_idx(virt);
    pt[pt_idx] = phys | leaf_flags;
    _invlpg(virt);
    return virt_base;
  }

  haddr_t* pml3 = get_pml4_entry(virt);
  if (!entry_is_present(virtual_directory[pml4_idx])) {
    haddr_t new_pml3_phys = pmm_alloc_frame();
    if (new_pml3_phys == 0) { return 0; }
    virtual_directory[pml4_idx] = new_pml3_phys | table_flags;
    memset(pml3, 0, PAGE_SIZE);
  }
  clear_nx(&virtual_directory[pml4_idx], flags);
  set_user_access(&virtual_directory[pml4_idx], virt, flags);

  haddr_t* pd = get_pml3_entry(virt);
  uint16_t pml3_idx = get_pml3_idx(virt);
  if (!entry_is_present(pml3[pml3_idx])) {
    haddr_t new_pd_phys = pmm_alloc_frame();
    if (new_pd_phys == 0) { return 0; }
    pml3[pml3_idx] = new_pd_phys | table_flags;
    memset(pd, 0, PAGE_SIZE);
  }
  clear_nx(&pml3[pml3_idx], flags);
  set_user_access(&pml3[pml3_idx], virt, flags);

  haddr_t* pt = get_pd_entry(virt);
  uint16_t pd_idx = get_pd_idx(virt);
  if (!entry_is_present(pd[pd_idx])) {
    haddr_t new_pt_phys = pmm_alloc_frame();
    if (new_pt_phys == 0) { return 0; }
    pd[pd_idx] = new_pt_phys | table_flags;
    memset(pt, 0, PAGE_SIZE);
  }
  clear_nx(&pd[pd_idx], flags);
  set_user_access(&pd[pd_idx], virt, flags);

  uint16_t pt_idx = get_pt_idx(virt);
  pt[pt_idx] = phys | leaf_flags;
  _invlpg(virt);
  return virt_base;
}

haddr_t vmm_unmap(vmm_t* vmm, haddr_t virt) {
  haddr_t virt_base = virt & MAPPING_STRUCTURE_MASK;
  uint16_t pml4_idx = get_pml4_idx(virt);
  if (pml4_idx == RECURSIVE_IDX) { return 0; }

  if (vmm != g_kernel.vmm) {
    haddr_t* pt = walk_non_kernel_to_pt(vmm, virt, 0, false);
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

void vmm_free(vmm_t* vmm) {
  if (vmm == NULL || vmm == g_kernel.vmm) { return; }

  vmm_destroy_mappings(vmm);
  pmm_free_frame((haddr_t)vmm->pml4_phys);
  free(vmm);
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

static haddr_t* get_current_pml4(void) {
  haddr_t entry = CANONICAL_HIGH | (RECURSIVE_IDX << 39) |
                  (RECURSIVE_IDX << 30) | (RECURSIVE_IDX << 21) |
                  (RECURSIVE_IDX << 12);
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

  haddr_t* pml4 = get_current_pml4();
  haddr_t* pml3 = get_pml4_entry(virt);
  uint16_t pml4_idx = get_pml4_idx(virt);
  if (pml4_idx == RECURSIVE_IDX) { return 0; }
  if (!(pml4[pml4_idx] & (PAGE_PRESENT | PAGE_WRITABLE))) {
    pml4[pml4_idx] = pmm_alloc_frame() | PAGE_PRESENT | PAGE_WRITABLE;
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
  return (entry & PAGE_PRESENT) == PAGE_PRESENT;
}

static haddr_t entry_phys(haddr_t entry) { return entry & ENTRY_PHYS_MASK; }

static haddr_t entry_flags(haddr_t entry) { return entry & ~ENTRY_PHYS_MASK; }

static haddr_t* scratch_map_table(haddr_t scratch_addr, haddr_t phys) {
  if (map_in_current_cr3(scratch_addr, phys, PAGE_PRESENT | PAGE_WRITABLE) ==
      0) {
    return NULL;
  }
  _invlpg(scratch_addr);
  return (haddr_t*)scratch_addr;
}

static haddr_t* scratch_map_page(haddr_t scratch_addr, haddr_t phys) {
  return scratch_map_table(scratch_addr, phys);
}

static haddr_t* walk_to_child_table(haddr_t* table,
                                    uint16_t idx,
                                    haddr_t scratch_addr,
                                    haddr_t virt,
                                    haddr_t flags,
                                    bool create) {
  if (!entry_is_present(table[idx])) {
    if (!create) { return NULL; }

    haddr_t new_table_phys = pmm_alloc_frame();
    if (new_table_phys == 0) { return NULL; }
    table[idx] = new_table_phys | normalize_table_flags(virt, flags);

    haddr_t* new_table = scratch_map_table(scratch_addr, new_table_phys);
    if (new_table == NULL) {
      table[idx] = 0;
      pmm_free_frame(new_table_phys);
      return NULL;
    }
    memset(new_table, 0, PAGE_SIZE);
    return new_table;
  }

  clear_nx(&table[idx], flags);
  set_user_access(&table[idx], virt, flags);
  return scratch_map_table(scratch_addr, entry_phys(table[idx]));
}

static haddr_t flags_to_mapping_request(haddr_t flags) {
  haddr_t request = flags;
  if ((flags & PAGE_NO_EXECUTE) == 0) { request |= PAGE_EXECUTABLE; }
  return request;
}

static haddr_t map_at_paddr_with_leaf_flags(vmm_t* vmm,
                                            haddr_t virt,
                                            haddr_t phys,
                                            haddr_t leaf_flags) {
  haddr_t virt_base = virt & MAPPING_STRUCTURE_MASK;
  haddr_t* pt = walk_non_kernel_to_pt(
      vmm, virt, flags_to_mapping_request(leaf_flags), true);
  if (pt == NULL) { return 0; }

  uint16_t pt_idx = get_pt_idx(virt);
  pt[pt_idx] = phys | leaf_flags;
  _invlpg(virt);
  return virt_base;
}

static bool walk_lower_mappings(vmm_t* vmm,
                                vmm_mapping_callback_t callback,
                                void* ctx,
                                bool free_tables) {
  for (haddr_t pml4_idx = 0; pml4_idx < KERNEL_PML4_FIRST; ++pml4_idx) {
    haddr_t* pml4 = scratch_map_table(VIRT_SCRATCH_L4, (haddr_t)vmm->pml4_phys);
    if (pml4 == NULL) { return false; }

    if (!entry_is_present(pml4[pml4_idx])) { continue; }

    haddr_t pml3_phys = entry_phys(pml4[pml4_idx]);
    for (haddr_t pml3_idx = 0; pml3_idx <= PD_ENTRIES; ++pml3_idx) {
      haddr_t* pml3 = scratch_map_table(VIRT_SCRATCH_L3, pml3_phys);
      if (pml3 == NULL) { return false; }

      if (!entry_is_present(pml3[pml3_idx])) { continue; }

      haddr_t pd_phys = entry_phys(pml3[pml3_idx]);
      for (haddr_t pd_idx = 0; pd_idx <= PD_ENTRIES; ++pd_idx) {
        haddr_t* pd = scratch_map_table(VIRT_SCRATCH_L2, pd_phys);
        if (pd == NULL) { return false; }

        if (!entry_is_present(pd[pd_idx])) { continue; }

        haddr_t pt_phys = entry_phys(pd[pd_idx]);
        for (haddr_t pt_idx = 0; pt_idx <= PD_ENTRIES; ++pt_idx) {
          haddr_t* pt = scratch_map_table(VIRT_SCRATCH_L1, pt_phys);
          if (pt == NULL) { return false; }

          haddr_t pt_entry = pt[pt_idx];
          if (!entry_is_present(pt_entry)) { continue; }

          haddr_t virt = (pml4_idx << 39) | (pml3_idx << 30) | (pd_idx << 21) |
                         (pt_idx << 12);
          if (!callback(vmm, virt, pt_entry, ctx)) { return false; }
        }

        if (free_tables) { pmm_free_frame(pt_phys); }
      }

      if (free_tables) { pmm_free_frame(pd_phys); }
    }

    if (free_tables) { pmm_free_frame(pml3_phys); }
  }

  return true;
}

static bool vmm_copy_mappings(vmm_t* dst, vmm_t* src) {
  return walk_lower_mappings(src, copy_mapping, dst, false);
}

static void vmm_destroy_mappings(vmm_t* vmm) {
  if (!walk_lower_mappings(vmm, destroy_mapping, NULL, true)) { return; }
}

static bool copy_mapping(vmm_t* src, haddr_t virt, haddr_t entry, void* ctx) {
  vmm_t* dst = (vmm_t*)ctx;
  haddr_t new_page = pmm_alloc_frame();
  if (new_page == 0) { return false; }

  haddr_t* src_page = scratch_map_page(VIRT_SCRATCH_ADDR, entry_phys(entry));
  haddr_t* dst_page = scratch_map_page(VIRT_PML4, new_page);
  if (src_page == NULL || dst_page == NULL) {
    pmm_free_frame(new_page);
    return false;
  }

  memcpy(dst_page, src_page, PAGE_SIZE);

  if (map_at_paddr_with_leaf_flags(dst, virt, new_page, entry_flags(entry)) ==
      0) {
    pmm_free_frame(new_page);
    return false;
  }

  (void)src;
  return true;
}

static bool destroy_mapping(vmm_t* vmm,
                            haddr_t virt,
                            haddr_t entry,
                            void* ctx) {
  pmm_free_frame(entry_phys(entry));
  return true;
}

static haddr_t* walk_non_kernel_to_pt(vmm_t* vmm,
                                      haddr_t virt,
                                      haddr_t flags,
                                      bool create) {
  uint16_t pml4_idx = get_pml4_idx(virt);
  uint16_t pml3_idx = get_pml3_idx(virt);
  uint16_t pd_idx = get_pd_idx(virt);

  if (pml4_idx == RECURSIVE_IDX) { return NULL; }

  haddr_t* pml4 = scratch_map_table(VIRT_SCRATCH_L4, (haddr_t)vmm->pml4_phys);
  if (pml4 == NULL) { return NULL; }

  haddr_t* pml3 =
      walk_to_child_table(pml4, pml4_idx, VIRT_SCRATCH_L3, virt, flags, create);
  if (pml3 == NULL) { return NULL; }

  haddr_t* pd =
      walk_to_child_table(pml3, pml3_idx, VIRT_SCRATCH_L2, virt, flags, create);
  if (pd == NULL) { return NULL; }

  return walk_to_child_table(pd, pd_idx, VIRT_SCRATCH_L1, virt, flags, create);
}

static haddr_t normalize_leaf_flags(haddr_t virt, haddr_t flags) {
  haddr_t leaf_flags = flags & ~PAGE_EXECUTABLE;

  if (is_higher_half(virt)) { leaf_flags &= ~PAGE_USER_ACCESIBLE; }

  if (is_higher_half(virt) && !(flags & PAGE_EXECUTABLE)) {
    leaf_flags |= PAGE_NO_EXECUTE;
  } else if (flags & PAGE_EXECUTABLE) {
    leaf_flags &= ~PAGE_NO_EXECUTE;
  }

  return leaf_flags;
}

static haddr_t normalize_table_flags(haddr_t virt, haddr_t flags) {
  haddr_t table_flags = PAGE_PRESENT | PAGE_WRITABLE;

  if ((flags & PAGE_USER_ACCESIBLE) && !is_higher_half(virt)) {
    table_flags |= PAGE_USER_ACCESIBLE;
  }

  if (is_higher_half(virt) && !(flags & PAGE_EXECUTABLE)) {
    table_flags |= PAGE_NO_EXECUTE;
  } else if (flags & PAGE_EXECUTABLE) {
    table_flags &= ~PAGE_NO_EXECUTE;
  }

  return table_flags;
}

static bool is_higher_half(haddr_t virt) {
  return get_pml4_idx(virt) >= KERNEL_PML4_FIRST;
}

static void clear_nx(haddr_t* entry, haddr_t flags) {
  if ((flags & PAGE_EXECUTABLE) && ((*entry & PAGE_NO_EXECUTE) != 0)) {
    *entry &= ~PAGE_NO_EXECUTE;
  }
}

static void set_user_access(haddr_t* entry, haddr_t virt, haddr_t flags) {
  if ((flags & PAGE_USER_ACCESIBLE) == 0) { return; }
  if (is_higher_half(virt)) { return; }
  if ((*entry & PAGE_USER_ACCESIBLE) == 0) { *entry |= PAGE_USER_ACCESIBLE; }
}

void _invlpg(haddr_t virt) {
  asm volatile("invlpg (%0)" ::"r"(virt) : "memory");
}
