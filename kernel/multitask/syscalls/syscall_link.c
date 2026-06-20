#include <errno.h>
#include <fs/vfs.h>
#include <multitask/syscall_callbacks.h>
#include <multitask/syscall_utils.h>
#include <stddef.h>
#include <stdlib.h>

long syscall_link(const char* old_path, const char* new_path) {
  if (old_path == NULL || new_path == NULL) { return -EINVAL; }
  if (!syscall_is_uaddr(old_path, SYSCALL_USER_STRING_MAX) ||
      !syscall_is_uaddr(new_path, SYSCALL_USER_STRING_MAX)) {
    return -EINVAL;
  }

  char* old_path_copy = syscall_utok_strcpy(old_path, SYSCALL_USER_STRING_MAX);
  if (old_path_copy == NULL) { return -ENOMEM; }

  char* new_path_copy = syscall_utok_strcpy(new_path, SYSCALL_USER_STRING_MAX);
  if (new_path_copy == NULL) {
    free(old_path_copy);
    return -ENOMEM;
  }

  vfs_status_t status = vfs_link(old_path_copy, new_path_copy);
  free(new_path_copy);
  free(old_path_copy);
  return status == VFS_STATUS_OK ? 0 : -vfs_status_to_errno(status);
}
