#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#define __HOJICHA_USLEEP_NS_MULTIPLIER 1000UL

int usleep(unsigned long usec) {
  if (usec > (unsigned long)-1 / __HOJICHA_USLEEP_NS_MULTIPLIER) {
    errno = EINVAL;
    return -1;
  }

  nanosleep(usec * __HOJICHA_USLEEP_NS_MULTIPLIER);
  return 0;
}
