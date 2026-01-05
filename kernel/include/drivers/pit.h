#ifndef PIT_H
#define PIT_H

#include <stdint.h>

typedef struct pit_state pit_state_t;
struct pit_state;

void initialize_pit();
void handle_pit();
uint64_t pit_get_ns_elapsed_since_init(pit_state_t* pit);

#endif  // PIT_H
