#ifndef HOJICHA_TIME_H
#define HOJICHA_TIME_H

#include <stdint.h>

#define CLOCK_MONOTONIC 1
#define CLOCK_REALTIME  2

typedef int clockid_t;

// https://www.man7.org/linux/man-pages/man3/timespec.3type.html

typedef struct timespec timespec_t;
struct timespec {
  int64_t tv_sec;
  int64_t tv_nsec;
};

#ifdef __cplusplus
extern "C" {
#endif

int clock_gettime(clockid_t clockid, struct timespec* tp);
unsigned long nanosleep(unsigned long ns);

#ifdef __cplusplus
}
#endif

#endif  // HOJICHA_TIME_H
