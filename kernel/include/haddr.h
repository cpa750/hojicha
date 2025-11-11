#ifndef HADDR_H
#define HADDR_H

#include <stdint.h>

#if defined(h32)
typedef uint32_t haddr_t;
#elif defined(h64)
typedef uint64_t haddr_t;
#endif

#endif // HADDR_H

