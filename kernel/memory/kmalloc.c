#include <kernel/kernel_state.h>
#include <memory/kmalloc.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAGE_SIZE 4096
#define MAX_GROW_SIZE 160
#define SIZEOF_HEADER 13
#define SIZEOF_FOOTER 4

typedef struct block_footer block_footer_t;

typedef struct block_header block_header_t;
struct block_header {
  bool is_free;
  uint32_t size_bytes;
  block_header_t* next;
  block_footer_t* footer;
} __attribute__((packed));

typedef struct block_footer block_footer_t;
struct block_footer {
  block_header_t* header;
} __attribute__((packed));

void add_to_free_list(block_header_t* prev, block_header_t* current);
bool are_contiguous(block_header_t* first, block_header_t* second);
block_header_t* find_first_fit_block(block_header_t* free_regions, size_t size);
block_header_t* get_next(block_header_t* block);
block_header_t* get_next_free(block_header_t* block);
block_header_t* get_previous(block_header_t* block);
block_header_t* get_previous_free(block_header_t* block);
block_header_t* grow_heap();
block_header_t* grow_heap_by(size_t size);
void merge_blocks(block_header_t* first, block_header_t* second);
void occupy_block(block_header_t* block, size_t size);
void remove_from_free_list(block_header_t* block);
block_header_t* split_region(block_header_t* block, size_t size);
void set_footer_at(block_header_t* block, uint32_t addr);

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
      pmm_page_to_addr_base(kernel_heap_grow_size) - (SIZEOF_FOOTER * 8);
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

  return (void*)((uint32_t)first_fit + SIZEOF_HEADER);
}

void kfree(void* ptr) {
  block_header_t* current_block =
      (block_header_t*)((uint32_t)ptr - SIZEOF_HEADER);
  current_block->is_free = true;
  // Special case to handle when the first available block is freed
  if ((uint32_t)current_block == first_available_vaddr) {
    block_header_t* first_free = get_next_free(current_block);
    if (first_free != NULL) {
      if (are_contiguous(current_block, first_free)) {
        merge_blocks(current_block, first_free);
      } else {
        current_block->next = first_free;
      }
    }
  }

  block_header_t* previous_free = get_previous_free(current_block);
  block_header_t* next_free;
  if (previous_free != NULL && previous_free->next != NULL) {
    next_free = previous_free->next;
  } else {
    next_free = NULL;
  }

  if (are_contiguous(previous_free, current_block)) {
    if (are_contiguous(current_block, next_free)) {
      merge_blocks(previous_free, current_block);
      merge_blocks(previous_free, next_free);
    } else {
      merge_blocks(previous_free, current_block);
    }
  } else if (are_contiguous(current_block, next_free)) {
    merge_blocks(current_block, next_free);
  } else {
    add_to_free_list(previous_free, current_block);
  }
}

void add_to_free_list(block_header_t* prev, block_header_t* current) {
  if (current == NULL) {
    return;
  }

  if (prev != NULL) {
    current->next = prev->next;
    prev->next = current;
  } else {
    block_header_t* next_free = get_next_free(current);
    if (next_free != NULL) {
      current->next = next_free;
    }
  }
}

bool are_contiguous(block_header_t* first, block_header_t* second) {
  if (first == NULL || second == NULL) {
    return false;
  }

  if ((uint32_t)first->footer == (uint32_t)second - SIZEOF_FOOTER) {
    return true;
  }
  return false;
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

block_header_t* get_next(block_header_t* block) {
  if (((uint32_t)block->footer + SIZEOF_FOOTER) >= last_footer) {
    return NULL;
  }
  return (block_header_t*)((uint32_t)block->footer + SIZEOF_FOOTER);
}

block_header_t* get_next_free(block_header_t* block) {
  block_header_t* next = get_next(block);
  if (!next->is_free) {
    if (next->footer != NULL) {
      return get_next_free(next);
    }
    return NULL;
  }
  return next;
}

block_header_t* get_previous(block_header_t* block) {
  if ((uint32_t)block - SIZEOF_HEADER - SIZEOF_FOOTER < first_available_vaddr) {
    return NULL;
  }

  block_footer_t* previous_footer =
      (block_footer_t*)((uint32_t)block - SIZEOF_FOOTER);
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
  uint32_t addr_to_map =
      ((uint32_t)last_footer + sizeof(last_footer) + 4095) & ~((uint64_t)4095);
  uint32_t new_pages = vmm_map(addr_to_map, size, PAGE_PRESENT | PAGE_WRITABLE);
  if (new_pages == 0) {
    return NULL;
  }

  block_header_t* new_block = (block_header_t*)(last_footer + SIZEOF_FOOTER);
  uint32_t difference = new_pages - ((uint32_t)last_footer + SIZEOF_FOOTER);

  new_block->is_free = true;
  new_block->size_bytes =
      pmm_page_to_addr_base(size) + difference - SIZEOF_FOOTER;
  new_block->next = NULL;
  block_footer_t* new_block_footer =
      (block_footer_t*)(((uint32_t)new_block) + new_block->size_bytes);
  new_block_footer->header = new_block;
  new_block->footer = new_block_footer;
  last_footer = (uint32_t)new_block_footer;
  block_header_t* prev_free = get_previous_free(new_block);
  if (prev_free != NULL) {
    prev_free->next = new_block;
  }
  return new_block;
}

void merge_blocks(block_header_t* first, block_header_t* second) {
  memset(first->footer, 0, SIZEOF_FOOTER);
  first->footer = second->footer;
  first->footer->header = first;
  first->next = second->next;
  first->size_bytes += second->size_bytes;
  memset(second, 0, SIZEOF_HEADER);
}

void occupy_block(block_header_t* block, size_t size) {
  block->is_free = false;
  uint32_t size_needed_for_split = size + SIZEOF_HEADER + SIZEOF_FOOTER;
  if (block->size_bytes < size_needed_for_split) {
    remove_from_free_list(block);
  } else {
    block_header_t* leftover = split_region(block, size);
    block->next = leftover;
    block->size_bytes = size;
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
      (block_header_t*)((uint32_t)block + SIZEOF_HEADER + (uint32_t)size);
  leftover->footer = block->footer;
  leftover->footer->header = leftover;

  set_footer_at(block, (uint32_t)(leftover)-SIZEOF_FOOTER);
  leftover->size_bytes =
      block->size_bytes - size - SIZEOF_HEADER - SIZEOF_FOOTER;
  leftover->next = block->next;
  leftover->is_free = true;
  return leftover;
}

