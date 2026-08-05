#ifndef HOJICHA_UTILS_BITMAP_H
#define HOJICHA_UTILS_BITMAP_H

#include <stdbool.h>
#include <stdint.h>

typedef struct bitmap bitmap_t;
struct bitmap {
  uint8_t* storage;
  uint64_t bit_count;
  uint64_t byte_count;
  uint64_t lowest_clear_idx;
  uint64_t highest_clear_idx;
};

uint64_t bitmap_storage_size(uint64_t bit_count);
void bitmap_init(bitmap_t* bitmap,
                 void* storage,
                 uint64_t bit_count,
                 bool default_set);
bool bitmap_get(bitmap_t* bitmap, uint64_t idx);
bool bitmap_set(bitmap_t* bitmap, uint64_t idx);
bool bitmap_clear(bitmap_t* bitmap, uint64_t idx);
bool bitmap_find_clear(bitmap_t* bitmap, uint64_t* out);

#endif  // HOJICHA_UTILS_BITMAP_H
