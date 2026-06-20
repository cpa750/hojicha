#include <errno.h>
#include <fs/vfs.h>
#include <multitask/syscall_callbacks.h>
#include <multitask/syscall_utils.h>
#include <stddef.h>
#include <stdlib.h>

long syscall_symlink(const char* target, const char* link_path) {
  if (target == NULL || link_path == NULL) { return -EINVAL; }
  if (!syscall_is_uaddr(target, SYSCALL_USER_STRING_MAX) ||
      !syscall_is_uaddr(link_path, SYSCALL_USER_STRING_MAX)) {
    return -EINVAL;
  }

  char* target_copy = syscall_utok_strcpy(target, SYSCALL_USER_STRING_MAX);
  if (target_copy == NULL) { return -ENOMEM; }

  char* link_path_copy =
      syscall_utok_strcpy(link_path, SYSCALL_USER_STRING_MAX);
  if (link_path_copy == NULL) {
    free(target_copy);
    return -ENOMEM;
  }

  vfs_status_t status = vfs_symlink(target_copy, link_path_copy);
  free(link_path_copy);
  free(target_copy);
  return status == VFS_STATUS_OK ? 0 : -vfs_status_to_errno(status);
}
