#include <assert.h>
#include <hmalloc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SMALL_ALLOC_COUNT 64
#define SMALL_ALLOC_SIZE  128
#define LARGE_ALLOC_SIZE  65536
#define CHURN_COUNT       48

static void fill_pattern(uint8_t* ptr, size_t size, uint8_t pattern) {
  memset(ptr, pattern, size);
}

static void assert_pattern(uint8_t* ptr, size_t size, uint8_t pattern) {
  for (size_t i = 0; i < size; ++i) { assert(ptr[i] == pattern); }
}

int main(void) {
  printf("hmalloc userspace test start\n");

  uint8_t* small[SMALL_ALLOC_COUNT] = {0};
  for (size_t i = 0; i < SMALL_ALLOC_COUNT; ++i) {
    small[i] = hmalloc(SMALL_ALLOC_SIZE);
    assert(small[i] != NULL);
    fill_pattern(small[i], SMALL_ALLOC_SIZE, (uint8_t)(0x20 + i));
  }

  for (size_t i = 0; i < SMALL_ALLOC_COUNT; ++i) {
    assert_pattern(small[i], SMALL_ALLOC_SIZE, (uint8_t)(0x20 + i));
  }

  for (size_t i = 0; i < SMALL_ALLOC_COUNT; i += 2) {
    hfree(small[i]);
    small[i] = NULL;
  }

  for (size_t i = 0; i < SMALL_ALLOC_COUNT; i += 2) {
    small[i] = hmalloc(SMALL_ALLOC_SIZE);
    assert(small[i] != NULL);
    fill_pattern(small[i], SMALL_ALLOC_SIZE, (uint8_t)(0x80 + i));
  }

  uint8_t* large = hmalloc(LARGE_ALLOC_SIZE);
  assert(large != NULL);
  fill_pattern(large, LARGE_ALLOC_SIZE, 0xA5);

  uint8_t* churn[CHURN_COUNT] = {0};
  for (size_t i = 0; i < CHURN_COUNT; ++i) {
    size_t alloc_size = 512 + (i * 37);
    churn[i] = malloc(alloc_size);
    assert(churn[i] != NULL);
    fill_pattern(churn[i], alloc_size, (uint8_t)(0x40 + i));
  }

  assert_pattern(large, LARGE_ALLOC_SIZE, 0xA5);

  uint8_t* zeroed = calloc(256, sizeof(uint32_t));
  assert(zeroed != NULL);
  for (size_t i = 0; i < 256 * sizeof(uint32_t); ++i) {
    assert(zeroed[i] == 0);
  }

  for (size_t i = 0; i < CHURN_COUNT; ++i) { free(churn[i]); }
  free(zeroed);
  hfree(large);
  for (size_t i = 0; i < SMALL_ALLOC_COUNT; ++i) { hfree(small[i]); }

  printf("hmalloc userspace test passed\n");
  return 0;
}
