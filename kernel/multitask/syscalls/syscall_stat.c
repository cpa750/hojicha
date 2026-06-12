#include <errno.h>
#include <fs/vfs.h>
#include <multitask/syscall_callbacks.h>
#include <multitask/syscall_utils.h>
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

static long copy_vfs_stat(vfs_stat_t* vfs_stat_buf, stat_t* stat_buf) {
  if (vfs_stat_buf == NULL || stat_buf == NULL) { return -EINVAL; }

  memset(stat_buf, 0, sizeof(stat_t));
  stat_buf->st_mode = vfs_type_to_mode(vfs_stat_buf->type);
  stat_buf->st_size = vfs_stat_buf->size;
  return 0;
}

long syscall_stat(const char* path, stat_t* stat_buf) {
  if (path == NULL || stat_buf == NULL) { return -EINVAL; }
  if (!syscall_is_uaddr(path, SYSCALL_USER_STRING_MAX)) {
    return -EINVAL;
  }
  if (!syscall_is_uaddr(stat_buf, sizeof(stat_t))) { return -EINVAL; }

  char* path_copy = syscall_utok_strcpy(path, SYSCALL_USER_STRING_MAX);
  if (path_copy == NULL) { return -ENOMEM; }

  vfs_stat_t* vfs_stat_buf = NULL;
  vfs_status_t status = vfs_stat(path_copy, &vfs_stat_buf);
  free(path_copy);
  if (status != VFS_STATUS_OK) { return -vfs_status_to_errno(status); }

  long ret = copy_vfs_stat(vfs_stat_buf, stat_buf);

  free(vfs_stat_buf);
  return ret;
}

long syscall_fstat(long fd, stat_t* stat_buf) {
  if (stat_buf == NULL) { return -EINVAL; }
  if (!syscall_is_uaddr(stat_buf, sizeof(stat_t))) { return -EINVAL; }

  vfs_file_t* file = NULL;
  vfs_status_t status = vfs_resolve_fd(fd, &file);
  if (status != VFS_STATUS_OK) { return -vfs_status_to_errno(status); }

  vfs_stat_t* vfs_stat_buf = NULL;
  status = vfs_fstat(file, &vfs_stat_buf);
  if (status != VFS_STATUS_OK) { return -vfs_status_to_errno(status); }

  long ret = copy_vfs_stat(vfs_stat_buf, stat_buf);
  free(vfs_stat_buf);
  return ret;
}
