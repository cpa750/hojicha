#include <internal/__stdio.h>
#include <stddef.h>
#include <stdio.h>

int ferror(FILE* stream) {
  if (stream == NULL) { return 0; }
  return (stream->flags & __HOJICHA_FILE_ERROR) != 0;
}
