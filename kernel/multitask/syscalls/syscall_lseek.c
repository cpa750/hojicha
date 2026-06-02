#include <errno.h>
#include <fs/vfs.h>
#include <multitask/syscall_callbacks.h>
#include <stddef.h>
#include <unistd.h>

long syscall_lseek(long fd, long offset, int whence) {
  if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
    return -EINVAL;
  }

  vfs_file_t* file = NULL;
  vfs_status_t status = vfs_resolve_fd(fd, &file);
  if (status != VFS_STATUS_OK) { return -vfs_status_to_errno(status); }

  uint64_t new_pos = 0;
  status = vfs_seek(file, offset, (vfs_seek_whence_t)whence, &new_pos);
  if (status != VFS_STATUS_OK) { return -vfs_status_to_errno(status); }

  return new_pos;
}
