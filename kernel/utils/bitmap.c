#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <utils/bitmap.h>

static bool bitmap_find_clear_forward(bitmap_t* bitmap,
                                      uint64_t start,
                                      uint64_t end,
                                      uint64_t* out);
static bool bitmap_find_clear_backward(bitmap_t* bitmap,
                                       uint64_t start,
                                       uint64_t end,
                                       uint64_t* out);
static uint8_t bitmap_get_highest_zero_bit(uint8_t num);
static uint8_t bitmap_get_lowest_zero_bit(uint8_t num);
static void bitmap_refresh_bounds_after_set(bitmap_t* bitmap, uint64_t idx);

uint64_t bitmap_storage_size(uint64_t bit_count) {
  return (bit_count + 7) >> 3;
}

void bitmap_init(bitmap_t* bitmap,
                 void* storage,
                 uint64_t bit_count,
                 bool default_set) {
  if (bitmap == NULL) { return; }

  bitmap->storage = (uint8_t*)storage;
  bitmap->bit_count = bit_count;
  bitmap->byte_count = bitmap_storage_size(bit_count);
  bitmap->lowest_clear_idx = bit_count;
  bitmap->highest_clear_idx = bit_count;

  if (storage == NULL || bit_count == 0) { return; }

  memset(storage, default_set ? 0xFF : 0, bitmap->byte_count);
  if (!default_set) {
    bitmap->lowest_clear_idx = 0;
    bitmap->highest_clear_idx = bit_count - 1;
  }
}

bool bitmap_get(bitmap_t* bitmap, uint64_t idx) {
  if (bitmap == NULL || bitmap->storage == NULL || idx >= bitmap->bit_count) {
    return false;
  }

  return (bitmap->storage[idx >> 3] & (1 << (idx & 7))) != 0;
}

bool bitmap_set(bitmap_t* bitmap, uint64_t idx) {
  if (bitmap == NULL || bitmap->storage == NULL || idx >= bitmap->bit_count) {
    return false;
  }
  if (bitmap_get(bitmap, idx)) { return true; }

  bitmap->storage[idx >> 3] |= 1 << (idx & 7);
  bitmap_refresh_bounds_after_set(bitmap, idx);
  return true;
}

bool bitmap_clear(bitmap_t* bitmap, uint64_t idx) {
  if (bitmap == NULL || bitmap->storage == NULL || idx >= bitmap->bit_count) {
    return false;
  }
  if (!bitmap_get(bitmap, idx)) { return true; }

  bitmap->storage[idx >> 3] &= ~(1 << (idx & 7));
  if (bitmap->lowest_clear_idx >= bitmap->bit_count ||
      bitmap->highest_clear_idx >= bitmap->bit_count) {
    bitmap->lowest_clear_idx = idx;
    bitmap->highest_clear_idx = idx;
    return true;
  }

  if (idx < bitmap->lowest_clear_idx) { bitmap->lowest_clear_idx = idx; }
  if (idx > bitmap->highest_clear_idx) { bitmap->highest_clear_idx = idx; }
  return true;
}

bool bitmap_find_clear(bitmap_t* bitmap, uint64_t* out) {
  if (bitmap == NULL || out == NULL || bitmap->storage == NULL ||
      bitmap->lowest_clear_idx >= bitmap->bit_count ||
      bitmap->highest_clear_idx >= bitmap->bit_count) {
    return false;
  }

  return bitmap_find_clear_forward(
      bitmap, bitmap->lowest_clear_idx, bitmap->highest_clear_idx, out);
}

static void bitmap_refresh_bounds_after_set(bitmap_t* bitmap, uint64_t idx) {
  if (bitmap == NULL) { return; }
  if (bitmap->lowest_clear_idx >= bitmap->bit_count ||
      bitmap->highest_clear_idx >= bitmap->bit_count) {
    return;
  }

  if (idx == bitmap->lowest_clear_idx) {
    uint64_t next = bitmap->bit_count;
    if (bitmap_find_clear_forward(
            bitmap, idx + 1, bitmap->highest_clear_idx, &next)) {
      bitmap->lowest_clear_idx = next;
    } else {
      bitmap->lowest_clear_idx = bitmap->bit_count;
      bitmap->highest_clear_idx = bitmap->bit_count;
      return;
    }
  }

  if (idx == bitmap->highest_clear_idx) {
    uint64_t prev = bitmap->bit_count;
    if (idx > 0 &&
        bitmap_find_clear_backward(
            bitmap, idx - 1, bitmap->lowest_clear_idx, &prev)) {
      bitmap->highest_clear_idx = prev;
    } else {
      bitmap->lowest_clear_idx = bitmap->bit_count;
      bitmap->highest_clear_idx = bitmap->bit_count;
    }
  }
}

static bool bitmap_find_clear_forward(bitmap_t* bitmap,
                                      uint64_t start,
                                      uint64_t end,
                                      uint64_t* out) {
  if (bitmap == NULL || out == NULL || start > end ||
      start >= bitmap->bit_count) {
    return false;
  }
  if (end >= bitmap->bit_count) { end = bitmap->bit_count - 1; }

  uint64_t start_byte = start >> 3;
  uint64_t end_byte = end >> 3;
  for (uint64_t byte_idx = start_byte; byte_idx <= end_byte; ++byte_idx) {
    uint8_t byte = bitmap->storage[byte_idx];
    if (byte_idx == start_byte) {
      uint8_t first_bit = start & 7;
      byte |= (1 << first_bit) - 1;
    }
    if (byte_idx == end_byte) {
      uint8_t last_bit = end & 7;
      if (last_bit < 7) { byte |= 0xFF << (last_bit + 1); }
    }

    if (byte != 0xFF) {
      *out = (byte_idx << 3) + bitmap_get_lowest_zero_bit(byte);
      return true;
    }
  }
  return false;
}

static bool bitmap_find_clear_backward(bitmap_t* bitmap,
                                       uint64_t start,
                                       uint64_t end,
                                       uint64_t* out) {
  if (bitmap == NULL || out == NULL || start < end ||
      end >= bitmap->bit_count) {
    return false;
  }
  if (start >= bitmap->bit_count) { start = bitmap->bit_count - 1; }

  uint64_t start_byte = start >> 3;
  uint64_t end_byte = end >> 3;
  for (uint64_t byte_idx = start_byte;; --byte_idx) {
    uint8_t byte = bitmap->storage[byte_idx];
    if (byte_idx == start_byte) {
      uint8_t first_bit = start & 7;
      if (first_bit < 7) { byte |= 0xFF << (first_bit + 1); }
    }
    if (byte_idx == end_byte) {
      uint8_t last_bit = end & 7;
      byte |= (1 << last_bit) - 1;
    }

    if (byte != 0xFF) {
      *out = (byte_idx << 3) + bitmap_get_highest_zero_bit(byte);
      return true;
    }
    if (byte_idx == end_byte) { break; }
  }
  return false;
}

static uint8_t bitmap_get_lowest_zero_bit(uint8_t num) {
  for (int idx = 0; idx < 8; ++idx) {
    if (~(num >> idx) & 0b1) { return idx; }
  }
  return 7;
}

static uint8_t bitmap_get_highest_zero_bit(uint8_t num) {
  for (int idx = 7; idx >= 0; --idx) {
    if (~(num >> idx) & 0b1) { return idx; }
  }
  return 0;
}
