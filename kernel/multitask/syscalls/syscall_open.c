#include <errno.h>
#include <fcntl.h>
#include <fs/vfs.h>
#include <multitask/syscall_callbacks.h>
#include <stddef.h>

static long translate_open_flags(unsigned int userspace_flags,
                                 uint32_t* vfs_flags_out) {
  const unsigned int supported_flags =
      O_ACCMODE | O_CREAT | O_DIRECTORY;
  if ((userspace_flags & ~supported_flags) != 0) { return -EINVAL; }

  uint32_t vfs_flags = 0;
  if (userspace_flags & O_DIRECTORY) {
    if ((userspace_flags & O_ACCMODE) != O_RDONLY) { return -EINVAL; }
    vfs_flags |= VFS_OPEN_DIRECTORY;
  } else {
    switch (userspace_flags & O_ACCMODE) {
      case O_RDONLY:
        vfs_flags |= VFS_OPEN_READ;
        break;
      case O_WRONLY:
        vfs_flags |= VFS_OPEN_WRITE;
        break;
      case O_RDWR:
        vfs_flags |= VFS_OPEN_READ | VFS_OPEN_WRITE;
        break;
      default:
        return -EINVAL;
    }
  }

  if (userspace_flags & O_CREAT) { vfs_flags |= VFS_OPEN_CREATE; }

  *vfs_flags_out = vfs_flags;
  return 0;
}

long syscall_open(const char* absolute_path, unsigned int flags) {
  uint32_t vfs_flags = 0;
  long translate_status = translate_open_flags(flags, &vfs_flags);
  if (translate_status != 0) { return translate_status; }

  vfs_file_t* file = NULL;
  long out_fd;
  vfs_status_t open_stat = vfs_open(absolute_path, vfs_flags, &file, &out_fd);
  if (open_stat != VFS_STATUS_OK) {
    return (long)-vfs_status_to_errno(open_stat);
  }
  return out_fd;
}
