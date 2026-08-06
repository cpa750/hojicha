#ifndef HOJICHA_STRINGS_H
#define HOJICHA_STRINGS_H

#include <stddef.h>
#include <sys/cdefs.h>

#ifdef __cplusplus
extern "C" {
#endif

int strcasecmp(const char* a, const char* b);
int strncasecmp(const char* a, const char* b, size_t len);

#ifdef __cplusplus
}
#endif

#endif  // HOJICHA_STRINGS_H
