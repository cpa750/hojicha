#include <errno.h>
#include <fs/vfs.h>
#include <multitask/syscall_utils.h>
#include <stddef.h>

long syscall_lookup_parent_for_child(const char* path,
                                     vfs_node_t** parent_out,
                                     const char** name_out,
                                     uint32_t* name_len_out) {
  if (path == NULL || parent_out == NULL || name_out == NULL ||
      name_len_out == NULL) {
    return -EINVAL;
  }

  vfs_status_t status =
      vfs_lookup_parent(path, parent_out, name_out, name_len_out);
  if (status != VFS_STATUS_OK) { return -vfs_status_to_errno(status); }
  if (*name_out == NULL || *name_len_out == 0) {
    vfs_vnode_release(*parent_out);
    *parent_out = NULL;
    return -EINVAL;
  }

  return 0;
}
