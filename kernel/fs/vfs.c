#include <fs/vfs.h>
#include <kernel/g_kernel.h>
#include <multitask/scheduler.h>
#include <stdbool.h>
#include <stddef.h>

#define SET_OUT(out, val)                                                      \
  if (out != NULL) { *out = val; }

static vfs_mount_t* root_mount = NULL;

static void claim_vnode(vfs_node_t* vnode);
static void release_vnode(vfs_node_t* vnode);
static void clear_process_fd(vfs_file_t* file);
static vfs_status_t walk_path(const char* absolute_path,
                              bool stop_at_parent,
                              vfs_node_t** out,
                              const char** name_out,
                              uint32_t* name_len_out);
static vfs_status_t remove_child(vfs_node_t* dir,
                                 const char* name,
                                 uint32_t name_len,
                                 uint32_t flags,
                                 bool expect_dir);
static vfs_status_t validate_root_mount(vfs_mount_t* mount);

vfs_status_t vfs_mount_root(vfs_mount_t* mount) {
  vfs_status_t status = validate_root_mount(mount);
  if (status != VFS_STATUS_OK) { return status; }

  root_mount = mount;
  claim_vnode(mount->root);
  return VFS_STATUS_OK;
}

vfs_status_t vfs_lookup(const char* absolute_path, vfs_node_t** out) {
  if (out == NULL) { return VFS_STATUS_INVALID_ARG; }
  return walk_path(absolute_path, false, out, NULL, NULL);
}

vfs_status_t vfs_lookup_parent(const char* absolute_path,
                               vfs_node_t** parent_out,
                               const char** name_out,
                               uint32_t* name_len_out) {
  if (parent_out == NULL || name_out == NULL || name_len_out == NULL) {
    return VFS_STATUS_INVALID_ARG;
  }

  return walk_path(absolute_path, true, parent_out, name_out, name_len_out);
}

vfs_status_t vfs_open(const char* absolute_path,
                      uint32_t flags,
                      vfs_file_t** out) {
  if (absolute_path == NULL || out == NULL) { return VFS_STATUS_INVALID_ARG; }
  *out = NULL;

  const char* path_end = absolute_path;
  while (*path_end != '\0') { path_end++; }

  if ((flags & VFS_OPEN_CREATE) && (flags & VFS_OPEN_DIRECTORY)) {
    return VFS_STATUS_INVALID_ARG;
  }
  if ((flags & VFS_OPEN_CREATE) && path_end > absolute_path + 1 &&
      path_end[-1] == '/') {
    return VFS_STATUS_INVALID_ARG;
  }

  uint64_t fd_idx;
  bool has_fd = sched_pb_fd_find_null(g_kernel.current_process, &fd_idx);
  if (!has_fd) { return VFS_STATUS_TOO_MANY_OPEN; }

  vfs_node_t* target = NULL;
  vfs_status_t lookup_res = vfs_lookup(absolute_path, &target);
  if (lookup_res != VFS_STATUS_OK &&
      !(lookup_res == VFS_STATUS_NOENT && (flags & VFS_OPEN_CREATE) &&
        !(flags & VFS_OPEN_DIRECTORY))) {
    return lookup_res;
  }

  if (lookup_res == VFS_STATUS_NOENT) {
    vfs_node_t* dir = NULL;
    const char* name = NULL;
    uint32_t name_len = 0;
    vfs_status_t dir_status =
        vfs_lookup_parent(absolute_path, &dir, &name, &name_len);
    if (dir_status != VFS_STATUS_OK) { return dir_status; }

    vfs_status_t create_status = vfs_create(dir, name, name_len, &target);
    release_vnode(dir);
    if (create_status != VFS_STATUS_OK) { return create_status; }
  }

  uint32_t open_flags = flags & ~VFS_OPEN_CREATE;
  vfs_status_t open_status = target->ops->open(target, open_flags, out);
  if (open_status == VFS_STATUS_OK) {
    sched_pb_fd_set(g_kernel.current_process, fd_idx, *out);
  } else {
    release_vnode(target);
  }
  return open_status;
}

vfs_status_t vfs_create(vfs_node_t* dir,
                        const char* name,
                        uint32_t name_len,
                        vfs_node_t** out) {
  if (dir == NULL || name == NULL) { return VFS_STATUS_INVALID_ARG; }
  if (dir->ops == NULL || dir->ops->create_file == NULL) {
    SET_OUT(out, NULL);
    return VFS_STATUS_NOT_IMPLEMENTED;
  }

  vfs_node_t* created = NULL;
  vfs_status_t status = dir->ops->create_file(dir, name, name_len, &created);
  if (status != VFS_STATUS_OK) {
    SET_OUT(out, NULL);
    return status;
  }

  if (out == NULL) {
    release_vnode(created);
  } else {
    *out = created;
  }
  return VFS_STATUS_OK;
}

vfs_status_t vfs_mkdir(vfs_node_t* dir,
                       const char* name,
                       uint32_t name_len,
                       vfs_node_t** out) {
  if (dir == NULL || name == NULL) {
    SET_OUT(out, NULL);
    return VFS_STATUS_INVALID_ARG;
  }

  while (name_len > 0 && name[name_len - 1] == '/') { --name_len; }
  if (name_len == 0) {
    SET_OUT(out, NULL);
    return VFS_STATUS_INVALID_ARG;
  }
  if (dir->ops == NULL || dir->ops->create_dir == NULL) {
    SET_OUT(out, NULL);
    return VFS_STATUS_NOT_IMPLEMENTED;
  }

  vfs_node_t* created = NULL;
  vfs_status_t status = dir->ops->create_dir(dir, name, name_len, &created);
  if (status != VFS_STATUS_OK) {
    SET_OUT(out, NULL);
    return status;
  }

  if (out == NULL) {
    release_vnode(created);
  } else {
    *out = created;
  }
  return VFS_STATUS_OK;
}

vfs_status_t vfs_unlink(vfs_node_t* dir,
                        const char* name,
                        uint32_t name_len,
                        uint32_t flags) {
  if (dir == NULL || name == NULL || name_len == 0) {
    return VFS_STATUS_INVALID_ARG;
  }

  return remove_child(dir, name, name_len, flags, false);
}

vfs_status_t vfs_rmdir(vfs_node_t* dir,
                       const char* name,
                       uint32_t name_len,
                       uint32_t flags) {
  if (dir == NULL || name == NULL || name_len == 0) {
    return VFS_STATUS_INVALID_ARG;
  }

  return remove_child(dir, name, name_len, flags, true);
}

vfs_status_t vfs_read(vfs_file_t* file,
                      void* buffer,
                      uint64_t len,
                      uint64_t* out_read) {
  if (file == NULL) { return VFS_STATUS_INVALID_ARG; }
  if (!(file->flags & VFS_OPEN_READ)) { return VFS_STATUS_FLAGS; }
  return file->ops->read(file, buffer, len, out_read);
}

vfs_status_t vfs_write(vfs_file_t* file,
                       void* buffer,
                       uint64_t len,
                       uint64_t* bytes_written_out) {
  if (file == NULL) {
    SET_OUT(bytes_written_out, 0);
    return VFS_STATUS_INVALID_ARG;
  }
  if (!(file->flags & VFS_OPEN_WRITE)) { return VFS_STATUS_FLAGS; }
  if (buffer == NULL) {
    SET_OUT(bytes_written_out, 0);
    return VFS_STATUS_INVALID_ARG;
  }

  return file->ops->write(file, buffer, len, bytes_written_out);
}

vfs_status_t vfs_readdir(vfs_file_t* dir, vfs_dirent_t** out) {
  if (dir == NULL) {
    SET_OUT(out, NULL);
    return VFS_STATUS_NOENT;
  }
  if (dir->vnode->type != VFS_NODE_DIR) {
    SET_OUT(out, NULL);
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
    SET_OUT(out, NULL);
    return status;
  }

  status = vnode->ops->stat(vnode, out);
  release_vnode(vnode);
  return status;
}

vfs_status_t vfs_fstat(vfs_file_t* file, vfs_stat_t** out) {
  if (file == NULL) {
    SET_OUT(out, NULL);
    return VFS_STATUS_NOENT;
  }
  return file->vnode->ops->stat(file->vnode, out);
}

vfs_status_t vfs_close(vfs_file_t* file) {
  if (file == NULL) { return VFS_STATUS_OK; }

  vfs_node_t* vnode = file->vnode;
  clear_process_fd(file);
  vfs_status_t status = file->ops->close(file);
  release_vnode(vnode);
  return status;
}

vfs_status_t vfs_resolve_fd(uint64_t fd, vfs_file_t** out) {
  vfs_file_t* ret = sched_pb_fd_get(g_kernel.current_process, fd);
  if (ret == NULL) { return VFS_STATUS_BAD_FD; }
  SET_OUT(out, ret);
  return VFS_STATUS_OK;
}

static void claim_vnode(vfs_node_t* vnode) {
  if (vnode != NULL) { vnode->refcount++; }
}

static void release_vnode(vfs_node_t* vnode) {
  if (vnode == NULL || vnode->refcount == 0) { return; }

  vnode->refcount--;
  if (vnode->refcount == 0 && vnode->link_count == 0 && vnode->ops != NULL &&
      vnode->ops->free != NULL) {
    vnode->ops->free(vnode);
  }
}

static void clear_process_fd(vfs_file_t* file) {
  if (g_kernel.current_process == NULL) { return; }

  for (uint64_t i = 0; i < MAX_FDS; ++i) {
    if (sched_pb_fd_get(g_kernel.current_process, i) == file) {
      sched_pb_fd_set(g_kernel.current_process, i, NULL);
      return;
    }
  }
}

static vfs_status_t walk_path(const char* absolute_path,
                              bool stop_at_parent,
                              vfs_node_t** out,
                              const char** name_out,
                              uint32_t* name_len_out) {
  if (absolute_path == NULL || out == NULL || root_mount == NULL ||
      root_mount->root == NULL || absolute_path[0] != '/') {
    return VFS_STATUS_NOENT;
  }

  SET_OUT(name_out, NULL);
  SET_OUT(name_len_out, 0);

  vfs_node_t* current = root_mount->root;
  bool current_is_root = true;
  const char* cursor = absolute_path;

  while (*cursor == '/') { cursor++; }
  if (*cursor == '\0') {
    claim_vnode(current);
    *out = current;
    return VFS_STATUS_OK;
  }

  while (*cursor != '\0') {
    if (current->type != VFS_NODE_DIR || current->ops == NULL ||
        current->ops->lookup == NULL) {
      if (!current_is_root) { release_vnode(current); }
      return VFS_STATUS_NOTDIR;
    }

    const char* component = cursor;
    uint32_t component_len = 0;
    while (*cursor != '\0' && *cursor != '/') {
      cursor++;
      component_len++;
    }

    while (*cursor == '/') { cursor++; }
    if (stop_at_parent && *cursor == '\0') {
      if (current_is_root) { claim_vnode(current); }
      *out = current;
      SET_OUT(name_out, component);
      SET_OUT(name_len_out, component_len);
      return VFS_STATUS_OK;
    }

    vfs_node_t* next = NULL;
    vfs_status_t status =
        current->ops->lookup(current, component, component_len, &next);
    if (status != VFS_STATUS_OK) {
      if (!current_is_root) { release_vnode(current); }
      return status;
    }

    if (!current_is_root) { release_vnode(current); }
    current = next;
    current_is_root = false;
  }

  if (current_is_root) { claim_vnode(current); }
  *out = current;
  return VFS_STATUS_OK;
}

static vfs_status_t remove_child(vfs_node_t* dir,
                                 const char* name,
                                 uint32_t name_len,
                                 uint32_t flags,
                                 bool expect_dir) {
  if (dir->type != VFS_NODE_DIR || dir->ops == NULL ||
      dir->ops->lookup == NULL) {
    return VFS_STATUS_NOTDIR;
  }
  if ((expect_dir && dir->ops->rmdir == NULL) ||
      (!expect_dir && dir->ops->unlink == NULL)) {
    return VFS_STATUS_NOT_IMPLEMENTED;
  }

  vfs_node_t* target = NULL;
  vfs_status_t status = dir->ops->lookup(dir, name, name_len, &target);
  if (status != VFS_STATUS_OK) { return status; }

  if (expect_dir && target->type != VFS_NODE_DIR) {
    release_vnode(target);
    return VFS_STATUS_NOTDIR;
  }
  if (!expect_dir && target->type == VFS_NODE_DIR) {
    release_vnode(target);
    return VFS_STATUS_ISDIR;
  }

  if (expect_dir) {
    status = dir->ops->rmdir(dir, name, name_len, flags);
  } else {
    status = dir->ops->unlink(dir, name, name_len, flags);
  }
  if (status != VFS_STATUS_OK) {
    release_vnode(target);
    return status;
  }

  if (target->link_count > 0) { target->link_count--; }
  release_vnode(target);
  return VFS_STATUS_OK;
}

static vfs_status_t validate_root_mount(vfs_mount_t* mount) {
  if (mount == NULL || mount->root == NULL) { return VFS_STATUS_NOMEM; }
  if (mount->root->type != VFS_NODE_DIR) { return VFS_STATUS_NOTDIR; }
  if (mount->root->ops == NULL || mount->root->ops->lookup == NULL) {
    return VFS_STATUS_NOTDIR;
  }

  return VFS_STATUS_OK;
}
