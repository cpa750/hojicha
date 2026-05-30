#include <fs/vfs.h>
#include <multitask/syscall_callbacks.h>
#include <stddef.h>

long syscall_open(const char* absolute_path, unsigned int flags) {
  vfs_file_t* file = NULL;
  long out_fd;
  vfs_status_t open_stat = vfs_open(absolute_path, flags, &file, &out_fd);
  if (open_stat != VFS_STATUS_OK) {
    return (long)-vfs_status_to_errno(open_stat);
  }
  return out_fd;
}
