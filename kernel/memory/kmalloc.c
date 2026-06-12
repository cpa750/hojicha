#include <haddr.h>
#include <kernel/g_kernel.h>
#include <memory/kmalloc.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <utils/irq.h>

#define PAGE_SIZE      4096
#define MAX_GROW_SIZE  160
#define SIZEOF_HEADER  sizeof(block_header_t)
#define SIZEOF_FOOTER  sizeof(block_footer_t)
#define BLOCK_OVERHEAD (SIZEOF_HEADER + SIZEOF_FOOTER)

typedef struct block_footer block_footer_t;

typedef struct block_header block_header_t;
struct block_header {
  bool is_free;
  haddr_t size_bytes;
  block_header_t* next;
  block_footer_t* footer;
} __attribute__((packed));

typedef struct block_footer block_footer_t;
struct block_footer {
  block_header_t* header;
} __attribute__((packed));

block_header_t* find_first_fit_block(block_header_t* free_regions, size_t size);
block_header_t* get_next(block_header_t* block);
block_header_t* get_previous(block_header_t* block);
block_header_t* grow_heap();
block_header_t* grow_heap_by(size_t size);
void insert_free_block(block_header_t* block);
bool is_block_header_in_heap(block_header_t* block);
bool is_footer_in_heap(block_footer_t* footer);
void merge_blocks(block_header_t* first, block_header_t* second);
void occupy_block(block_header_t* block, size_t size);
void remove_from_free_list(block_header_t* block);
void replace_free_list_block(block_header_t* old_block,
                             block_header_t* new_block);
block_header_t* split_region(block_header_t* block, size_t size);
void set_footer_at(block_header_t* block, haddr_t addr);

block_header_t* free_regions;
haddr_t kernel_heap_grow_size;
haddr_t first_available_vaddr;
haddr_t last_footer;

void kmalloc_initialize() {
  kernel_heap_grow_size = 5;  // Allocate 5 pages to start
  first_available_vaddr = vmm_get_first_available_vaddr(g_kernel.vmm);
  free_regions = (block_header_t*)vmm_map(g_kernel.vmm,
                                          first_available_vaddr,
                                          kernel_heap_grow_size,
                                          PAGE_PRESENT | PAGE_WRITABLE);

  if (free_regions == 0) {
    printf("OOM initializing kmalloc. Halt.");
    abort();
  }

  free_regions->is_free = true;

  free_regions->size_bytes = pmm_page_to_addr_base(kernel_heap_grow_size) -
                             SIZEOF_HEADER - SIZEOF_FOOTER;
  free_regions->next = NULL;
  block_footer_t* free_region_footer =
      (block_footer_t*)(((haddr_t)free_regions) + SIZEOF_HEADER +
                        free_regions->size_bytes);
  free_region_footer->header = free_regions;
  free_regions->footer = free_region_footer;
  last_footer = (haddr_t)free_region_footer;

  if (kernel_heap_grow_size < MAX_GROW_SIZE) { kernel_heap_grow_size <<= 1; }
}

void* kmalloc_page_aligned(size_t size) {
  uint64_t irq_state = irq_store();

  // TODO: replace this with a multi-core enabled spinlock later
  if (size > 0xFFFFFFFFFFFFFFFF) {
    irq_load(irq_state);
    return 0;
  }
  block_header_t* new_block = grow_heap_by(pmm_addr_to_page(size) + 1);
  if (new_block == NULL) {
    irq_load(irq_state);
    return NULL;
  }
  occupy_block(new_block, size);
  irq_load(irq_state);
  return new_block;
}

void* kmalloc(size_t size) {
  uint64_t irq_state = irq_store();

  if (size > 0xFFFFFFFFFFFFFFFF) {
    irq_load(irq_state);
    return 0;
  }

  if (size > (kernel_heap_grow_size)*PAGE_SIZE) {
    block_header_t* new_block = grow_heap_by(pmm_addr_to_page(size) + 1);
    if (new_block == NULL) {
      irq_load(irq_state);
      return NULL;
    }
    occupy_block(new_block, size);
    irq_load(irq_state);
    return (void*)((haddr_t)new_block + SIZEOF_HEADER);
  }

  block_header_t* first_fit = find_first_fit_block(free_regions, size);
  if (first_fit == NULL) {
    first_fit = grow_heap();
    if (first_fit == NULL) {
      irq_load(irq_state);
      return NULL;
    }
  }

  occupy_block(first_fit, size);

  void* ret = (void*)((haddr_t)first_fit + SIZEOF_HEADER);
  irq_load(irq_state);
  return ret;
}

void kfree(void* ptr) {
  uint64_t irq_state = irq_store();

  if (ptr == NULL) {
    irq_load(irq_state);
    return;
  }
  block_header_t* current_block =
      (block_header_t*)((haddr_t)ptr - SIZEOF_HEADER);
  if ((haddr_t)current_block < first_available_vaddr ||
      (haddr_t)current_block >= last_footer) {
    irq_load(irq_state);
    return;
  }

  if (!is_footer_in_heap(current_block->footer) ||
      current_block->footer->header != current_block) {
    irq_load(irq_state);
    return;
  }

  if (current_block->is_free) {
    irq_load(irq_state);
    return;
  }

  current_block->is_free = true;
  current_block->next = NULL;

  block_header_t* previous_block = get_previous(current_block);
  if (previous_block != NULL && previous_block->is_free) {
    remove_from_free_list(previous_block);
    merge_blocks(previous_block, current_block);
    current_block = previous_block;
  }

  block_header_t* next_block = get_next(current_block);
  if (next_block != NULL && next_block->is_free) {
    remove_from_free_list(next_block);
    merge_blocks(current_block, next_block);
  }

  insert_free_block(current_block);
  irq_load(irq_state);
}

#if defined(__stress_kmalloc)
uint64_t kmalloc_debug_last_footer(void) { return last_footer; }

void* kmalloc_debug_last_block_user(void) {
  block_header_t* last_block = ((block_footer_t*)last_footer)->header;
  return (void*)((haddr_t)last_block + SIZEOF_HEADER);
}

size_t kmalloc_debug_last_block_size(void) {
  block_header_t* last_block = ((block_footer_t*)last_footer)->header;
  return last_block->size_bytes;
}

bool kmalloc_debug_last_block_is_free(void) {
  block_header_t* last_block = ((block_footer_t*)last_footer)->header;
  return last_block->is_free;
}
#endif

void insert_free_block(block_header_t* block) {
  if (block == NULL) { return; }

  block->is_free = true;

  if (free_regions == NULL || (haddr_t)block < (haddr_t)free_regions) {
    block->next = free_regions;
    free_regions = block;
    return;
  }

  block_header_t* prev = free_regions;
  while (prev->next != NULL && (haddr_t)prev->next < (haddr_t)block) {
    prev = prev->next;
  }

  block->next = prev->next;
  prev->next = block;
}

block_header_t* find_first_fit_block(block_header_t* block, size_t size) {
  while (block != NULL) {
    if (block->size_bytes >= size && block->is_free) { return block; }
    block = block->next;
  }
  return NULL;
}

block_header_t* get_next(block_header_t* block) {
  if (block == NULL || block->footer == NULL) { return NULL; }

  haddr_t footer_addr = (haddr_t)block->footer;
  if (footer_addr < first_available_vaddr || footer_addr >= last_footer) {
    return NULL;
  }

  haddr_t next_addr = footer_addr + SIZEOF_FOOTER;
  if (next_addr >= last_footer) { return NULL; }
  return (block_header_t*)next_addr;
}

block_header_t* get_previous(block_header_t* block) {
  if ((haddr_t)block - SIZEOF_HEADER - SIZEOF_FOOTER < first_available_vaddr) {
    return NULL;
  }

  block_footer_t* previous_footer =
      (block_footer_t*)((haddr_t)block - SIZEOF_FOOTER);
  return previous_footer->header;
}

block_header_t* grow_heap() { return grow_heap_by(kernel_heap_grow_size++); }
block_header_t* grow_heap_by(size_t size) {
  haddr_t addr_to_map =
      ((haddr_t)last_footer + sizeof(last_footer) + 4095) & ~((uint64_t)4095);
  haddr_t new_pages =
      vmm_map(g_kernel.vmm, addr_to_map, size, PAGE_PRESENT | PAGE_WRITABLE);
  if (new_pages == 0) { return NULL; }

  block_header_t* last_block = ((block_footer_t*)last_footer)->header;
  block_header_t* new_block = (block_header_t*)(last_footer + SIZEOF_FOOTER);
  haddr_t difference = new_pages - ((haddr_t)last_footer + SIZEOF_FOOTER);

  new_block->is_free = true;
  new_block->size_bytes =
      pmm_page_to_addr_base(size) + difference - SIZEOF_HEADER - SIZEOF_FOOTER;
  new_block->next = NULL;
  block_footer_t* new_block_footer =
      (block_footer_t*)(((haddr_t)new_block) + SIZEOF_HEADER +
                        new_block->size_bytes);
  new_block_footer->header = new_block;
  new_block->footer = new_block_footer;
  last_footer = (haddr_t)new_block_footer;
  if (last_block->is_free) {
    remove_from_free_list(last_block);
    merge_blocks(last_block, new_block);
    new_block = last_block;
  }
  insert_free_block(new_block);
  return new_block;
}

void merge_blocks(block_header_t* first, block_header_t* second) {
  memset(first->footer, 0, SIZEOF_FOOTER);
  first->footer = second->footer;
  first->footer->header = first;
  first->next = second->next;
  first->size_bytes += second->size_bytes + BLOCK_OVERHEAD;
  memset(second, 0, SIZEOF_HEADER);
}

void __attribute__((noinline)) occupy_block(block_header_t* block,
                                            size_t size) {
  block->is_free = false;
  haddr_t size_needed_for_split = size + SIZEOF_HEADER + SIZEOF_FOOTER;
  if (block->size_bytes <= size_needed_for_split) {
    remove_from_free_list(block);
  } else {
    block_header_t* leftover = split_region(block, size);
    block->size_bytes = size;
    replace_free_list_block(block, leftover);
  }
  block->next = NULL;
}

void remove_from_free_list(block_header_t* block) {
  if (block == NULL || free_regions == NULL) { return; }

  if (free_regions == block) {
    free_regions = block->next;
    block->next = NULL;
    return;
  }

  block_header_t* prev = free_regions;
  while (prev->next != NULL && prev->next != block) { prev = prev->next; }

  if (prev->next == block) {
    prev->next = block->next;
    block->next = NULL;
  }
}

void replace_free_list_block(block_header_t* old_block,
                             block_header_t* new_block) {
  if (old_block == NULL || new_block == NULL) { return; }

  if (free_regions == old_block) {
    new_block->next = old_block->next;
    free_regions = new_block;
    old_block->next = NULL;
    return;
  }

  block_header_t* prev = free_regions;
  while (prev != NULL && prev->next != old_block) { prev = prev->next; }

  if (prev != NULL) {
    new_block->next = old_block->next;
    prev->next = new_block;
    old_block->next = NULL;
    return;
  }

  insert_free_block(new_block);
  old_block->next = NULL;
}

void set_footer_at(block_header_t* block, haddr_t addr) {
  block_footer_t* footer = (block_footer_t*)addr;
  footer->header = block;
  block->footer = footer;
}

block_header_t* split_region(block_header_t* block, size_t size) {
  block_header_t* leftover = (block_header_t*)((haddr_t)block + SIZEOF_HEADER +
                                               SIZEOF_FOOTER + (haddr_t)size);
  leftover->footer = block->footer;
  leftover->footer->header = leftover;

  set_footer_at(block, (haddr_t)(leftover)-SIZEOF_FOOTER);
  leftover->size_bytes =
      block->size_bytes - size - SIZEOF_HEADER - SIZEOF_FOOTER;
  leftover->next = block->next;
  leftover->is_free = true;
  return leftover;
}
