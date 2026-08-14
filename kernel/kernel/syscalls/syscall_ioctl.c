#include <errno.h>
#include <fs/vfs.h>
#include <kernel/g_kernel.h>
#include <mp/proc.h>
#include <mp/scheduler.h>
#include <kernel/syscall_callbacks.h>
#include <kernel/syscall_utils.h>
#include <stddef.h>

long syscall_ioctl(long fd, unsigned long request, void* arg) {
  if (arg != NULL && !syscall_is_uaddr(arg, 1)) { return -EINVAL; }

  vfs_file_t* file = proc_fd_get(g_kernel.current_process, fd);
  if (file == NULL) { return -EBADF; }

  vfs_status_t status = vfs_ioctl(file, request, arg);
  if (status != VFS_STATUS_OK) { return -vfs_status_to_errno(status); }
  return 0;
}
