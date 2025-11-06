#include <drivers/serial.h>
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

__attribute__((
    used,
    section(".limine_requests"))) static volatile struct limine_memmap_request
    memmap_request = {.id = LIMINE_MEMMAP_REQUEST, .revision = 0};

__attribute__((used, section(".limine_requests"))) static volatile struct
    limine_executable_address_request executable_addr_request = {
        .id = LIMINE_EXECUTABLE_ADDRESS_REQUEST, .revision = 0};

// 0 -> Free
// 1 -> Reserved
uint8_t* mem_bitmap;

extern haddr_t __kernel_start;
haddr_t kernel_start_vaddr = (haddr_t)&__kernel_start;
extern haddr_t __kernel_end;
haddr_t kernel_end_vaddr = (haddr_t)&__kernel_end;

struct pmm_state {
  haddr_t total_mem;
  haddr_t total_pages;
  haddr_t free_pages;
  haddr_t page_size;
  haddr_t kernel_start;
  haddr_t kernel_vstart;
  haddr_t kernel_end;
  haddr_t kernel_vend;
  haddr_t kernel_page_count;
  haddr_t mem_bitmap;
  haddr_t bitmap_size;
  haddr_t max_section_length;
  haddr_t max_section_start_addr;
  haddr_t first_early_alloc_addr;
  haddr_t last_early_alloc_addr;
  bool memmap_is_initialized;
};
typedef struct pmm_state pmm_state_t;
haddr_t pmm_state_get_total_mem(pmm_state_t* p) { return p->total_mem; };
haddr_t pmm_state_get_free_mem(pmm_state_t* p) { return p->free_pages << 12; };
haddr_t pmm_state_get_total_pages(pmm_state_t* p) { return p->total_pages; };
haddr_t pmm_state_get_free_pages(pmm_state_t* p) { return p->free_pages; };
haddr_t pmm_state_get_page_size(pmm_state_t* p) { return p->page_size; };
haddr_t pmm_state_get_kernel_start(pmm_state_t* p) { return p->kernel_start; };
haddr_t pmm_state_get_kernel_vstart(pmm_state_t* p) {
  return p->kernel_vstart;
};
haddr_t pmm_state_get_kernel_end(pmm_state_t* p) { return p->kernel_end; };
haddr_t pmm_state_get_kernel_vend(pmm_state_t* p) { return p->kernel_vend; };
haddr_t pmm_state_get_kernel_page_count(pmm_state_t* p) {
  return p->kernel_page_count;
};
void pmm_state_dump(pmm_state_t* p) {
  printf("[PMM] Struct addr:\t\t\t\t%d B\n", (haddr_t)p);
  printf("[PMM] Total memory:\t\t\t\t%d B\n", p->total_mem);
  printf("[PMM] Free memory:\t\t\t\t%d B\n", p->free_pages << 12);
  printf("[PMM] Total pages:\t\t\t\t%d\n", p->total_pages);
  printf("[PMM] Free pages:\t\t\t\t%d\n", p->free_pages);
  printf("[PMM] Page size:\t\t\t\t%d B\n", p->page_size);
  printf("[PMM] Memory bitmap address:\t%x\n", p->mem_bitmap);
}

haddr_t align_to_prev_page(haddr_t addr);
haddr_t align_to_next_page(haddr_t addr);
void clear_page(haddr_t idx);
haddr_t early_alloc();
uint8_t get_lowest_zero_bit(uint8_t num);
void mark_page(haddr_t idx);

haddr_t pmm_addr_to_page(haddr_t addr) { return align_to_prev_page(addr); }
haddr_t pmm_page_to_addr_base(haddr_t page) { return page << 12; }

pmm_state_t pmm;

void initialize_pmm() {
  pmm.page_size = PAGE_SIZE;
  pmm.total_mem = 0;
  pmm.total_pages = 0;
  pmm.free_pages = 0;
  pmm.kernel_start = 0;
  pmm.kernel_end = 0;
  pmm.max_section_length = 0;
  pmm.max_section_start_addr = 0;
  pmm.memmap_is_initialized = false;

  uint64_t kernel_size_bytes = kernel_end_vaddr - kernel_start_vaddr;

  struct limine_executable_address_response* exec_addr =
      executable_addr_request.response;

  pmm.kernel_start = (haddr_t)exec_addr->physical_base;
  pmm.kernel_vstart = (haddr_t)exec_addr->virtual_base;
  pmm.kernel_vend = pmm_page_to_addr_base(align_to_next_page(
      (haddr_t)exec_addr->virtual_base + kernel_size_bytes + 1));
  haddr_t kernel_end = pmm.kernel_start + kernel_size_bytes;

  pmm.kernel_end = pmm_page_to_addr_base(align_to_next_page(kernel_end));
  pmm.kernel_page_count =
      pmm_addr_to_page(kernel_end - pmm.kernel_start + 4095);
  g_kernel.pmm = &pmm;

  struct limine_memmap_response* m_info = memmap_request.response;
  uint64_t entry_count = m_info->entry_count;

  // Calculate total available memory and find largest free area
  for (uint64_t i = 0; i < entry_count; ++i) {
    struct limine_memmap_entry* mmap_entry =
        (struct limine_memmap_entry*)(m_info->entries[i]);
    pmm.total_mem += mmap_entry->length;
    if (mmap_entry->type == LIMINE_MEMMAP_USABLE ||
        mmap_entry->type == LIMINE_MEMMAP_ACPI_RECLAIMABLE) {
      if (mmap_entry->length > pmm.max_section_length) {
        pmm.max_section_length = mmap_entry->length;
        pmm.max_section_start_addr = mmap_entry->base;
      }
    }
  }

  // Divide by page size (log base 2 (4096) == 12)
  pmm.total_pages = pmm.total_mem >> 12;
  pmm.bitmap_size = pmm.total_pages >> 3;
  if (pmm.max_section_length <
      (pmm.kernel_end - pmm.kernel_start + pmm.bitmap_size)) {
    printf("Not enough memory to load memory bitmap. Halt.");
    abort();
  }

  pmm.first_early_alloc_addr =
      pmm_page_to_addr_base(align_to_next_page(pmm.max_section_start_addr));
  pmm.last_early_alloc_addr = pmm.first_early_alloc_addr;
}

void pmm_initialize_bitmap() {
  uint64_t pages_for_bitmap = (pmm.bitmap_size + PAGE_SIZE - 1) / PAGE_SIZE;
  haddr_t bitmap_paddr;
  for (uint64_t i = 1; i <= pages_for_bitmap; ++i) {
    uint64_t addr = early_alloc();
    vmm_map_at_paddr(pmm.kernel_vend + PAGE_SIZE * i, addr,
                     PAGE_PRESENT | PAGE_WRITABLE);
    if (i == 1) {
      bitmap_paddr = addr;
    }
  }

  pmm.mem_bitmap = (pmm.kernel_vend + PAGE_SIZE);

  // Mark all memory as used
  mem_bitmap = (uint8_t*)pmm.mem_bitmap;
  memset(mem_bitmap, 0xFF, pmm.bitmap_size);

  haddr_t last_page =
      align_to_prev_page(pmm.max_section_start_addr + pmm.max_section_length);
  haddr_t first_page_after_bitmap =
      align_to_next_page(bitmap_paddr + pmm.bitmap_size);

  if (first_page_after_bitmap >= last_page) {
    printf("No free pages after PMM initialization. Halt.");
    abort();
  }

  // Make available only the largest region after kernel + bitmap
  for (haddr_t i = align_to_next_page(pmm.max_section_start_addr);
       i <=
       align_to_next_page(pmm.max_section_start_addr + pmm.max_section_length);
       ++i) {
    clear_page(i);
  }
}

haddr_t pmm_alloc_frame() {
  if (!pmm.memmap_is_initialized) {
    return early_alloc();
  }

  haddr_t bitmap_idx;
  for (bitmap_idx = 0; bitmap_idx <= pmm.bitmap_size; ++bitmap_idx) {
    haddr_t base_page_idx = bitmap_idx << 3;
    if (mem_bitmap[bitmap_idx] != 0xFF) {
      uint8_t offset = get_lowest_zero_bit(mem_bitmap[bitmap_idx]);
      mark_page(base_page_idx + offset);
      return pmm_page_to_addr_base(base_page_idx + offset);
    }
  }
  // OOM
  return 0;
}

void pmm_free_frame(haddr_t addr) { clear_page(pmm_addr_to_page(addr)); }

haddr_t align_to_next_page(haddr_t addr) { return (addr >> 12) + 1; }

haddr_t align_to_prev_page(haddr_t addr) { return addr >> 12; }

haddr_t early_alloc() {
  haddr_t ret = pmm.last_early_alloc_addr;
  pmm.last_early_alloc_addr += PAGE_SIZE;
  return ret;
}

void mark_page(haddr_t idx) {
  haddr_t mask = 1 << (idx & 7);
  // Convert page idx to bitmap idx by log base 2 (8) = 3 right shift
  mem_bitmap[idx >> 3] |= mask;
  pmm.free_pages--;
}

void clear_page(haddr_t idx) {
  haddr_t mask = ~(1 << (idx & 7));
  mem_bitmap[idx >> 3] &= mask;
  pmm.free_pages++;
}

uint8_t get_lowest_zero_bit(uint8_t num) {
  for (int i = 0; i < 8; ++i) {
    if (~(num >> i) & 0b1) {
      return i;
    }
  }
  return 7;
}

