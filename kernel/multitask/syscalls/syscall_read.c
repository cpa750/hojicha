#include <errno.h>
#include <fs/vfs.h>
#include <kernel/g_kernel.h>
#include <multitask/scheduler.h>
#include <multitask/syscall_callbacks.h>
#include <stddef.h>

long syscall_read(long fd, void* buf, long count) {
  vfs_file_t* file = sched_pb_fd_get(g_kernel.current_process, fd);
  if (file == NULL) { return -EBADF; }

  long ret;
  vfs_status_t read_stat = vfs_read(file, buf, count, &ret);
  if (read_stat != VFS_STATUS_OK) { return -vfs_status_to_errno(read_stat); }
  return ret;
}

