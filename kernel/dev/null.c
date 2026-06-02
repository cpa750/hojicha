#include <dev/null.h>
#include <fs/vfs.h>
#include <fs/vfs_utils.h>
#include <stdlib.h>

static vfs_status_t null_open(vfs_node_t* vnode,
                              uint32_t flags,
                              vfs_file_t** out);
static vfs_status_t null_close(vfs_file_t* file);
static vfs_status_t null_read(vfs_file_t* file,
                              void* buffer,
                              uint64_t len,
                              uint64_t* bytes_read_out);
static vfs_status_t null_write(vfs_file_t* file,
                               void* buffer,
                               uint64_t len,
                               uint64_t* bytes_written_out);
static vfs_status_t null_stat(vfs_node_t* vnode, vfs_stat_t** out);

devfs_device_t* null_dev_new(void) {
  vfs_file_ops_t* file_ops = calloc(1, sizeof(vfs_file_ops_t));
  vfs_node_ops_t* node_ops = calloc(1, sizeof(vfs_node_ops_t));
  if (file_ops == NULL || node_ops == NULL) {
    free(file_ops);
    free(node_ops);
    return NULL;
  }

  file_ops->read = null_read;
  file_ops->write = null_write;
  file_ops->close = null_close;

  node_ops->open = null_open;
  node_ops->stat = null_stat;

  devfs_device_t* dev = devfs_device_new(file_ops, node_ops);
  if (dev == NULL) {
    free(file_ops);
    free(node_ops);
    return NULL;
  }

  return dev;
}

static vfs_status_t null_open(vfs_node_t* vnode,
                              uint32_t flags,
                              vfs_file_t** out) {
  (void)flags;
  if (vnode == NULL || out == NULL || *out == NULL) {
    return VFS_STATUS_INVALID_ARG;
  }

  (*out)->fs_data = NULL;
  return VFS_STATUS_OK;
}

static vfs_status_t null_close(vfs_file_t* file) {
  if (file == NULL) { return VFS_STATUS_INVALID_ARG; }

  file->fs_data = NULL;
  return VFS_STATUS_OK;
}

static vfs_status_t null_read(vfs_file_t* file,
                              void* buffer,
                              uint64_t len,
                              uint64_t* bytes_read_out) {
  SET_OUT(bytes_read_out, 0);
  return VFS_STATUS_OK;
}

static vfs_status_t null_write(vfs_file_t* file,
                               void* buffer,
                               uint64_t len,
                               uint64_t* bytes_written_out) {
  SET_OUT(bytes_written_out, len);
  return VFS_STATUS_OK;
}

static vfs_status_t null_stat(vfs_node_t* vnode, vfs_stat_t** out) {
  if (vnode == NULL || out == NULL) { return VFS_STATUS_INVALID_ARG; }

  vfs_stat_t* stat = calloc(1, sizeof(vfs_stat_t));
  if (stat == NULL) { return VFS_STATUS_NOMEM; }

  stat->type = vnode->type;
  stat->size = 0;
  *out = stat;
  return VFS_STATUS_OK;
}
