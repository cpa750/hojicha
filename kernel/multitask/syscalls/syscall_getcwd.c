#include <errno.h>
#include <fs/vfs.h>
#include <kernel/g_kernel.h>
#include <multitask/scheduler.h>
#include <multitask/syscall_callbacks.h>
#include <multitask/syscall_utils.h>
#include <stddef.h>

long syscall_getcwd(char* buf, unsigned long size) {
  if (buf == NULL) { return -EINVAL; }
  if (!syscall_is_uaddr(buf, size)) { return -EINVAL; }

  vfs_node_t* cwd = sched_pb_get_cwd(g_kernel.current_process);
  vfs_status_t status = vfs_getcwd(cwd, buf, size);
  if (status != VFS_STATUS_OK) { return -vfs_status_to_errno(status); }

  return (long)buf;
}
