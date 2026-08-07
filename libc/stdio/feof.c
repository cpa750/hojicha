#include <internal/__stdio.h>
#include <stddef.h>
#include <stdio.h>

int feof(FILE* stream) {
  if (stream == NULL) { return 0; }
  return (stream->flags & __HOJICHA_FILE_EOF) != 0;
}
