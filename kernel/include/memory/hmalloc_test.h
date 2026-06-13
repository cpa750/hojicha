#ifndef HMALLOC_TEST_H
#define HMALLOC_TEST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void hmalloc_test(void);
void hmalloc_stress_test(void);

#if defined(__stress_hmalloc)
uint64_t hmalloc_debug_last_footer(void);
void* hmalloc_debug_last_block_user(void);
size_t hmalloc_debug_last_block_size(void);
bool hmalloc_debug_last_block_is_free(void);
#endif

#endif  // HMALLOC_TEST_H
