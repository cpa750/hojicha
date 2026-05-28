#include <fs/vfs.h>
#include <kernel/g_kernel.h>
#include <multitask/scheduler.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define SET_OUT(out, val)                                                      \
  if (out != NULL) { *out = val; }
#define HVFS_VOP_MISSING(vnode, op)                                            \
  ((vnode) == NULL || (vnode)->ops == NULL || (vnode)->ops->op == NULL)
#define HVFS_FOP_MISSING(file, op)                                             \
  ((file) == NULL || (file)->ops == NULL || (file)->ops->op == NULL)

static vfs_mount_t* root_mount = NULL;

static void clear_process_fd(vfs_file_t* file);
static vfs_node_t* traverse_mount(vfs_node_t* vnode);
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
  if (root_mount != NULL) { return VFS_STATUS_INVALID_ARG; }

  mount->point = NULL;
  mount->parent = NULL;
  root_mount = mount;
  vfs_vnode_borrow(mount->root);
  return VFS_STATUS_OK;
}

vfs_status_t vfs_mount(vfs_node_t* mountpoint,
                       vfs_mount_t* mount,
                       vfs_mount_t* parent) {
  vfs_status_t status = validate_root_mount(mount);
  if (status != VFS_STATUS_OK) { return status; }
  if (root_mount == NULL || mountpoint == NULL) {
    return VFS_STATUS_INVALID_ARG;
  }
  if (mountpoint->type != VFS_NODE_DIR) { return VFS_STATUS_NOTDIR; }
  if (mountpoint->mount != NULL) { return VFS_STATUS_INVALID_ARG; }

  mount->point = mountpoint;
  mount->parent = parent;
  mountpoint->mount = mount;

  vfs_vnode_borrow(mountpoint);
  vfs_vnode_borrow(mount->root);
  return VFS_STATUS_OK;
}

vfs_status_t vfs_unmount(vfs_mount_t* mount) {
  if (mount == NULL) { return VFS_STATUS_INVALID_ARG; }
  if (mount == root_mount) { return VFS_STATUS_INVALID_ARG; }
  if (mount->point == NULL || mount->root == NULL) {
    return VFS_STATUS_INVALID_ARG;
  }
  if (mount->point->mount != mount) { return VFS_STATUS_INVALID_ARG; }

  mount->point->mount = NULL;
  vfs_vnode_release(mount->root);
  vfs_vnode_release(mount->point);
  mount->point = NULL;
  mount->parent = NULL;
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
    vfs_vnode_release(dir);
    if (create_status != VFS_STATUS_OK) { return create_status; }
  }

  if (HVFS_VOP_MISSING(target, open)) {
    vfs_vnode_release(target);
    return VFS_STATUS_NOT_IMPLEMENTED;
  }

  uint32_t open_flags = flags & ~VFS_OPEN_CREATE;
  vfs_status_t open_status = target->ops->open(target, open_flags, out);
  if (open_status == VFS_STATUS_OK) {
    sched_pb_fd_set(g_kernel.current_process, fd_idx, *out);
  } else {
    vfs_vnode_release(target);
  }
  return open_status;
}

vfs_status_t vfs_create(vfs_node_t* dir,
                        const char* name,
                        uint32_t name_len,
                        vfs_node_t** out) {
  if (dir == NULL || name == NULL) { return VFS_STATUS_INVALID_ARG; }
  if (HVFS_VOP_MISSING(dir, create_file)) {
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
    vfs_vnode_release(created);
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
  if (HVFS_VOP_MISSING(dir, create_dir)) {
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
    vfs_vnode_release(created);
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
  if (HVFS_FOP_MISSING(file, read)) { return VFS_STATUS_NOT_IMPLEMENTED; }
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
  if (HVFS_FOP_MISSING(file, write)) {
    SET_OUT(bytes_written_out, 0);
    return VFS_STATUS_NOT_IMPLEMENTED;
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
  if (HVFS_FOP_MISSING(dir, readdir)) {
    SET_OUT(out, NULL);
    return VFS_STATUS_NOT_IMPLEMENTED;
  }

  return dir->ops->readdir(dir, out);
}

vfs_status_t vfs_seek(vfs_file_t* file,
                      uint64_t offset,
                      vfs_seek_whence_t whence,
                      uint64_t* new_pos) {
  if (file == NULL) { return VFS_STATUS_INVALID_ARG; }
  if (HVFS_FOP_MISSING(file, seek)) { return VFS_STATUS_NOT_IMPLEMENTED; }

  return file->ops->seek(file, offset, whence, new_pos);
}

vfs_status_t vfs_stat(const char* absolute_path, vfs_stat_t** out) {
  vfs_node_t* vnode = NULL;
  vfs_status_t status = vfs_lookup(absolute_path, &vnode);
  if (status != VFS_STATUS_OK) {
    SET_OUT(out, NULL);
    return status;
  }
  if (HVFS_VOP_MISSING(vnode, stat)) {
    vfs_vnode_release(vnode);
    SET_OUT(out, NULL);
    return VFS_STATUS_NOT_IMPLEMENTED;
  }

  status = vnode->ops->stat(vnode, out);
  vfs_vnode_release(vnode);
  return status;
}

vfs_status_t vfs_fstat(vfs_file_t* file, vfs_stat_t** out) {
  if (file == NULL) {
    SET_OUT(out, NULL);
    return VFS_STATUS_NOENT;
  }
  if (HVFS_VOP_MISSING(file->vnode, stat)) {
    SET_OUT(out, NULL);
    return VFS_STATUS_NOT_IMPLEMENTED;
  }
  return file->vnode->ops->stat(file->vnode, out);
}

vfs_status_t vfs_close(vfs_file_t* file) {
  if (file == NULL) { return VFS_STATUS_OK; }
  if (HVFS_FOP_MISSING(file, close)) { return VFS_STATUS_NOT_IMPLEMENTED; }

  vfs_node_t* vnode = file->vnode;
  clear_process_fd(file);
  vfs_status_t status = file->ops->close(file);
  vfs_vnode_release(vnode);
  return status;
}

vfs_status_t vfs_resolve_fd(uint64_t fd, vfs_file_t** out) {
  vfs_file_t* ret = sched_pb_fd_get(g_kernel.current_process, fd);
  if (ret == NULL) { return VFS_STATUS_BAD_FD; }
  SET_OUT(out, ret);
  return VFS_STATUS_OK;
}

void vfs_vnode_borrow(vfs_node_t* vnode) {
  if (vnode != NULL) { vnode->refcount++; }
}

void vfs_vnode_release(vfs_node_t* vnode) {
  if (vnode == NULL || vnode->refcount == 0) { return; }

  vnode->refcount--;
  if (vnode->refcount == 0 && vnode->link_count == 0 &&
      !HVFS_VOP_MISSING(vnode, free)) {
    vnode->ops->free(vnode);
  }
}

bool vfs_validate_name(const char* name, uint64_t name_len) {
  if (name == NULL || name_len == 0) { return false; }

  for (uint64_t i = 0; i < name_len; ++i) {
    if (name[i] == '/' || (name[i] == '\0' && i < name_len - 1)) {
      return false;
    }
  }

  return true;
}

char* vfs_clone_name(const char* name, uint64_t name_len, bool trailing_slash) {
  uint64_t alloc_len = name_len + (trailing_slash ? 1 : 0);
  char* cloned = (char*)malloc(alloc_len + 1);
  if (cloned == NULL) { return NULL; }

  memcpy(cloned, name, name_len);
  if (trailing_slash) { cloned[name_len++] = '/'; }
  cloned[name_len] = '\0';
  return cloned;
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

static vfs_node_t* traverse_mount(vfs_node_t* vnode) {
  if (vnode == NULL) { return NULL; }

  vfs_mount_t* mount = vnode->mount;
  if (mount == NULL || mount->root == NULL) { return vnode; }

  vfs_vnode_borrow(mount->root);
  vfs_vnode_release(vnode);
  return mount->root;
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
    vfs_vnode_borrow(current);
    *out = current;
    return VFS_STATUS_OK;
  }

  while (*cursor != '\0') {
    if (current->type != VFS_NODE_DIR || HVFS_VOP_MISSING(current, lookup)) {
      if (!current_is_root) { vfs_vnode_release(current); }
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
      if (current_is_root) { vfs_vnode_borrow(current); }
      *out = current;
      SET_OUT(name_out, component);
      SET_OUT(name_len_out, component_len);
      return VFS_STATUS_OK;
    }

    vfs_node_t* next = NULL;
    vfs_status_t status =
        current->ops->lookup(current, component, component_len, &next);
    if (status != VFS_STATUS_OK) {
      if (!current_is_root) { vfs_vnode_release(current); }
      return status;
    }
    next = traverse_mount(next);

    if (!current_is_root) { vfs_vnode_release(current); }
    current = next;
    current_is_root = false;
  }

  if (current_is_root) { vfs_vnode_borrow(current); }
  *out = current;
  return VFS_STATUS_OK;
}

static vfs_status_t remove_child(vfs_node_t* dir,
                                 const char* name,
                                 uint32_t name_len,
                                 uint32_t flags,
                                 bool expect_dir) {
  if (dir->type != VFS_NODE_DIR || HVFS_VOP_MISSING(dir, lookup)) {
    return VFS_STATUS_NOTDIR;
  }
  if ((expect_dir && HVFS_VOP_MISSING(dir, rmdir)) ||
      (!expect_dir && HVFS_VOP_MISSING(dir, unlink))) {
    return VFS_STATUS_NOT_IMPLEMENTED;
  }

  vfs_node_t* target = NULL;
  vfs_status_t status = dir->ops->lookup(dir, name, name_len, &target);
  if (status != VFS_STATUS_OK) { return status; }

  if (expect_dir && target->type != VFS_NODE_DIR) {
    vfs_vnode_release(target);
    return VFS_STATUS_NOTDIR;
  }
  if (!expect_dir && target->type == VFS_NODE_DIR) {
    vfs_vnode_release(target);
    return VFS_STATUS_ISDIR;
  }

  if (expect_dir) {
    status = dir->ops->rmdir(dir, name, name_len, flags);
  } else {
    status = dir->ops->unlink(dir, name, name_len, flags);
  }
  if (status != VFS_STATUS_OK) {
    vfs_vnode_release(target);
    return status;
  }

  if (target->link_count > 0) { target->link_count--; }
  vfs_vnode_release(target);
  return VFS_STATUS_OK;
}

static vfs_status_t validate_root_mount(vfs_mount_t* mount) {
  if (mount == NULL || mount->root == NULL) { return VFS_STATUS_NOMEM; }
  if (mount->root->type != VFS_NODE_DIR) { return VFS_STATUS_NOTDIR; }
  if (HVFS_VOP_MISSING(mount->root, lookup)) { return VFS_STATUS_NOTDIR; }

  return VFS_STATUS_OK;
}
