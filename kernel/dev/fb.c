#include <dev/fb.h>
#include <drivers/vga.h>
#include <fs/vfs.h>
#include <kernel/g_kernel.h>
#include <utils/set_out.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static vfs_status_t fb_open(vfs_node_t* vnode,
                            uint32_t flags,
                            vfs_file_t** out);
static vfs_status_t fb_close(vfs_file_t* file);
static vfs_status_t fb_read(vfs_file_t* file,
                            void* buffer,
                            uint64_t len,
                            uint64_t* bytes_read_out);
static vfs_status_t fb_write(vfs_file_t* file,
                             void* buffer,
                             uint64_t len,
                             uint64_t* bytes_written_out);
static vfs_status_t fb_seek(vfs_file_t* file,
                            int64_t offset,
                            vfs_seek_whence_t whence,
                            uint64_t* new_pos);
static vfs_status_t fb_stat(vfs_node_t* vnode, vfs_stat_t** out);

static uint8_t* fb_base(void);
static uint64_t fb_size(void);
static uint64_t seek_offset(uint64_t origin, int64_t offset, uint64_t limit);

devfs_device_t* fb_dev_new(void) {
  vfs_file_ops_t* file_ops = calloc(1, sizeof(vfs_file_ops_t));
  vfs_node_ops_t* node_ops = calloc(1, sizeof(vfs_node_ops_t));
  if (file_ops == NULL || node_ops == NULL) {
    free(file_ops);
    free(node_ops);
    return NULL;
  }

  file_ops->read = fb_read;
  file_ops->write = fb_write;
  file_ops->seek = fb_seek;
  file_ops->close = fb_close;

  node_ops->open = fb_open;
  node_ops->stat = fb_stat;

  devfs_device_t* dev = devfs_device_new(file_ops, node_ops);
  if (dev == NULL) {
    free(file_ops);
    free(node_ops);
    return NULL;
  }

  return dev;
}

static vfs_status_t fb_open(vfs_node_t* vnode,
                            uint32_t flags,
                            vfs_file_t** out) {
  (void)flags;
  if (vnode == NULL || out == NULL || *out == NULL) {
    return VFS_STATUS_INVALID_ARG;
  }

  (*out)->fs_data = NULL;
  return VFS_STATUS_OK;
}

static vfs_status_t fb_close(vfs_file_t* file) {
  if (file == NULL) { return VFS_STATUS_INVALID_ARG; }

  file->fs_data = NULL;
  return VFS_STATUS_OK;
}

static vfs_status_t fb_read(vfs_file_t* file,
                            void* buffer,
                            uint64_t len,
                            uint64_t* bytes_read_out) {
  SET_OUT(bytes_read_out, 0);
  if (file == NULL || (buffer == NULL && len > 0)) {
    return VFS_STATUS_INVALID_ARG;
  }

  uint64_t size = fb_size();
  if (file->offset >= size || len == 0) { return VFS_STATUS_OK; }

  uint8_t* base = fb_base();
  if (base == NULL) { return VFS_STATUS_INVALID_ARG; }

  uint64_t available = size - file->offset;
  uint64_t bytes_to_read = len < available ? len : available;
  memcpy(buffer, base + file->offset, bytes_to_read);
  file->offset += bytes_to_read;
  SET_OUT(bytes_read_out, bytes_to_read);
  return VFS_STATUS_OK;
}

static vfs_status_t fb_write(vfs_file_t* file,
                             void* buffer,
                             uint64_t len,
                             uint64_t* bytes_written_out) {
  SET_OUT(bytes_written_out, 0);
  if (file == NULL || (buffer == NULL && len > 0)) {
    return VFS_STATUS_INVALID_ARG;
  }

  uint64_t size = fb_size();
  if (file->offset >= size || len == 0) { return VFS_STATUS_OK; }

  uint8_t* base = fb_base();
  if (base == NULL) { return VFS_STATUS_INVALID_ARG; }

  uint64_t available = size - file->offset;
  uint64_t bytes_to_write = len < available ? len : available;
  memcpy(base + file->offset, buffer, bytes_to_write);
  file->offset += bytes_to_write;
  SET_OUT(bytes_written_out, bytes_to_write);
  return VFS_STATUS_OK;
}

static vfs_status_t fb_seek(vfs_file_t* file,
                            int64_t offset,
                            vfs_seek_whence_t whence,
                            uint64_t* new_pos) {
  if (file == NULL) { return VFS_STATUS_INVALID_ARG; }

  uint64_t origin = 0;
  uint64_t size = fb_size();
  switch (whence) {
    case VFS_SEEK_SET:
      origin = 0;
      break;
    case VFS_SEEK_CUR:
      origin = file->offset;
      break;
    case VFS_SEEK_END:
      origin = size;
      break;
    default:
      return VFS_STATUS_INVALID_ARG;
  }

  uint64_t target = seek_offset(origin, offset, size);

  file->offset = target;
  SET_OUT(new_pos, target);
  return VFS_STATUS_OK;
}

static vfs_status_t fb_stat(vfs_node_t* vnode, vfs_stat_t** out) {
  if (vnode == NULL || out == NULL) { return VFS_STATUS_INVALID_ARG; }

  vfs_stat_t* stat = calloc(1, sizeof(vfs_stat_t));
  if (stat == NULL) { return VFS_STATUS_NOMEM; }

  stat->type = vnode->type;
  stat->size = fb_size();
  stat->link_count = vnode->link_count;
  *out = stat;
  return VFS_STATUS_OK;
}

static uint8_t* fb_base(void) {
  if (g_kernel.vga == NULL) { return NULL; }
  return (uint8_t*)vga_state_get_framebuffer_addr(g_kernel.vga);
}

static uint64_t fb_size(void) {
  if (g_kernel.vga == NULL) { return 0; }
  return vga_state_get_pitch(g_kernel.vga) *
         vga_state_get_height(g_kernel.vga);
}

static uint64_t seek_offset(uint64_t origin, int64_t offset, uint64_t limit) {
  if (offset < 0) {
    uint64_t delta = (uint64_t)(-(offset + 1)) + 1;
    return delta > origin ? 0 : origin - delta;
  }

  if ((uint64_t)offset > UINT64_MAX - origin) { return limit; }
  uint64_t target = origin + (uint64_t)offset;
  return target > limit ? limit : target;
}
