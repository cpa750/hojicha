#ifndef HOJICHA_KTIME_H
#define HOJICHA_KTIME_H

#include <stdint.h>

void ktime_initialize();

uint64_t uptime();
int64_t unix_time();

#endif  // HOJICHA_KTIME_H

