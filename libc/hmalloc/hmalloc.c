#include <hmalloc.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#if defined(__is_libk)
#include <haddr.h>
#include <kernel/g_kernel.h>
#include <memory/vmm.h>
#include <stdio.h>
#include <stdlib.h>
#include <utils/irq.h>
#else
#include <internal/__hlibc_heap.h>
#include <unistd.h>
#endif

#define PAGE_SIZE      4096
#define MAX_GROW_SIZE  160
#define SIZEOF_HEADER  sizeof(block_header_t)
#define SIZEOF_FOOTER  sizeof(block_footer_t)
#define BLOCK_OVERHEAD (SIZEOF_HEADER + SIZEOF_FOOTER)

typedef uintptr_t hmalloc_addr_t;

typedef struct block_footer block_footer_t;

typedef struct block_header block_header_t;
struct block_header {
  bool is_free;
  hmalloc_addr_t size_bytes;
  block_header_t* next;
  block_footer_t* footer;
} __attribute__((packed));

typedef struct block_footer block_footer_t;
struct block_footer {
  block_header_t* header;
} __attribute__((packed));

static block_header_t* find_first_fit_block(block_header_t* block,
                                            size_t size);
static block_header_t* get_next(block_header_t* block);
static block_header_t* get_previous(block_header_t* block);
static block_header_t* grow_heap(void);
static block_header_t* grow_heap_by(size_t pages);
static void insert_free_block(block_header_t* block);
static bool is_block_header_in_heap(block_header_t* block);
static bool is_footer_in_heap(block_footer_t* footer);
static void merge_blocks(block_header_t* first, block_header_t* second);
static void occupy_block(block_header_t* block, size_t size);
static void remove_from_free_list(block_header_t* block);
static void replace_free_list_block(block_header_t* old_block,
                                    block_header_t* new_block);
static block_header_t* split_region(block_header_t* block, size_t size);
static void set_footer_at(block_header_t* block, hmalloc_addr_t addr);

static block_header_t* free_regions;
static hmalloc_addr_t heap_grow_size;
static hmalloc_addr_t first_available_vaddr;
static hmalloc_addr_t last_footer;
static bool heap_initialized;

static hmalloc_addr_t addr_to_page(hmalloc_addr_t addr) {
  return addr / PAGE_SIZE;
}

static hmalloc_addr_t page_to_addr_base(hmalloc_addr_t page) {
  return page * PAGE_SIZE;
}

static hmalloc_addr_t align_up(hmalloc_addr_t addr, hmalloc_addr_t alignment) {
  return (addr + alignment - 1) & ~(alignment - 1);
}

static bool bytes_to_pages(size_t size, size_t* pages) {
  if (size > ((size_t)-1) - BLOCK_OVERHEAD - PAGE_SIZE + 1) {
    return false;
  }

  *pages =
      addr_to_page((hmalloc_addr_t)size + BLOCK_OVERHEAD + PAGE_SIZE - 1);
  if (*pages == 0) { *pages = 1; }
  return true;
}

static bool initialize_heap_region(void* heap_start, size_t pages) {
  free_regions = (block_header_t*)heap_start;
  if (free_regions == NULL) { return false; }

  first_available_vaddr = (hmalloc_addr_t)free_regions;
  free_regions->is_free = true;
  free_regions->size_bytes =
      page_to_addr_base(pages) - SIZEOF_HEADER - SIZEOF_FOOTER;
  free_regions->next = NULL;

  block_footer_t* free_region_footer =
      (block_footer_t*)(first_available_vaddr + SIZEOF_HEADER +
                        free_regions->size_bytes);
  free_region_footer->header = free_regions;
  free_regions->footer = free_region_footer;
  last_footer = (hmalloc_addr_t)free_region_footer;

  if (heap_grow_size < MAX_GROW_SIZE) { heap_grow_size <<= 1; }
  heap_initialized = true;
  return true;
}

#if defined(__is_libk)
void hmalloc_initialize(void) {
  heap_grow_size = 5;  // Allocate 5 pages to start.
  hmalloc_addr_t heap_start = vmm_get_first_available_vaddr(g_kernel.vmm);
  void* heap_region = (void*)vmm_map(g_kernel.vmm,
                                     heap_start,
                                     heap_grow_size,
                                     PAGE_PRESENT | PAGE_WRITABLE);

  if (!initialize_heap_region(heap_region, heap_grow_size)) {
    printf("OOM initializing hmalloc. Halt.");
    abort();
  }
}

static uint64_t hmalloc_lock(void) { return irq_store(); }

static void hmalloc_unlock(uint64_t irq_state) { irq_load(irq_state); }

static hmalloc_addr_t map_heap_pages(hmalloc_addr_t addr, size_t pages) {
  return vmm_map(g_kernel.vmm, addr, pages, PAGE_PRESENT | PAGE_WRITABLE);
}
#else
static bool hmalloc_initialize_user(void) {
  heap_grow_size = 5;  // Allocate 5 pages to start.
  __hlibc_heap_init();

  size_t bytes = page_to_addr_base(heap_grow_size);
  if (bytes > (size_t)INTPTR_MAX) { return false; }

  void* heap_region = sbrk((intptr_t)bytes);
  if (heap_region == (void*)-1) { return false; }

  return initialize_heap_region(heap_region, heap_grow_size);
}

static uint64_t hmalloc_lock(void) { return 0; }

static void hmalloc_unlock(uint64_t irq_state) { (void)irq_state; }

static hmalloc_addr_t map_heap_pages(hmalloc_addr_t addr, size_t pages) {
  size_t bytes = page_to_addr_base(pages);
  if (bytes > (size_t)INTPTR_MAX) { return 0; }

  void* mapped = sbrk((intptr_t)bytes);
  if (mapped == (void*)-1 || (hmalloc_addr_t)mapped != addr) { return 0; }
  return (hmalloc_addr_t)mapped;
}
#endif

#if defined(__is_libk)
void* hmalloc_page_aligned(size_t size) {
  uint64_t lock_state = hmalloc_lock();
  size_t pages;
  if (!heap_initialized || !bytes_to_pages(size, &pages)) {
    hmalloc_unlock(lock_state);
    return NULL;
  }

  block_header_t* new_block = grow_heap_by(pages);
  if (new_block == NULL) {
    hmalloc_unlock(lock_state);
    return NULL;
  }
  occupy_block(new_block, size);
  hmalloc_unlock(lock_state);
  return new_block;
}
#endif

void* hmalloc(size_t size) {
  uint64_t lock_state = hmalloc_lock();

#if !defined(__is_libk)
  if (!heap_initialized && !hmalloc_initialize_user()) {
    hmalloc_unlock(lock_state);
    return NULL;
  }
#endif

  if (!heap_initialized) {
    hmalloc_unlock(lock_state);
    return NULL;
  }

  size_t pages;
  if (!bytes_to_pages(size, &pages)) {
    hmalloc_unlock(lock_state);
    return NULL;
  }

  if (pages > heap_grow_size) {
    block_header_t* new_block = grow_heap_by(pages);
    if (new_block == NULL) {
      hmalloc_unlock(lock_state);
      return NULL;
    }
    occupy_block(new_block, size);
    hmalloc_unlock(lock_state);
    return (void*)((hmalloc_addr_t)new_block + SIZEOF_HEADER);
  }

  block_header_t* first_fit = find_first_fit_block(free_regions, size);
  if (first_fit == NULL) {
    first_fit = grow_heap();
    if (first_fit == NULL) {
      hmalloc_unlock(lock_state);
      return NULL;
    }
  }

  occupy_block(first_fit, size);

  void* ret = (void*)((hmalloc_addr_t)first_fit + SIZEOF_HEADER);
  hmalloc_unlock(lock_state);
  return ret;
}

void hfree(void* ptr) {
  uint64_t lock_state = hmalloc_lock();

  if (ptr == NULL || !heap_initialized) {
    hmalloc_unlock(lock_state);
    return;
  }

  block_header_t* current_block =
      (block_header_t*)((hmalloc_addr_t)ptr - SIZEOF_HEADER);
  if ((hmalloc_addr_t)current_block < first_available_vaddr ||
      (hmalloc_addr_t)current_block >= last_footer) {
    hmalloc_unlock(lock_state);
    return;
  }

  if (!is_footer_in_heap(current_block->footer) ||
      current_block->footer->header != current_block) {
    hmalloc_unlock(lock_state);
    return;
  }

  if (current_block->is_free) {
    hmalloc_unlock(lock_state);
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
  hmalloc_unlock(lock_state);
}

#if defined(__stress_hmalloc)
uint64_t hmalloc_debug_last_footer(void) { return last_footer; }

void* hmalloc_debug_last_block_user(void) {
  block_header_t* last_block = ((block_footer_t*)last_footer)->header;
  return (void*)((hmalloc_addr_t)last_block + SIZEOF_HEADER);
}

size_t hmalloc_debug_last_block_size(void) {
  block_header_t* last_block = ((block_footer_t*)last_footer)->header;
  return last_block->size_bytes;
}

bool hmalloc_debug_last_block_is_free(void) {
  block_header_t* last_block = ((block_footer_t*)last_footer)->header;
  return last_block->is_free;
}
#endif

static void insert_free_block(block_header_t* block) {
  if (block == NULL) { return; }

  block->is_free = true;

  if (free_regions == NULL ||
      (hmalloc_addr_t)block < (hmalloc_addr_t)free_regions) {
    block->next = free_regions;
    free_regions = block;
    return;
  }

  block_header_t* prev = free_regions;
  while (prev->next != NULL &&
         (hmalloc_addr_t)prev->next < (hmalloc_addr_t)block) {
    prev = prev->next;
  }

  block->next = prev->next;
  prev->next = block;
}

static block_header_t* find_first_fit_block(block_header_t* block,
                                            size_t size) {
  while (block != NULL) {
    if (block->size_bytes >= size && block->is_free) { return block; }
    block = block->next;
  }
  return NULL;
}

static block_header_t* get_next(block_header_t* block) {
  if (!is_block_header_in_heap(block) || !is_footer_in_heap(block->footer)) {
    return NULL;
  }

  hmalloc_addr_t footer_addr = (hmalloc_addr_t)block->footer;
  hmalloc_addr_t next_addr = footer_addr + SIZEOF_FOOTER;
  if (next_addr >= last_footer) { return NULL; }

  block_header_t* next = (block_header_t*)next_addr;
  if (!is_block_header_in_heap(next) || !is_footer_in_heap(next->footer)) {
    return NULL;
  }
  if (next->footer->header != next) { return NULL; }
  return next;
}

static block_header_t* get_previous(block_header_t* block) {
  if (!is_block_header_in_heap(block)) { return NULL; }
  if ((hmalloc_addr_t)block <
      first_available_vaddr + SIZEOF_HEADER + SIZEOF_FOOTER) {
    return NULL;
  }

  block_footer_t* previous_footer =
      (block_footer_t*)((hmalloc_addr_t)block - SIZEOF_FOOTER);
  if (!is_footer_in_heap(previous_footer)) { return NULL; }

  block_header_t* previous = previous_footer->header;
  if (!is_block_header_in_heap(previous)) { return NULL; }
  if ((hmalloc_addr_t)previous >= (hmalloc_addr_t)block) { return NULL; }
  if (previous->footer != previous_footer) { return NULL; }
  return previous;
}

static block_header_t* grow_heap(void) {
  return grow_heap_by(heap_grow_size++);
}

static bool is_block_header_in_heap(block_header_t* block) {
  return block != NULL && (hmalloc_addr_t)block >= first_available_vaddr &&
         (hmalloc_addr_t)block + SIZEOF_HEADER <= last_footer;
}

static bool is_footer_in_heap(block_footer_t* footer) {
  return footer != NULL && (hmalloc_addr_t)footer >= first_available_vaddr &&
         (hmalloc_addr_t)footer <= last_footer;
}

static block_header_t* grow_heap_by(size_t pages) {
  if (pages == 0) { return NULL; }

  hmalloc_addr_t addr_to_map = align_up(last_footer + SIZEOF_FOOTER, PAGE_SIZE);
  hmalloc_addr_t new_pages = map_heap_pages(addr_to_map, pages);
  if (new_pages == 0) { return NULL; }

  block_header_t* last_block = ((block_footer_t*)last_footer)->header;
  block_header_t* new_block =
      (block_header_t*)(last_footer + SIZEOF_FOOTER);
  hmalloc_addr_t difference =
      new_pages - (last_footer + SIZEOF_FOOTER);

  new_block->is_free = true;
  new_block->size_bytes =
      page_to_addr_base(pages) + difference - SIZEOF_HEADER - SIZEOF_FOOTER;
  new_block->next = NULL;
  block_footer_t* new_block_footer =
      (block_footer_t*)(((hmalloc_addr_t)new_block) + SIZEOF_HEADER +
                        new_block->size_bytes);
  new_block_footer->header = new_block;
  new_block->footer = new_block_footer;
  last_footer = (hmalloc_addr_t)new_block_footer;
  if (last_block->is_free) {
    remove_from_free_list(last_block);
    merge_blocks(last_block, new_block);
    new_block = last_block;
  }
  insert_free_block(new_block);
  return new_block;
}

static void merge_blocks(block_header_t* first, block_header_t* second) {
  memset(first->footer, 0, SIZEOF_FOOTER);
  first->footer = second->footer;
  first->footer->header = first;
  first->next = second->next;
  first->size_bytes += second->size_bytes + BLOCK_OVERHEAD;
  memset(second, 0, SIZEOF_HEADER);
}

static void __attribute__((noinline)) occupy_block(block_header_t* block,
                                                   size_t size) {
  block->is_free = false;
  hmalloc_addr_t size_needed_for_split = size + SIZEOF_HEADER + SIZEOF_FOOTER;
  if (block->size_bytes <= size_needed_for_split) {
    remove_from_free_list(block);
  } else {
    block_header_t* leftover = split_region(block, size);
    block->size_bytes = size;
    replace_free_list_block(block, leftover);
  }
  block->next = NULL;
}

static void remove_from_free_list(block_header_t* block) {
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

static void replace_free_list_block(block_header_t* old_block,
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

static void set_footer_at(block_header_t* block, hmalloc_addr_t addr) {
  block_footer_t* footer = (block_footer_t*)addr;
  footer->header = block;
  block->footer = footer;
}

static block_header_t* split_region(block_header_t* block, size_t size) {
  block_header_t* leftover =
      (block_header_t*)((hmalloc_addr_t)block + SIZEOF_HEADER +
                        SIZEOF_FOOTER + (hmalloc_addr_t)size);
  leftover->footer = block->footer;
  leftover->footer->header = leftover;

  set_footer_at(block, (hmalloc_addr_t)(leftover)-SIZEOF_FOOTER);
  leftover->size_bytes =
      block->size_bytes - size - SIZEOF_HEADER - SIZEOF_FOOTER;
  leftover->next = block->next;
  leftover->is_free = true;
  return leftover;
}
