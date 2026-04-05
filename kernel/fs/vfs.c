#include <fs/vfs.h>
#include <kernel/g_kernel.h>
#include <multitask/scheduler.h>
#include <stdbool.h>
#include <stddef.h>

#define SET_OUT(out, val)                                                      \
  if (out != NULL) { *out = val; }

static vfs_mount_t* root_mount = NULL;

static vfs_status_t validate_root_mount(vfs_mount_t* mount) {
  if (mount == NULL || mount->root == NULL) { return VFS_STATUS_NOMEM; }
  if (mount->root->type != VFS_NODE_DIR) { return VFS_STATUS_NOTDIR; }
  if (mount->root->ops == NULL || mount->root->ops->lookup == NULL) {
    return VFS_STATUS_NOTDIR;
  }

  return VFS_STATUS_OK;
}

vfs_status_t vfs_mount_root(vfs_mount_t* mount) {
  vfs_status_t status = validate_root_mount(mount);
  if (status != VFS_STATUS_OK) { return status; }

  root_mount = mount;
  return VFS_STATUS_OK;
}

vfs_status_t vfs_lookup(const char* absolute_path, vfs_node_t** out) {
  if (absolute_path == NULL || out == NULL || root_mount == NULL ||
      root_mount->root == NULL || absolute_path[0] != '/') {
    return VFS_STATUS_NOENT;
  }

  vfs_node_t* current = root_mount->root;
  bool current_is_root = true;
  const char* cursor = absolute_path;

  while (*cursor == '/') { cursor++; }
  if (*cursor == '\0') {
    *out = current;
    return VFS_STATUS_OK;
  }

  while (*cursor != '\0') {
    if (current->type != VFS_NODE_DIR || current->ops == NULL ||
        current->ops->lookup == NULL) {
      if (!current_is_root && current->ops != NULL &&
          current->ops->release != NULL) {
        current->ops->release(current);
      }
      return VFS_STATUS_NOTDIR;
    }

    const char* component = cursor;
    uint32_t component_len = 0;

    while (*cursor != '\0' && *cursor != '/') {
      cursor++;
      component_len++;
    }

    vfs_node_t* next = NULL;
    vfs_status_t status =
        current->ops->lookup(current, component, component_len, &next);
    if (status != VFS_STATUS_OK) {
      if (!current_is_root && current->ops != NULL &&
          current->ops->release != NULL) {
        current->ops->release(current);
      }
      return status;
    }

    if (!current_is_root && current->ops != NULL &&
        current->ops->release != NULL) {
      current->ops->release(current);
    }
    current = next;
    current_is_root = false;

    while (*cursor == '/') { cursor++; }
  }

  *out = current;
  return VFS_STATUS_OK;
}

vfs_status_t vfs_open(const char* absolute_path,
                      uint32_t flags,
                      vfs_file_t** out) {
  uint64_t fd_idx;
  bool has_fd = sched_pb_fd_find_null(g_kernel.current_process, &fd_idx);
  if (!has_fd) {
    if (out != NULL) { *out = NULL; }
    return VFS_STATUS_TOO_MANY_OPEN;
  }

  vfs_node_t* target = NULL;
  vfs_status_t lookup_res = vfs_lookup(absolute_path, &target);
  if (lookup_res != VFS_STATUS_OK) {
    if (out != NULL) { *out = NULL; }
    return lookup_res;
  }

  vfs_status_t open_status = target->ops->open(target, flags, out);
  if (open_status == VFS_STATUS_OK) {
    sched_pb_fd_set(g_kernel.current_process, fd_idx, *out);
  }
  return open_status;
}

vfs_status_t vfs_create(vfs_node_t* dir,
                        const char* name,
                        uint32_t name_len,
                        vfs_node_t** out) {
  return VFS_STATUS_NOT_IMPLEMENTED;
}

vfs_status_t vfs_mkdir(vfs_node_t* dir,
                       const char* name,
                       uint32_t name_len,
                       vfs_node_t** out) {
  return VFS_STATUS_NOT_IMPLEMENTED;
}

vfs_status_t vfs_unlink(vfs_node_t* dir, const char* name, uint32_t name_len) {
  return VFS_STATUS_NOT_IMPLEMENTED;
}

vfs_status_t vfs_rmdir(vfs_node_t* dir, const char* name, uint32_t name_len) {
  return VFS_STATUS_NOT_IMPLEMENTED;
}

vfs_status_t vfs_read(vfs_file_t* file,
                      void* buffer,
                      uint64_t len,
                      uint64_t* out_read) {
  if (!(file->flags & VFS_OPEN_READ)) { return VFS_STATUS_FLAGS; }
  return file->ops->read(file, buffer, len, out_read);
}

vfs_status_t vfs_write(vfs_file_t* file,
                       void* buffer,
                       uint64_t len,
                       uint64_t* bytes_written_out) {
  if (!(file->flags & VFS_OPEN_WRITE)) { return VFS_STATUS_FLAGS; }
  if (file == NULL || buffer == NULL) {
    SET_OUT(bytes_written_out, 0);
    return VFS_STATUS_INVALID_ARG;
  }

  return file->ops->write(file, buffer, len, bytes_written_out);
}

vfs_status_t vfs_readdir(vfs_file_t* dir, vfs_dirent_t** out) {
  if (dir == NULL) {
    if (out != NULL) { *out = NULL; }
    return VFS_STATUS_NOENT;
  }
  if (dir->vnode->type != VFS_NODE_DIR) {
    if (out != NULL) { *out = NULL; }
    return VFS_STATUS_NOTDIR;
  }

  return dir->ops->readdir(dir, out);
}

vfs_status_t vfs_seek(vfs_file_t* file,
                      uint64_t offset,
                      vfs_seek_whence_t whence,
                      uint64_t* new_pos) {
  if (file == NULL) { return VFS_STATUS_INVALID_ARG; }

  return file->ops->seek(file, offset, whence, new_pos);
}

vfs_status_t vfs_stat(const char* absolute_path, vfs_stat_t** out) {
  vfs_node_t* vnode = NULL;
  vfs_status_t status = vfs_lookup(absolute_path, &vnode);
  if (status != VFS_STATUS_OK) {
    if (out != NULL) { *out = NULL; }
    return status;
  }
  return vnode->ops->stat(vnode, out);
}

vfs_status_t vfs_fstat(vfs_file_t* file, vfs_stat_t** out) {
  if (file == NULL) {
    if (out != NULL) { *out = NULL; }
    return VFS_STATUS_NOENT;
  }
  return file->vnode->ops->stat(file->vnode, out);
}

vfs_status_t vfs_close(vfs_file_t* file) {
  if (file != NULL) { file->ops->close(file); }
  return VFS_STATUS_OK;
}

vfs_status_t vfs_resolve_fd(uint64_t fd, vfs_file_t** out) {
  vfs_file_t* ret = sched_pb_fd_get(g_kernel.current_process, fd);
  if (ret == NULL) { return VFS_STATUS_BAD_FD; }
  SET_OUT(out, ret);
  return VFS_STATUS_OK;
}

