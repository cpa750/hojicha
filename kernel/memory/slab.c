#include <hmalloc.h>
#include <memory/pmm.h>
#include <memory/slab.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <utils/bitmap.h>
#include <utils/irq.h>

#define SLAB_MAGIC       0x48534C4142ULL  // ASCII "HSLAB"
#define SLAB_BIG_CLASS   ((uint64_t)-1)
#define SLAB_CLASS_COUNT 10

typedef struct slab slab_t;
typedef struct slab_object_header slab_object_header_t;

struct slab_object_header {
  uint64_t magic;
  uint64_t class_idx;
  slab_t* slab;
};

struct slab {
  void* raw_page;
  void* page;
  bitmap_t bitmap;
  slab_t* next;
  uint64_t object_count;
  uint64_t free_count;
};

static const uint64_t slab_class_sizes[SLAB_CLASS_COUNT] =
    {32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384};
static slab_t* slab_classes[SLAB_CLASS_COUNT];

static int slab_class_for_size(size_t size) {
  size_t needed = size + sizeof(slab_object_header_t);
  for (int idx = 0; idx < SLAB_CLASS_COUNT; ++idx) {
    if (needed <= slab_class_sizes[idx]) { return idx; }
  }
  return -1;
}

static uint64_t slab_pages_for_class(int class_idx) {
  uint64_t class_size = slab_class_sizes[class_idx];
  return (class_size + PAGE_SIZE - 1) / PAGE_SIZE;
}

static uintptr_t slab_align_up(uintptr_t addr, uintptr_t alignment) {
  return (addr + alignment - 1) & ~(alignment - 1);
}

static slab_t* slab_grow(int class_idx) {
  uint64_t pages = slab_pages_for_class(class_idx);
  uint64_t bytes = pages * PAGE_SIZE;
  uint64_t class_size = slab_class_sizes[class_idx];
  uint64_t object_count = bytes / class_size;
  uint64_t bitmap_bytes = bitmap_storage_size(object_count);

  slab_t* slab = (slab_t*)malloc(sizeof(slab_t));
  uint8_t* bitmap_storage = (uint8_t*)calloc(bitmap_bytes, sizeof(uint8_t));
  void* raw_page = malloc(bytes + sizeof(uintptr_t) - 1);
  if (slab == NULL || bitmap_storage == NULL || raw_page == NULL) {
    free(slab);
    free(bitmap_storage);
    free(raw_page);
    return NULL;
  }
  void* page = (void*)slab_align_up((uintptr_t)raw_page, sizeof(uintptr_t));

  slab->raw_page = raw_page;
  slab->page = page;
  bitmap_init(&slab->bitmap, bitmap_storage, object_count, false);
  slab->next = slab_classes[class_idx];
  slab->object_count = object_count;
  slab->free_count = object_count;

  for (uint64_t idx = 0; idx < object_count; ++idx) {
    slab_object_header_t* header =
        (slab_object_header_t*)((uintptr_t)page + (idx * class_size));
    header->magic = SLAB_MAGIC;
    header->class_idx = (uint64_t)class_idx;
    header->slab = slab;
  }

  slab_classes[class_idx] = slab;
  return slab;
}

void* slab_alloc(size_t size) {
  if (size == 0) { size = 1; }

  int class_idx = slab_class_for_size(size);
  if (class_idx < 0) {
    slab_object_header_t* header =
        (slab_object_header_t*)malloc(sizeof(slab_object_header_t) + size);
    if (header == NULL) { return NULL; }

    header->magic = SLAB_MAGIC;
    header->class_idx = SLAB_BIG_CLASS;
    header->slab = NULL;
    return header + 1;
  }

  uint64_t irq_state = irq_store();
  slab_t* slab = slab_classes[class_idx];
  while (slab != NULL && slab->free_count == 0) { slab = slab->next; }
  if (slab == NULL) { slab = slab_grow(class_idx); }
  if (slab == NULL || slab->free_count == 0) {
    irq_load(irq_state);
    return NULL;
  }

  uint64_t object_idx = slab->object_count;
  if (!bitmap_find_clear(&slab->bitmap, &object_idx)) {
    slab = slab_grow(class_idx);
    if (slab == NULL) {
      irq_load(irq_state);
      return NULL;
    }
    if (!bitmap_find_clear(&slab->bitmap, &object_idx)) {
      irq_load(irq_state);
      return NULL;
    }
  }

  bitmap_set(&slab->bitmap, object_idx);
  slab->free_count--;
  slab_object_header_t* header =
      (slab_object_header_t*)((uintptr_t)slab->page +
                              (object_idx * slab_class_sizes[class_idx]));
  irq_load(irq_state);
  return header + 1;
}

void* slab_calloc(size_t size) {
  void* ret = slab_alloc(size);
  if (ret != NULL) { memset(ret, 0, size); }
  return ret;
}

void slab_free(void* ptr) {
  if (ptr == NULL) { return; }

  slab_object_header_t* header = ((slab_object_header_t*)ptr) - 1;
  if (header->magic != SLAB_MAGIC) { return; }

  if (header->class_idx == SLAB_BIG_CLASS) {
    header->magic = 0;
    free(header);
    return;
  }

  if (header->class_idx >= SLAB_CLASS_COUNT || header->slab == NULL) { return; }

  uint64_t irq_state = irq_store();
  slab_t* slab = header->slab;
  uint64_t class_size = slab_class_sizes[header->class_idx];
  uintptr_t header_addr = (uintptr_t)header;
  uintptr_t slab_base = (uintptr_t)slab->page;
  if (header_addr < slab_base) {
    irq_load(irq_state);
    return;
  }

  uintptr_t offset = header_addr - slab_base;
  if (offset % class_size != 0) {
    irq_load(irq_state);
    return;
  }

  uint64_t object_idx = offset / class_size;
  if (object_idx >= slab->object_count ||
      !bitmap_get(&slab->bitmap, object_idx)) {
    irq_load(irq_state);
    return;
  }

  bitmap_clear(&slab->bitmap, object_idx);
  slab->free_count++;
  irq_load(irq_state);
}
