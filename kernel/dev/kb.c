#include <dev/kb.h>
#include <drivers/keyboard.h>
#include <fs/vfs.h>
#include <memory/slab.h>
#include <stdlib.h>
#include <string.h>
#include <utils/set_out.h>

static vfs_status_t kb_open(vfs_node_t* vnode,
                            uint32_t flags,
                            vfs_file_t** out);
static vfs_status_t kb_close(vfs_file_t* file);
static vfs_status_t kb_read(vfs_file_t* file,
                            void* buffer,
                            uint64_t len,
                            uint64_t* bytes_read_out);
static vfs_status_t kb_stat(vfs_node_t* vnode, vfs_stat_t** out);

devfs_device_t* kb_dev_new(void) {
  vfs_file_ops_t* file_ops = slab_calloc(sizeof(vfs_file_ops_t));
  vfs_node_ops_t* node_ops = slab_calloc(sizeof(vfs_node_ops_t));
  if (file_ops == NULL || node_ops == NULL) {
    slab_free(file_ops);
    slab_free(node_ops);
    return NULL;
  }

  file_ops->read = kb_read;
  file_ops->close = kb_close;
  node_ops->open = kb_open;
  node_ops->stat = kb_stat;

  devfs_device_t* dev = devfs_device_new(file_ops, node_ops);
  if (dev == NULL) {
    slab_free(file_ops);
    slab_free(node_ops);
    return NULL;
  }

  return dev;
}

static vfs_status_t kb_open(vfs_node_t* vnode,
                            uint32_t flags,
                            vfs_file_t** out) {
  (void)flags;
  if (vnode == NULL || out == NULL || *out == NULL) {
    return VFS_STATUS_INVALID_ARG;
  }

  (*out)->fs_data = NULL;
  return VFS_STATUS_OK;
}

static vfs_status_t kb_close(vfs_file_t* file) {
  if (file == NULL) { return VFS_STATUS_INVALID_ARG; }

  file->fs_data = NULL;
  return VFS_STATUS_OK;
}

static vfs_status_t kb_read(vfs_file_t* file,
                            void* buffer,
                            uint64_t len,
                            uint64_t* bytes_read_out) {
  (void)file;
  SET_OUT(bytes_read_out, 0);
  if (buffer == NULL) { return VFS_STATUS_INVALID_ARG; }
  if (len < sizeof(keyboard_event_t)) { return VFS_STATUS_RANGE; }

  keyboard_event_t event;
  if (!keyboard_read_event_wait(&event)) {
    return VFS_STATUS_NOT_IMPLEMENTED;
  }

  memcpy(buffer, &event, sizeof(event));
  SET_OUT(bytes_read_out, sizeof(event));
  return VFS_STATUS_OK;
}

static vfs_status_t kb_stat(vfs_node_t* vnode, vfs_stat_t** out) {
  if (vnode == NULL || out == NULL) { return VFS_STATUS_INVALID_ARG; }

  vfs_stat_t* stat = calloc(1, sizeof(vfs_stat_t));
  if (stat == NULL) { return VFS_STATUS_NOMEM; }

  stat->type = vnode->type;
  stat->size = sizeof(keyboard_event_t);
  *out = stat;
  return VFS_STATUS_OK;
}
