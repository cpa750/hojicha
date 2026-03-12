#include <time.h>
#include <unistd.h>

int main(void) {
  sleep(5);
  nanosleep(500000);
  return 2 + 2 - 1;  // Big maths
}

