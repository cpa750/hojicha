#include <fs/vfs.h>
#include <multitask/syscall_callbacks.h>
#include <multitask/syscall_utils.h>
#include <stddef.h>

long syscall_mkdir(const char* path) {
  vfs_node_t* parent = NULL;
  const char* name = NULL;
  uint32_t name_len = 0;
  long lookup_status =
      syscall_lookup_parent_for_child(path, &parent, &name, &name_len);
  if (lookup_status != 0) { return lookup_status; }

  vfs_status_t status = vfs_mkdir(parent, name, name_len, NULL);
  vfs_vnode_release(parent);
  return status == VFS_STATUS_OK ? 0 : -vfs_status_to_errno(status);
}
