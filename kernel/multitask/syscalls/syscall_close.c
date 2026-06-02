#include <fs/vfs.h>
#include <multitask/syscall_callbacks.h>
#include <stddef.h>

long syscall_close(long fd) {
  vfs_file_t* file = NULL;
  vfs_status_t status = vfs_resolve_fd(fd, &file);
  if (status != VFS_STATUS_OK) { return -vfs_status_to_errno(status); }

  status = vfs_close(file);
  return status == VFS_STATUS_OK ? 0 : -vfs_status_to_errno(status);
}
