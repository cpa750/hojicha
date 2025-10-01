#include <drivers/serial.h>
#include <kernel/kernel_state.h>
#include <memory/pmm.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 4096

extern uint32_t __kernel_start;
uint32_t kernel_start = (uint32_t)&__kernel_start;
extern uint32_t __kernel_end;
uint32_t kernel_end = (uint32_t)&__kernel_end;

// 0 -> Free
// 1 -> Reserved
uint8_t* mem_bitmap;

uint32_t last_page = 0;
uint32_t first_page_after_bitmap = 0;

struct pmm_state {
  uint32_t total_mem;
  uint32_t total_pages;
  uint32_t free_pages;
  uint32_t page_size;
  uint32_t kernel_start;
  uint32_t kernel_end;
  uint32_t kernel_page_count;
  uint32_t mem_bitmap;
};
typedef struct pmm_state pmm_state_t;
uint32_t pmm_state_get_total_mem(pmm_state_t* p) { return p->total_mem; };
uint32_t pmm_state_get_free_mem(pmm_state_t* p) { return p->free_pages << 12; };
uint32_t pmm_state_get_total_pages(pmm_state_t* p) { return p->total_pages; };
uint32_t pmm_state_get_free_pages(pmm_state_t* p) { return p->free_pages; };
uint32_t pmm_state_get_page_size(pmm_state_t* p) { return p->page_size; };
uint32_t pmm_state_get_kernel_start(pmm_state_t* p) { return p->kernel_start; };
uint32_t pmm_state_get_kernel_end(pmm_state_t* p) { return p->kernel_end; };
uint32_t pmm_state_get_kernel_page_count(pmm_state_t* p) {
  return p->kernel_page_count;
};
void pmm_state_dump(pmm_state_t* p) {
  printf("[PMM] Struct addr:\t\t\t\t%d B\n", (uint32_t)p);
  printf("[PMM] Total memory:\t\t\t\t%d B\n", p->total_mem);
  printf("[PMM] Free memory:\t\t\t\t%d B\n", p->free_pages << 12);
  printf("[PMM] Total pages:\t\t\t\t%d\n", p->total_pages);
  printf("[PMM] Free pages:\t\t\t\t%d\n", p->free_pages);
  printf("[PMM] Page size:\t\t\t\t%d B\n", p->page_size);
  printf("[PMM] Memory bitmap address:\t%x\n", p->mem_bitmap);
}

void mark_page(uint32_t idx);
uint8_t get_lowest_zero_bit(uint8_t num);
void clear_page(uint32_t idx);
uint32_t align_to_next_page(uint32_t addr);
uint32_t align_to_prev_page(uint32_t addr);

uint32_t pmm_addr_to_page(uint32_t addr) { return align_to_prev_page(addr); }

uint32_t pmm_page_to_addr_base(uint32_t page) { return page << 12; }

void pmm_reserve_region(uint16_t idx, uint16_t len) {
  for (int i = 0; i < len; ++i) {
    mark_page(idx + i);
  }
}

pmm_state_t pmm;

void initialize_pmm(multiboot_info_t* m_info) {
  pmm.page_size = PAGE_SIZE;
  pmm.total_mem = 0;
  pmm.total_pages = 0;
  pmm.free_pages = 0;

  uint32_t max_section_length = 0;
  uint64_t max_section_start_addr = 0;

  // Calculate total available memory and find largest free area
  for (uint32_t i = 0; i < m_info->mmap_length;
       i += sizeof(multiboot_memory_map_t)) {
    multiboot_memory_map_t* mmap_entry =
        (multiboot_memory_map_t*)(m_info->mmap_addr + i);
    pmm.total_mem += mmap_entry->len;
    if (mmap_entry->type == MULTIBOOT_MEMORY_AVAILABLE ||
        mmap_entry->type == MULTIBOOT_MEMORY_ACPI_RECLAIMABLE) {
      if (mmap_entry->len > max_section_length) {
        max_section_length = mmap_entry->len;
        max_section_start_addr = mmap_entry->addr;
      }
    }
  }

  // Divide by page size (log base 2 (4096) == 12)
  pmm.total_pages = pmm.total_mem >> 12;
  uint32_t bitmap_size = pmm.total_pages >> 3;
  if (max_section_length < (kernel_end - kernel_start + bitmap_size)) {
    printf("Not enough memory to load memory bitmap. Halt.");
    abort();
  }

  // Mark all memory as used
  mem_bitmap = (uint8_t*)((align_to_next_page(kernel_end + 1) + 1) * PAGE_SIZE);
  memset(mem_bitmap, 0xFF, bitmap_size);

  last_page = align_to_prev_page(max_section_start_addr + max_section_length);
  first_page_after_bitmap =
      align_to_next_page((uint32_t)(mem_bitmap) + bitmap_size);

  if (first_page_after_bitmap >= last_page) {
    printf("No free pages after PMM initialization. Halt.");
    abort();
  }

  // Make available only the largest region after kernel + bitmap
  for (uint32_t i = first_page_after_bitmap; i <= last_page; ++i) {
    clear_page(i);
  }
  pmm.mem_bitmap = (uint32_t)mem_bitmap;
  pmm.kernel_start = kernel_start;
  pmm.kernel_end = pmm_page_to_addr_base(first_page_after_bitmap);
  pmm.kernel_page_count = pmm_addr_to_page(kernel_end - kernel_start + 4095);
  g_kernel.pmm = &pmm;
}

uint32_t pmm_alloc_frame() {
  uint32_t bitmap_idx;
  for (bitmap_idx = (first_page_after_bitmap >> 3);
       bitmap_idx <= (last_page >> 3); ++bitmap_idx) {
    uint32_t base_page_idx = bitmap_idx << 3;
    if (mem_bitmap[bitmap_idx] != 0xFF) {
      uint8_t offset = get_lowest_zero_bit(mem_bitmap[bitmap_idx]);

      mark_page(base_page_idx + offset);
      return pmm_page_to_addr_base(base_page_idx + offset);
    }
  }
  // OOM
  return 0;
}

void pmm_free_frame(uint32_t addr) { clear_page(pmm_addr_to_page(addr)); }

uint32_t align_to_next_page(uint32_t addr) { return (addr >> 12) + 1; }

uint32_t align_to_prev_page(uint32_t addr) { return addr >> 12; }

void mark_page(uint32_t idx) {
  uint32_t mask = 1 << (idx & 7);
  // Convert page idx to bitmap idx by log base 2 (8) = 3 right shift
  mem_bitmap[idx >> 3] |= mask;
  pmm.free_pages--;
}

void clear_page(uint32_t idx) {
  uint32_t mask = ~(1 << (idx & 7));
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

