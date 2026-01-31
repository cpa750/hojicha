#ifndef PIT_H
#define PIT_H

#include <stdint.h>

/*
 * The type of callback to be registered with `pit_register_callback()`. This
 * function is passed the current number of ticks since PIT init.
 */
typedef void (*pit_callback_func_t)(uint64_t timestamp);

typedef struct pit_callback pit_callback_t;
struct pit_callback {
  pit_callback_func_t callback_func;
  pit_callback_t* next;
};

typedef struct pit_state pit_state_t;
struct pit_state;

void initialize_pit();
void pit_handle();

/*
 * Gets the time in nanoseconds since the PIT has been initialized.
 */
uint64_t pit_get_ns_elapsed_since_init(pit_state_t* pit);

/*
 * Registers a callback to be called every tick. The callback's `next` field
 * will be set to NULL. The callback is passed the current number of ticks since
 * PIT init.
 */
void pit_register_callback(pit_callback_t* callback);

#endif  // PIT_H
