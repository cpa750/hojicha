#include <limits.h>
#include <memory/pmm.h>

#define PAGE_SIZE 4096
#define MEM_BITMAP_LENGTH 32768

// TODO: declare these in asm
extern uint32_t __kernel_start;
extern uint32_t __kernel_end;

// TODO: Align this to next page after kernel end,
// reserve the region needed for it and memset to 0
// 0 -> Free
// 1 -> Reserved
uint32_t mem_bitmap[32768] = {0};

void mark_page(uint16_t idx) {
  uint16_t mask = 1 << (idx & 31);
  // Convert page idx to bitmap idx by log base 2 (32) = 5 right shift
  mem_bitmap[idx >> 5] |= mask;
}

void clear_page(uint16_t idx) {
  uint16_t mask = ~(1 << (idx & 31));
  mem_bitmap[idx >> 5] &= mask;
}

uint32_t align_to_next_page(uint32_t addr) { return (addr >> 12) + 1; }
uint32_t align_to_prev_page(uint32_t addr) { return addr >> 12; }

void pmm_reserve_region(uint16_t idx, uint16_t len) {
  // TODO figure out a more efficient way of doing this
  for (int i = 0; i < len; ++i) {
    mark_page(idx + i);
  }
}

void initialize_pmm(multiboot_info_t* m_info) {
  // Reserve low mem regions (up to 1M)
  pmm_reserve_region(0, align_to_next_page(1048577));
  uint32_t max_section_length = 0;
  uint64_t max_section_start_addr = 0;
  for (uint32_t i = 0; i < m_info->mmap_length;
       i += sizeof(multiboot_memory_map_t)) {
    multiboot_memory_map_t* mmap_entry =
        (multiboot_memory_map_t*)(m_info->mmap_addr + i);
    if (mmap_entry->len > max_section_length &&
        mmap_entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
      max_section_length = mmap_entry->len;
      max_section_start_addr = mmap_entry->addr;
    }
  }
  pmm_reserve_region(align_to_prev_page(__kernel_start),
                     align_to_next_page(__kernel_end - __kernel_start + 1));
}

