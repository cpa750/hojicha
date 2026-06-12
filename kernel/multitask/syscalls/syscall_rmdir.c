#include <errno.h>
#include <fs/vfs.h>
#include <multitask/syscall_callbacks.h>
#include <multitask/syscall_utils.h>
#include <stddef.h>
#include <stdlib.h>

long syscall_rmdir(const char* path) {
  if (path == NULL) { return -EINVAL; }
  if (!syscall_is_uaddr(path, SYSCALL_USER_STRING_MAX)) {
    return -EINVAL;
  }

  char* path_copy = syscall_utok_strcpy(path, SYSCALL_USER_STRING_MAX);
  if (path_copy == NULL) { return -ENOMEM; }

  vfs_node_t* parent = NULL;
  const char* name = NULL;
  uint32_t name_len = 0;
  long lookup_status =
      syscall_lookup_parent_for_child(path_copy, &parent, &name, &name_len);
  if (lookup_status != 0) {
    free(path_copy);
    return lookup_status;
  }

  vfs_status_t status = vfs_rmdir(parent, name, name_len, 0);
  vfs_vnode_release(parent);
  free(path_copy);
  return status == VFS_STATUS_OK ? 0 : -vfs_status_to_errno(status);
}
