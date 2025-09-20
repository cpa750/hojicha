#include <drivers/serial.h>
#include <limits.h>
#include <memory/pmm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 4096
#define MEM_BITMAP_LENGTH 32768

extern uint32_t __kernel_start;
uint32_t kernel_start = (uint32_t)&__kernel_start;
extern uint32_t __kernel_end;
uint32_t kernel_end = (uint32_t)&__kernel_end;

// 0 -> Free
// 1 -> Reserved
static uint8_t* mem_bitmap;

uint32_t total_pages = 0;
uint32_t free_pages = 0;

void mark_page(uint32_t idx) {
  uint32_t mask = 1 << (idx & 31);
  // Convert page idx to bitmap idx by log base 2 (32) = 5 right shift
  mem_bitmap[idx >> 5] |= mask;
}

void clear_page(uint32_t idx) {
  uint32_t mask = ~(1 << (idx & 31));
  mem_bitmap[idx >> 5] &= mask;
}

uint32_t align_to_next_page(uint32_t addr) { return (addr >> 12) + 1; }
uint32_t align_to_prev_page(uint32_t addr) { return addr >> 12; }

void pmm_reserve_region(uint16_t idx, uint16_t len) {
  // TODO figure out a more efficient way of doing this
  for (int i = 0; i < len; ++i) {
    mark_page(idx + i);
    --free_pages;
  }
}

void initialize_pmm(multiboot_info_t* m_info) {
  uint32_t max_section_length = 0;
  uint64_t max_section_start_addr = 0;
  uint32_t available_mem_bytes = 0;
  for (uint32_t i = 0; i < m_info->mmap_length;
       i += sizeof(multiboot_memory_map_t)) {
    multiboot_memory_map_t* mmap_entry =
        (multiboot_memory_map_t*)(m_info->mmap_addr + i);
    available_mem_bytes += mmap_entry->len;
    if (mmap_entry->len > max_section_length &&
        mmap_entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
      max_section_length = mmap_entry->len;
      max_section_start_addr = mmap_entry->addr;
    }
  }

  uint32_t mem_bitmap_bytes = sizeof(uint32_t) * MEM_BITMAP_LENGTH;
  if (max_section_length > (kernel_end - kernel_start + mem_bitmap_bytes)) {
    printf("Not enough memory to load memory bitmap. Halt.");
    abort();
  }

  total_pages = max_section_length;
  mem_bitmap = (uint8_t*)((align_to_next_page(kernel_end + 1) + 1) * PAGE_SIZE);
  memset(mem_bitmap, 0xFF, mem_bitmap_bytes);

  uint16_t kernel_start_page = align_to_prev_page(kernel_start);
  uint16_t kernel_end_page = align_to_next_page(kernel_end + 1);
}

