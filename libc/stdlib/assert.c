#include <stdio.h>
#include <stdlib.h>

__attribute__((__noreturn__)) void assert_false(const char* cond,
                                                const char* file,
                                                const int line) {
  printf("Assert failed at %s (%d): %s does not hold true.\n", file, line,
         cond);
  abort();
}

