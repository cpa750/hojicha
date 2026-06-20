#include <errno.h>
#include <fs/vfs.h>
#include <multitask/syscall_callbacks.h>
#include <multitask/syscall_utils.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

long syscall_readlink(const char* path, char* buf, long bufsiz) {
  if (bufsiz < 0) { return -EINVAL; }
  if (path == NULL || (buf == NULL && bufsiz > 0)) { return -EINVAL; }
  if (!syscall_is_uaddr(path, SYSCALL_USER_STRING_MAX) ||
      !syscall_is_uaddr(buf, (size_t)bufsiz)) {
    return -EINVAL;
  }

  char* path_copy = syscall_utok_strcpy(path, SYSCALL_USER_STRING_MAX);
  if (path_copy == NULL) { return -ENOMEM; }

  size_t buffer_len = (size_t)bufsiz;
  char* kernel_buf = calloc(1, buffer_len == 0 ? 1 : buffer_len);
  if (kernel_buf == NULL) {
    free(path_copy);
    return -ENOMEM;
  }

  uint64_t bytes_read = 0;
  vfs_status_t status =
      vfs_readlink(path_copy, kernel_buf, buffer_len, &bytes_read);
  if (status == VFS_STATUS_OK && bytes_read > 0) {
    memcpy(buf, kernel_buf, bytes_read);
  }

  free(kernel_buf);
  free(path_copy);
  return status == VFS_STATUS_OK ? (long)bytes_read
                                 : -vfs_status_to_errno(status);
}
