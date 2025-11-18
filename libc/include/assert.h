#ifndef ASSERT_H
#define ASSERT_H

#ifdef __cplusplus
extern "C" {
#endif

#define assert(cond)                                                           \
  do {                                                                         \
    if (!(cond)) { assert_false(#cond, __FILE__, __LINE__); }                  \
  } while (0)

void assert_false(const char* cond, const char* file, const int line);

#ifdef __cplusplus
}
#endif

#endif
