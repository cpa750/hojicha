#include <stdlib.h>
#include <string.h>

char** environ;

char* getenv(const char* name) {
  if (name == NULL || name[0] == '\0' || environ == NULL) { return NULL; }

  size_t name_len = strlen(name);
  for (int i = 0; environ[i] != NULL; ++i) {
    if (strncmp(environ[i], name, name_len) == 0 && environ[i][name_len] == '=') {
      return environ[i] + name_len + 1;
    }
  }

  return NULL;
}
