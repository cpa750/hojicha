#include <fs/vfs.h>
#include <multitask/syscall_callbacks.h>
#include <multitask/syscall_utils.h>
#include <stddef.h>

long syscall_close(long fd) {
  vfs_status_t status = syscall_close_fd(fd);
  return status == VFS_STATUS_OK ? 0 : -vfs_status_to_errno(status);
}
