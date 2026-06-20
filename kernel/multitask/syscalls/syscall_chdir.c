#include <errno.h>
#include <fcntl.h>
#include <fs/vfs.h>
#include <kernel/g_kernel.h>
#include <multitask/scheduler.h>
#include <multitask/syscall_callbacks.h>
#include <multitask/syscall_utils.h>
#include <stddef.h>
#include <stdlib.h>

long syscall_chdir(const char* target) {
  if (target == NULL) { return -EINVAL; }
  if (!syscall_is_uaddr(target, SYSCALL_USER_STRING_MAX)) { return -EINVAL; }

  char* path = syscall_utok_strcpy(target, SYSCALL_USER_STRING_MAX);
  if (path == NULL) { return -ENOMEM; }

  vfs_node_t* node = NULL;
  vfs_status_t lookup_status = vfs_lookup(path, &node);
  free(path);
  if (lookup_status != VFS_STATUS_OK) {
    vfs_vnode_release(node);
    return (long)-vfs_status_to_errno(lookup_status);
  }
  if (node->type != VFS_NODE_DIR) {
    vfs_vnode_release(node);
    return (long)-vfs_status_to_errno(VFS_STATUS_NOTDIR);
  }
  sched_pb_set_cwd(g_kernel.current_process, node);
  return 0;
}

long syscall_fchdir(long target_fd) {
  vfs_file_t* file = sched_pb_fd_get(g_kernel.current_process, target_fd);
  if (file == NULL) { return -EBADF; }
  if (file->vnode->type != VFS_NODE_DIR) {
    return (long)-vfs_status_to_errno(VFS_STATUS_NOTDIR);
  }
  sched_pb_set_cwd(g_kernel.current_process, file->vnode);
  return 0;
}
