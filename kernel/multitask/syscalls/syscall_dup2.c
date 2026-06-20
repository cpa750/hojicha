#include <errno.h>
#include <fs/vfs.h>
#include <kernel/g_kernel.h>
#include <multitask/scheduler.h>
#include <multitask/syscall_callbacks.h>
#include <multitask/syscall_utils.h>

long syscall_dup2(long oldfd, long newfd) {
  if (oldfd < 0 || newfd < 0 || newfd >= MAX_FDS) { return -EBADF; }

  vfs_file_t* file = sched_pb_fd_get(g_kernel.current_process, oldfd);
  if (file == NULL) { return -EBADF; }

  if (oldfd == newfd) { return newfd; }

  if (sched_pb_fd_get(g_kernel.current_process, newfd) != NULL) {
    vfs_status_t status = syscall_close_fd(newfd);
    if (status != VFS_STATUS_OK) { return -vfs_status_to_errno(status); }
  }

  vfs_file_borrow(file);
  sched_pb_fd_set(g_kernel.current_process, newfd, file);
  return newfd;
}
