#include <kernel/kernel_state.h>
#include <memory/kmalloc.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#define PAGE_SIZE 4096
#define MAX_GROW_SIZE 160

typedef struct block_footer block_footer_t;

typedef struct block_header block_header_t;
struct block_header {
  bool is_free;
  uint32_t size_bytes;
  block_header_t* next;
  block_footer_t* footer;
};

typedef struct block_footer block_footer_t;
struct block_footer {
  block_header_t* header;
};

block_header_t* find_first_fit_block(block_header_t* free_regions, size_t size);
block_header_t* get_previous(block_header_t* block);
block_header_t* get_previous_free(block_header_t* block);
block_header_t* grow_heap();
block_header_t* grow_heap_by(size_t size);
void occupy_block(block_header_t* block, size_t size);
void remove_from_free_list(block_header_t* block);
block_header_t* split_region(block_header_t* block, size_t size);
void set_footer_at(block_header_t* block, uint32_t addr);
// bool are_contiguous(block_header_t* first, block_header_t* second);
// void merge_blocks(block_header_t* first, block_header_t* second);

block_header_t* free_regions;
uint32_t kernel_heap_grow_size;
uint32_t first_available_vaddr;
uint32_t last_footer;

void kmalloc_initialize() {
  kernel_heap_grow_size = 5;  // Allocate 5 pages to start
  first_available_vaddr = vmm_state_get_first_available_vaddr(g_kernel.vmm);
  free_regions =
      (block_header_t*)vmm_map(first_available_vaddr, kernel_heap_grow_size,
                               PAGE_PRESENT | PAGE_WRITABLE);

  if (free_regions == 0) {
    printf("OOM initializing kmalloc. Halt.");
    abort();
  }

  free_regions->is_free = true;

  free_regions->size_bytes =
      pmm_page_to_addr_base(kernel_heap_grow_size) - sizeof(block_footer_t);
  free_regions->next = NULL;
  block_footer_t* free_region_footer =
      (block_footer_t*)(((uint32_t)free_regions) + free_regions->size_bytes);
  free_region_footer->header = free_regions;
  free_regions->footer = free_region_footer;
  last_footer = (uint32_t)free_region_footer;

  if (kernel_heap_grow_size < MAX_GROW_SIZE) {
    kernel_heap_grow_size <<= 1;
  }
}

void* kmalloc(size_t size) {
  if (size > 0xFFFFFFFF) {
    return 0;
  }

  if (size > (kernel_heap_grow_size)*PAGE_SIZE) {
    block_header_t* new_block = grow_heap_by(pmm_addr_to_page(size) + 1);
    occupy_block(new_block, size);
    return new_block;
  }

  block_header_t* first_fit = find_first_fit_block(free_regions, size);

  if (first_fit == NULL) {
    first_fit = grow_heap();
    if (first_fit == NULL) {
      return NULL;
    }
  }

  occupy_block(first_fit, size);

  return (void*)((uint32_t)first_fit + sizeof(block_header_t));
}

block_header_t* find_first_fit_block(block_header_t* block, size_t size) {
  if (block == NULL) {
    return NULL;
  }

  if (block->size_bytes >= size && block->is_free) {
    return block;
  }

  return find_first_fit_block(block->next, size);
}

block_header_t* get_previous(block_header_t* block) {
  if ((uint32_t)block <= first_available_vaddr) {
    return NULL;
  }

  block_footer_t* previous_footer =
      (block_footer_t*)((uint32_t)block - sizeof(block_footer_t));
  return previous_footer->header;
}

block_header_t* get_previous_free(block_header_t* block) {
  block_header_t* prev = get_previous(block);

  if (prev == NULL) {
    return prev;
  }

  if (!prev->is_free) {
    return get_previous_free(prev);
  }
  return prev;
}

block_header_t* grow_heap() { return grow_heap_by(kernel_heap_grow_size++); }
block_header_t* grow_heap_by(size_t size) {
  block_header_t* new_block = (block_header_t*)vmm_map(
      last_footer + sizeof(last_footer), size, PAGE_PRESENT | PAGE_WRITABLE);

  if (new_block == 0) {
    return NULL;
  }

  new_block->is_free = true;
  new_block->size_bytes = pmm_page_to_addr_base(size) - sizeof(block_footer_t);
  new_block->next = NULL;
  block_footer_t* new_block_footer =
      (block_footer_t*)(((uint32_t)new_block) + new_block->size_bytes +
                        sizeof(block_footer_t));
  new_block_footer->header = new_block;
  new_block->footer = new_block_footer;
  last_footer = (uint32_t)new_block_footer;
  return new_block;
}

void occupy_block(block_header_t* block, size_t size) {
  block->is_free = false;
  uint32_t size_needed_for_split =
      size + sizeof(block_header_t) + sizeof(block_footer_t);
  if (block->size_bytes < size_needed_for_split) {
    remove_from_free_list(block);
  } else {
    // This call to this function needs to account for the fact we need to write
    // the footer at the end as well!
    block_header_t* leftover =
        split_region(block, size + sizeof(block_footer_t));
    block->next = leftover;
    remove_from_free_list(block);
  }
}

void remove_from_free_list(block_header_t* block) {
  block_header_t* previous_header = get_previous_free(block);
  if (previous_header != NULL) {
    previous_header->next = block->next;
  }
}

void set_footer_at(block_header_t* block, uint32_t addr) {
  block_footer_t* footer = (block_footer_t*)addr;
  footer->header = block;
  block->footer = footer;
}

block_header_t* split_region(block_header_t* block, size_t size) {
  block_header_t* leftover =
      (block_header_t*)((uint32_t)block + sizeof(block_header_t) +
                        (uint32_t)size);

  set_footer_at(block, (uint32_t)(leftover) - sizeof(block_footer_t));

  leftover->footer = block->footer;
  leftover->size_bytes = block->size_bytes - size - sizeof(block_header_t) -
                         sizeof(block_footer_t);
  leftover->next = block->next;
  leftover->is_free = true;
  return leftover;
}

// bool are_contiguous(block_header_t* first, block_header_t* second) {}
// void merge_blocks(block_header_t* first, block_header_t* second) {}

