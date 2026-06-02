#include <errno.h>
#include <fs/vfs.h>
#include <multitask/syscall_callbacks.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static uint32_t vfs_type_to_mode(vfs_node_type_t type) {
  switch (type) {
    case VFS_NODE_FILE:
      return S_IFREG;
    case VFS_NODE_DIR:
      return S_IFDIR;
    case VFS_NODE_SYMLINK:
      return S_IFLNK;
    case VFS_NODE_DEVICE:
      return S_IFCHR;
    default:
      return 0;
  }
}

long syscall_stat(const char* path, stat_t* stat_buf) {
  if (path == NULL || stat_buf == NULL) { return -EINVAL; }

  vfs_stat_t* vfs_stat_buf = NULL;
  vfs_status_t status = vfs_stat(path, &vfs_stat_buf);
  if (status != VFS_STATUS_OK) { return -vfs_status_to_errno(status); }
  if (vfs_stat_buf == NULL) { return -EINVAL; }

  memset(stat_buf, 0, sizeof(stat_t));
  stat_buf->st_mode = vfs_type_to_mode(vfs_stat_buf->type);
  stat_buf->st_size = vfs_stat_buf->size;

  free(vfs_stat_buf);
  return 0;
}
