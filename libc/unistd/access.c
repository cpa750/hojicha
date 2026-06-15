#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>

int access(const char* path, int amode) {
  if ((amode & ~(R_OK | W_OK | X_OK)) != 0) {
    errno = EINVAL;
    return -1;
  }

  stat_t st;
  if (stat(path, &st) < 0) { return -1; }

  if ((amode & X_OK) != 0 && (st.st_mode & S_IFMT) != S_IFREG) {
    errno = EACCES;
    return -1;
  }

  return 0;
}
