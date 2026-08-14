#ifndef HOJICHA_MP_SLEEP_H
#define HOJICHA_MP_SLEEP_H

#include <mp/proc.h>
#include <stdint.h>

void block_current_proc(uint8_t reason);
void sleep_current(uint64_t s);
void sleep_current_ns(uint64_t ns);
void sleep_proc_until(proc_t* process, uint64_t timestamp);
void wake_proc(proc_t* process);
void wake_procs_before_timestamp(uint64_t timestamp);

#endif  // HOJICHA_MP_SLEEP_H
