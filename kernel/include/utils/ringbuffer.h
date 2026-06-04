#ifndef HOJICHA_RINGBUFFER_H
#define HOJICHA_RINGBUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ringbuffer ringbuffer_t;

/*
 * Creates a new ringbuffer with `size` `char`-sized slots.
 */
void ringbuffer_new(uint64_t size, ringbuffer_t** out);

/*
 * Frees a ringbuffer created with `ringbuffer_new()`.
 */
void ringbuffer_free(ringbuffer_t* r);

/*
 * Reads one character and advances the read position.
 */
bool ringbuffer_read(ringbuffer_t* r, char* out);

/*
 * Writes one character and advances the write position.
 */
void ringbuffer_write(ringbuffer_t* r, char value);

#endif  // HOJICHA_RINGBUFFER_H

