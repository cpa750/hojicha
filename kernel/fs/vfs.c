#include <errno.h>
#include <fs/vfs.h>
#include <kernel/g_kernel.h>
#include <kernel/ktime.h>
#include <multitask/scheduler.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <utils/set_out.h>

#define HVFS_VOP_MISSING(vnode, op)                                            \
  ((vnode) == NULL || (vnode)->ops == NULL || (vnode)->ops->op == NULL)
#define HVFS_FOP_MISSING(file, op)                                             \
  ((file) == NULL || (file)->ops == NULL || (file)->ops->op == NULL)

#define VFS_SYMLINK_MAX_DEPTH  40
#define VFS_SYMLINK_TARGET_MAX 1024

static vfs_mount_t* root_mount = NULL;

typedef struct {
  vfs_node_t* base;
  const char* path;
  bool stop_at_parent;
  uint32_t symlink_depth;
} vfs_lookup_request_t;

typedef struct {
  vfs_node_t* vnode;
  const char* final_name;
  uint32_t final_name_len;
} vfs_lookup_result_t;

static void clear_process_fd(vfs_file_t* file);
static void cache_vnode_stat_timestamps(vfs_node_t* vnode, vfs_stat_t** out);
static vfs_status_t clone_symlink_target(vfs_node_t* vnode, char** out);
static vfs_status_t can_ioctl(vfs_file_t* file);
static vfs_status_t can_open(vfs_node_t* vnode, uint32_t flags);
static vfs_status_t can_read(vfs_file_t* file);
static vfs_status_t can_readdir(vfs_file_t* file);
static vfs_status_t can_seek(vfs_file_t* file, vfs_seek_whence_t whence);
static vfs_status_t can_write(vfs_file_t* file, void* buffer);
static vfs_status_t component_parent(vfs_node_t** current);
static bool component_is_dot(const char* component, uint32_t component_len);
static bool component_is_dotdot(const char* component, uint32_t component_len);
static void cross_mount(vfs_node_t** current);
static vfs_status_t dir_mutation_allowed(vfs_node_t* dir,
                                         const char* name,
                                         uint32_t name_len);
static char* join_symlink_path(const char* target, const char* remaining);
static vfs_status_t lookup_child_no_follow(const char* path, vfs_node_t** out);
static vfs_status_t lookup_parent_entry(const char* path,
                                        vfs_node_t** parent_out,
                                        const char** name_out,
                                        uint32_t* name_len_out);
static bool open_create_path_invalid(const char* path, uint32_t flags);
static bool path_is_root(const char* path);
static vfs_status_t resolve_symlink_component(vfs_node_t* base,
                                              vfs_node_t* link,
                                              const char* remaining,
                                              uint32_t symlink_depth,
                                              vfs_node_t** out);
static vfs_status_t stat_vnode(vfs_node_t* vnode, vfs_stat_t** out);
static vfs_status_t walk_path(const vfs_lookup_request_t* request,
                              vfs_lookup_result_t* result);
static vfs_status_t vnode_owning_mount(vfs_node_t* vnode, vfs_mount_t** out);
static vfs_status_t remove_child(vfs_node_t* dir,
                                 const char* name,
                                 uint32_t name_len,
                                 uint32_t flags,
                                 bool expect_dir);
static void mark_dir_changed(vfs_node_t* dir);
static void return_created_ref(vfs_node_t* created, vfs_node_t** out);
static vfs_status_t validate_root_mount(vfs_mount_t* mount);

vfs_status_t vfs_mount_root(vfs_mount_t* mount) {
  vfs_status_t status = validate_root_mount(mount);
  if (status != VFS_STATUS_OK) { return status; }
  if (root_mount != NULL) { return VFS_STATUS_INVALID_ARG; }

  mount->point = NULL;
  mount->parent = NULL;
  root_mount = mount;
  mount->root->mount = mount;
  vfs_vnode_borrow(mount->root);
  if (g_kernel.current_process != NULL &&
      sched_pb_get_cwd(g_kernel.current_process) == NULL) {
    sched_pb_set_cwd(g_kernel.current_process, mount->root);
  }
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
  mount->root->mount = mount;

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
  mount->root->mount = NULL;
  vfs_vnode_release(mount->root);
  vfs_vnode_release(mount->point);
  mount->point = NULL;
  mount->parent = NULL;
  return VFS_STATUS_OK;
}

vfs_status_t vfs_lookup(const char* path, vfs_node_t** out) {
  if (out == NULL) { return VFS_STATUS_INVALID_ARG; }
  *out = NULL;
  return vfs_lookup_at(NULL, path, out);
}

vfs_status_t vfs_lookup_at(vfs_node_t* base,
                           const char* path,
                           vfs_node_t** out) {
  if (out == NULL) { return VFS_STATUS_INVALID_ARG; }
  *out = NULL;

  vfs_lookup_request_t request = {
      .base = base, .path = path, .stop_at_parent = false, .symlink_depth = 0};
  vfs_lookup_result_t result = {0};
  vfs_status_t status = walk_path(&request, &result);
  if (status == VFS_STATUS_OK) { *out = result.vnode; }
  return status;
}

vfs_status_t vfs_lookup_parent(const char* path,
                               vfs_node_t** parent_out,
                               const char** name_out,
                               uint32_t* name_len_out) {
  if (parent_out == NULL || name_out == NULL || name_len_out == NULL) {
    return VFS_STATUS_INVALID_ARG;
  }
  *parent_out = NULL;
  *name_out = NULL;
  *name_len_out = 0;

  return vfs_lookup_parent_at(NULL, path, parent_out, name_out, name_len_out);
}

vfs_status_t vfs_lookup_parent_at(vfs_node_t* base,
                                  const char* path,
                                  vfs_node_t** parent_out,
                                  const char** name_out,
                                  uint32_t* name_len_out) {
  if (parent_out == NULL || name_out == NULL || name_len_out == NULL) {
    return VFS_STATUS_INVALID_ARG;
  }
  *parent_out = NULL;
  *name_out = NULL;
  *name_len_out = 0;

  vfs_lookup_request_t request = {
      .base = base, .path = path, .stop_at_parent = true, .symlink_depth = 0};
  vfs_lookup_result_t result = {0};
  vfs_status_t status = walk_path(&request, &result);
  if (status == VFS_STATUS_OK) {
    *parent_out = result.vnode;
    *name_out = result.final_name;
    *name_len_out = result.final_name_len;
  }
  return status;
}

vfs_status_t vfs_open(const char* path,
                      uint32_t flags,
                      vfs_file_t** out,
                      uint64_t* out_fd) {
  if (out == NULL) { return VFS_STATUS_INVALID_ARG; }
  *out = NULL;
  SET_OUT(out_fd, 0);
  if (path == NULL) { return VFS_STATUS_INVALID_ARG; }

  if (open_create_path_invalid(path, flags)) { return VFS_STATUS_INVALID_ARG; }

  uint64_t fd_idx;
  bool has_fd = sched_pb_fd_find_null(g_kernel.current_process, &fd_idx);
  if (!has_fd) { return VFS_STATUS_TOO_MANY_OPEN; }

  vfs_status_t open_status = vfs_get_file_handle(path, flags, out);
  if (open_status == VFS_STATUS_OK) {
    sched_pb_fd_set(g_kernel.current_process, fd_idx, *out);
    SET_OUT(out_fd, fd_idx);
  }
  return open_status;
}

vfs_status_t vfs_get_file_handle(const char* path,
                                 uint32_t flags,
                                 vfs_file_t** out) {
  if (out == NULL) { return VFS_STATUS_INVALID_ARG; }
  *out = NULL;
  if (path == NULL) { return VFS_STATUS_INVALID_ARG; }

  if (open_create_path_invalid(path, flags)) { return VFS_STATUS_INVALID_ARG; }

  vfs_node_t* target = NULL;
  vfs_status_t lookup_res = vfs_lookup(path, &target);
  if (lookup_res != VFS_STATUS_OK &&
      !(lookup_res == VFS_STATUS_NOENT && (flags & VFS_OPEN_CREATE) &&
        !(flags & VFS_OPEN_DIRECTORY))) {
    return lookup_res;
  }

  vfs_node_t* dir = NULL;
  vfs_status_t status = lookup_res;
  if (lookup_res == VFS_STATUS_NOENT) {
    const char* name = NULL;
    uint32_t name_len = 0;
    status = lookup_parent_entry(path, &dir, &name, &name_len);
    if (status != VFS_STATUS_OK) { goto cleanup; }

    status = vfs_create(dir, name, name_len, &target);
    if (status != VFS_STATUS_OK) { goto cleanup; }
  }

  if (HVFS_VOP_MISSING(target, open)) {
    status = VFS_STATUS_NOT_IMPLEMENTED;
    goto cleanup;
  }
  status = can_open(target, flags);
  if (status != VFS_STATUS_OK) { goto cleanup; }

  uint32_t open_flags = flags & ~VFS_OPEN_CREATE;
  status = target->ops->open(target, open_flags, out);

cleanup:
  vfs_vnode_release(dir);
  if (status != VFS_STATUS_OK) { vfs_vnode_release(target); }
  return status;
}

vfs_status_t vfs_create(vfs_node_t* dir,
                        const char* name,
                        uint32_t name_len,
                        vfs_node_t** out) {
  SET_OUT(out, NULL);
  vfs_status_t status = dir_mutation_allowed(dir, name, name_len);
  if (status != VFS_STATUS_OK) { return status; }
  if (HVFS_VOP_MISSING(dir, create_file)) { return VFS_STATUS_NOT_IMPLEMENTED; }

  vfs_node_t* created = NULL;
  status = dir->ops->create_file(dir, name, name_len, &created);
  if (status != VFS_STATUS_OK) { return status; }

  mark_dir_changed(dir);
  return_created_ref(created, out);
  return VFS_STATUS_OK;
}

vfs_status_t vfs_mkdir(vfs_node_t* dir,
                       const char* name,
                       uint32_t name_len,
                       vfs_node_t** out) {
  SET_OUT(out, NULL);

  if (name != NULL) {
    while (name_len > 0 && name[name_len - 1] == '/') { --name_len; }
  }
  vfs_status_t status = dir_mutation_allowed(dir, name, name_len);
  if (status != VFS_STATUS_OK) { return status; }
  if (HVFS_VOP_MISSING(dir, create_dir)) { return VFS_STATUS_NOT_IMPLEMENTED; }

  vfs_node_t* created = NULL;
  status = dir->ops->create_dir(dir, name, name_len, &created);
  if (status != VFS_STATUS_OK) { return status; }

  mark_dir_changed(dir);
  return_created_ref(created, out);
  return VFS_STATUS_OK;
}

vfs_status_t vfs_unlink(vfs_node_t* dir,
                        const char* name,
                        uint32_t name_len,
                        uint32_t flags) {
  vfs_status_t status = dir_mutation_allowed(dir, name, name_len);
  if (status != VFS_STATUS_OK) { return status; }

  return remove_child(dir, name, name_len, flags, false);
}

vfs_status_t vfs_rmdir(vfs_node_t* dir,
                       const char* name,
                       uint32_t name_len,
                       uint32_t flags) {
  vfs_status_t status = dir_mutation_allowed(dir, name, name_len);
  if (status != VFS_STATUS_OK) { return status; }

  return remove_child(dir, name, name_len, flags, true);
}

vfs_status_t vfs_link(const char* old_path, const char* new_path) {
  if (old_path == NULL || new_path == NULL) { return VFS_STATUS_INVALID_ARG; }

  vfs_node_t* target = NULL;
  vfs_node_t* parent = NULL;
  const char* name = NULL;
  uint32_t name_len = 0;
  vfs_status_t status = vfs_lookup(old_path, &target);
  if (status != VFS_STATUS_OK) { return status; }
  if (target->type == VFS_NODE_DIR) {
    status = VFS_STATUS_ISDIR;
    goto cleanup;
  }
  if (target->type != VFS_NODE_FILE) {
    status = VFS_STATUS_INVALID_ARG;
    goto cleanup;
  }

  status = lookup_parent_entry(new_path, &parent, &name, &name_len);
  if (status != VFS_STATUS_OK) { goto cleanup; }
  status = dir_mutation_allowed(parent, name, name_len);
  if (status != VFS_STATUS_OK) { goto cleanup; }

  vfs_mount_t* target_mount = NULL;
  vfs_mount_t* parent_mount = NULL;
  status = vnode_owning_mount(target, &target_mount);
  if (status == VFS_STATUS_OK) {
    status = vnode_owning_mount(parent, &parent_mount);
  }
  if (status != VFS_STATUS_OK) { goto cleanup; }
  if (target_mount != parent_mount) {
    status = VFS_STATUS_XDEV;
    goto cleanup;
  }

  if (HVFS_VOP_MISSING(parent, link)) {
    status = VFS_STATUS_NOT_IMPLEMENTED;
    goto cleanup;
  }

  status = parent->ops->link(parent, name, name_len, target);
  if (status == VFS_STATUS_OK) {
    mark_dir_changed(parent);
    target->link_count++;
  }

cleanup:
  vfs_vnode_release(parent);
  vfs_vnode_release(target);
  return status;
}

vfs_status_t vfs_symlink(const char* target, const char* link_path) {
  if (target == NULL || target[0] == '\0' || link_path == NULL) {
    return VFS_STATUS_INVALID_ARG;
  }

  uint32_t target_len = 0;
  while (target[target_len] != '\0') { target_len++; }

  vfs_node_t* parent = NULL;
  const char* name = NULL;
  uint32_t name_len = 0;
  vfs_status_t status =
      lookup_parent_entry(link_path, &parent, &name, &name_len);
  if (status != VFS_STATUS_OK) { return status; }
  status = dir_mutation_allowed(parent, name, name_len);
  if (status != VFS_STATUS_OK) { goto cleanup; }

  if (HVFS_VOP_MISSING(parent, symlink)) {
    status = VFS_STATUS_NOT_IMPLEMENTED;
    goto cleanup;
  }

  vfs_node_t* created = NULL;
  status = parent->ops->symlink(
      parent, name, name_len, target, target_len, &created);
  if (status == VFS_STATUS_OK) { mark_dir_changed(parent); }

cleanup:
  vfs_vnode_release(created);
  vfs_vnode_release(parent);
  return status;
}

vfs_status_t vfs_readlink(const char* path,
                          char* buffer,
                          uint64_t len,
                          uint64_t* bytes_read_out) {
  SET_OUT(bytes_read_out, 0);
  if (path == NULL || buffer == NULL) { return VFS_STATUS_INVALID_ARG; }

  vfs_node_t* link = NULL;
  vfs_status_t status = lookup_child_no_follow(path, &link);
  if (status != VFS_STATUS_OK) { return status; }
  if (link->type != VFS_NODE_SYMLINK) {
    status = VFS_STATUS_INVALID_ARG;
    goto cleanup;
  }
  if (HVFS_VOP_MISSING(link, readlink)) {
    status = VFS_STATUS_NOT_IMPLEMENTED;
    goto cleanup;
  }

  status = link->ops->readlink(link, buffer, len, bytes_read_out);

cleanup:
  vfs_vnode_release(link);
  return status;
}

vfs_status_t vfs_read(vfs_file_t* file,
                      void* buffer,
                      uint64_t len,
                      uint64_t* out_read) {
  SET_OUT(out_read, 0);
  if (file == NULL) { return VFS_STATUS_INVALID_ARG; }
  if (HVFS_FOP_MISSING(file, read)) { return VFS_STATUS_NOT_IMPLEMENTED; }
  vfs_status_t status = can_read(file);
  if (status != VFS_STATUS_OK) { return status; }

  uint64_t bytes_read = 0;
  uint64_t* bytes_read_out = out_read == NULL ? &bytes_read : out_read;
  status = file->ops->read(file, buffer, len, bytes_read_out);
  if (status == VFS_STATUS_OK && *bytes_read_out > 0) {
    file->vnode->accessed_timestamp = unix_time();
  }
  return status;
}

vfs_status_t vfs_write(vfs_file_t* file,
                       void* buffer,
                       uint64_t len,
                       uint64_t* bytes_written_out) {
  SET_OUT(bytes_written_out, 0);
  if (file == NULL) { return VFS_STATUS_INVALID_ARG; }
  if (HVFS_FOP_MISSING(file, write)) { return VFS_STATUS_NOT_IMPLEMENTED; }
  vfs_status_t status = can_write(file, buffer);
  if (status != VFS_STATUS_OK) { return status; }

  uint64_t bytes_written = 0;
  uint64_t* written_out =
      bytes_written_out == NULL ? &bytes_written : bytes_written_out;
  status = file->ops->write(file, buffer, len, written_out);
  if (status == VFS_STATUS_OK && *written_out > 0) {
    int64_t now = unix_time();
    file->vnode->modified_timestamp = now;
    file->vnode->changed_mdt_timestamp = now;
  }
  return status;
}

vfs_status_t vfs_readdir(vfs_file_t* dir, vfs_dirent_t** out) {
  SET_OUT(out, NULL);
  if (dir == NULL) { return VFS_STATUS_NOENT; }
  vfs_status_t status = can_readdir(dir);
  if (status != VFS_STATUS_OK) { return status; }
  if (HVFS_FOP_MISSING(dir, readdir)) { return VFS_STATUS_NOT_IMPLEMENTED; }

  return dir->ops->readdir(dir, out);
}

vfs_status_t vfs_seek(vfs_file_t* file,
                      int64_t offset,
                      vfs_seek_whence_t whence,
                      uint64_t* new_pos) {
  SET_OUT(new_pos, 0);
  if (file == NULL) { return VFS_STATUS_INVALID_ARG; }
  if (HVFS_FOP_MISSING(file, seek)) { return VFS_STATUS_NOT_IMPLEMENTED; }
  vfs_status_t status = can_seek(file, whence);
  if (status != VFS_STATUS_OK) { return status; }

  return file->ops->seek(file, offset, whence, new_pos);
}

vfs_status_t vfs_stat(const char* path, vfs_stat_t** out) {
  SET_OUT(out, NULL);
  vfs_node_t* vnode = NULL;
  vfs_status_t status = vfs_lookup(path, &vnode);
  if (status != VFS_STATUS_OK) { return status; }

  status = stat_vnode(vnode, out);

  vfs_vnode_release(vnode);
  return status;
}

vfs_status_t vfs_lstat(const char* path, vfs_stat_t** out) {
  SET_OUT(out, NULL);

  vfs_node_t* vnode = NULL;
  vfs_status_t status;
  if (path_is_root(path)) {
    status = vfs_lookup(path, &vnode);
  } else {
    status = lookup_child_no_follow(path, &vnode);
  }
  if (status != VFS_STATUS_OK) { return status; }

  status = stat_vnode(vnode, out);
  vfs_vnode_release(vnode);
  return status;
}

vfs_status_t vfs_fstat(vfs_file_t* file, vfs_stat_t** out) {
  SET_OUT(out, NULL);
  if (file == NULL) { return VFS_STATUS_NOENT; }
  return stat_vnode(file->vnode, out);
}

vfs_status_t vfs_close(vfs_file_t* file) {
  if (file == NULL) { return VFS_STATUS_OK; }
  if (file->refcount == 0) { return VFS_STATUS_INVALID_ARG; }
  if (file->refcount == 1 && HVFS_FOP_MISSING(file, close)) {
    return VFS_STATUS_NOT_IMPLEMENTED;
  }

  vfs_node_t* vnode = file->vnode;
  clear_process_fd(file);
  if (file->refcount > 1) {
    vfs_file_release(file);
    return VFS_STATUS_OK;
  }

  vfs_status_t status = file->ops->close(file);
  vfs_vnode_release(vnode);
  return status;
}

vfs_status_t vfs_ioctl(vfs_file_t* file, uint64_t request, void* args) {
  if (file == NULL) { return VFS_STATUS_BAD_FD; }
  vfs_status_t status = can_ioctl(file);
  if (status != VFS_STATUS_OK) { return status; }
  if (HVFS_FOP_MISSING(file, ioctl)) { return VFS_STATUS_NOT_IMPLEMENTED; }
  return file->ops->ioctl(file, request, args);
}

vfs_status_t vfs_resolve_fd(uint64_t fd, vfs_file_t** out) {
  SET_OUT(out, NULL);
  vfs_file_t* ret = sched_pb_fd_get(g_kernel.current_process, fd);
  if (ret == NULL) { return VFS_STATUS_BAD_FD; }
  SET_OUT(out, ret);
  return VFS_STATUS_OK;
}

void vfs_file_borrow(vfs_file_t* file) {
  if (file != NULL) { file->refcount++; }
}

void vfs_file_release(vfs_file_t* file) {
  if (file == NULL || file->refcount == 0) { return; }
  file->refcount--;
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
  char* cloned = (char*)calloc(alloc_len + 1, sizeof(char));
  if (cloned == NULL) { return NULL; }

  memcpy(cloned, name, name_len);
  if (trailing_slash) { cloned[name_len++] = '/'; }
  cloned[name_len] = '\0';
  return cloned;
}

int vfs_status_to_errno(vfs_status_t status) {
  switch (status) {
    case VFS_STATUS_OK:
      return 0;
    case VFS_STATUS_NOENT:
      return ENOENT;
    case VFS_STATUS_NOTDIR:
      return ENOTDIR;
    case VFS_STATUS_ISDIR:
      return EISDIR;
    case VFS_STATUS_NOMEM:
      return ENOMEM;
    case VFS_STATUS_INVALID_ARG:
      return EINVAL;
    case VFS_STATUS_TOO_MANY_OPEN:
      return EMFILE;
    case VFS_STATUS_BAD_FD:
      return EBADF;
    case VFS_STATUS_NOT_IMPLEMENTED:
      return ENOSYS;
    case VFS_STATUS_NOTEMPTY:
      return ENOTEMPTY;
    case VFS_STATUS_EXISTS:
      return EEXIST;
    case VFS_STATUS_XDEV:
      return EXDEV;
    case VFS_STATUS_LOOP:
      return ELOOP;
    default:
      return EINVAL;
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

static void cache_vnode_stat_timestamps(vfs_node_t* vnode, vfs_stat_t** out) {
  if (vnode == NULL || out == NULL || *out == NULL) { return; }

  (*out)->accessed_timestamp = vnode->accessed_timestamp;
  (*out)->modified_timestamp = vnode->modified_timestamp;
  (*out)->changed_mdt_timestamp = vnode->changed_mdt_timestamp;
  (*out)->link_count = vnode->link_count;
}

static vfs_status_t clone_symlink_target(vfs_node_t* vnode, char** out) {
  if (vnode == NULL || out == NULL) { return VFS_STATUS_INVALID_ARG; }
  *out = NULL;
  if (vnode->type != VFS_NODE_SYMLINK) { return VFS_STATUS_INVALID_ARG; }
  if (HVFS_VOP_MISSING(vnode, readlink)) { return VFS_STATUS_NOT_IMPLEMENTED; }

  char* target = (char*)calloc(VFS_SYMLINK_TARGET_MAX + 1, sizeof(char));
  if (target == NULL) { return VFS_STATUS_NOMEM; }

  uint64_t bytes_read = 0;
  vfs_status_t status =
      vnode->ops->readlink(vnode, target, VFS_SYMLINK_TARGET_MAX, &bytes_read);
  if (status != VFS_STATUS_OK) { goto cleanup; }
  if (bytes_read == 0 || bytes_read >= VFS_SYMLINK_TARGET_MAX) {
    status = VFS_STATUS_INVALID_ARG;
    goto cleanup;
  }

  target[bytes_read] = '\0';
  *out = target;
  return VFS_STATUS_OK;

cleanup:
  free(target);
  return status;
}

static bool vnode_is_file_like(vfs_node_t* vnode) {
  return vnode != NULL &&
         (vnode->type == VFS_NODE_FILE || vnode->type == VFS_NODE_DEVICE);
}

static vfs_status_t can_ioctl(vfs_file_t* file) {
  if (file == NULL || file->vnode == NULL) { return VFS_STATUS_BAD_FD; }
  if (file->vnode->type != VFS_NODE_DEVICE) {
    return VFS_STATUS_NOT_IMPLEMENTED;
  }
  return VFS_STATUS_OK;
}

static vfs_status_t can_open(vfs_node_t* vnode, uint32_t flags) {
  if (vnode == NULL) { return VFS_STATUS_INVALID_ARG; }
  if (flags & VFS_OPEN_DIRECTORY) {
    return vnode->type == VFS_NODE_DIR ? VFS_STATUS_OK : VFS_STATUS_NOTDIR;
  }
  if (vnode->type == VFS_NODE_DIR) { return VFS_STATUS_ISDIR; }
  if (!vnode_is_file_like(vnode)) { return VFS_STATUS_INVALID_ARG; }
  return VFS_STATUS_OK;
}

static vfs_status_t can_read(vfs_file_t* file) {
  if (file == NULL || file->vnode == NULL) { return VFS_STATUS_INVALID_ARG; }
  return (file->flags & VFS_OPEN_READ) ? VFS_STATUS_OK : VFS_STATUS_BAD_FD;
}

static vfs_status_t can_readdir(vfs_file_t* file) {
  if (file == NULL || file->vnode == NULL) { return VFS_STATUS_NOENT; }
  return file->vnode->type == VFS_NODE_DIR ? VFS_STATUS_OK : VFS_STATUS_NOTDIR;
}

static vfs_status_t can_seek(vfs_file_t* file, vfs_seek_whence_t whence) {
  if (file == NULL) { return VFS_STATUS_INVALID_ARG; }
  if (whence != VFS_SEEK_SET && whence != VFS_SEEK_CUR &&
      whence != VFS_SEEK_END) {
    return VFS_STATUS_INVALID_ARG;
  }
  return VFS_STATUS_OK;
}

static vfs_status_t can_write(vfs_file_t* file, void* buffer) {
  if (file == NULL || file->vnode == NULL) { return VFS_STATUS_INVALID_ARG; }
  if (!(file->flags & VFS_OPEN_WRITE)) { return VFS_STATUS_BAD_FD; }
  if (buffer == NULL) { return VFS_STATUS_INVALID_ARG; }
  return VFS_STATUS_OK;
}

static vfs_status_t component_parent(vfs_node_t** current) {
  if (current == NULL || *current == NULL) { return VFS_STATUS_INVALID_ARG; }
  if (*current == root_mount->root) { return VFS_STATUS_NOENT; }

  vfs_mount_t* mount = (*current)->mount;
  if (mount != NULL && mount != root_mount && *current == mount->root) {
    vfs_mount_t* parent_mount =
        mount->parent == NULL ? root_mount : mount->parent;
    if (mount->point == NULL || parent_mount == NULL) {
      return VFS_STATUS_INVALID_ARG;
    }

    if (mount->point == parent_mount->root) {
      vfs_vnode_borrow(parent_mount->root);
      vfs_vnode_release(*current);
      *current = parent_mount->root;
      return VFS_STATUS_OK;
    }

    if (HVFS_VOP_MISSING(mount->point, parent)) {
      return VFS_STATUS_NOT_IMPLEMENTED;
    }

    vfs_node_t* parent = NULL;
    vfs_status_t status = mount->point->ops->parent(mount->point, &parent);
    if (status != VFS_STATUS_OK) { return status; }
    if (parent == NULL) { return VFS_STATUS_NOENT; }

    vfs_vnode_release(*current);
    *current = parent;
    return VFS_STATUS_OK;
  }

  if (HVFS_VOP_MISSING(*current, parent)) { return VFS_STATUS_NOT_IMPLEMENTED; }

  vfs_node_t* parent = NULL;
  vfs_status_t status = (*current)->ops->parent(*current, &parent);
  if (status != VFS_STATUS_OK) { return status; }
  if (parent == NULL) { return VFS_STATUS_NOENT; }

  vfs_vnode_release(*current);
  *current = parent;
  return VFS_STATUS_OK;
}

static bool component_is_dot(const char* component, uint32_t component_len) {
  return component_len == 1 && component[0] == '.';
}

static bool component_is_dotdot(const char* component, uint32_t component_len) {
  return component_len == 2 && component[0] == '.' && component[1] == '.';
}

static void cross_mount(vfs_node_t** current) {
  if (current == NULL || *current == NULL) { return; }

  vfs_mount_t* mount = (*current)->mount;
  if (mount == NULL || mount->root == NULL || *current == mount->root) {
    return;
  }

  vfs_vnode_borrow(mount->root);
  vfs_vnode_release(*current);
  *current = mount->root;
}

static vfs_status_t dir_mutation_allowed(vfs_node_t* dir,
                                         const char* name,
                                         uint32_t name_len) {
  if (dir == NULL || name == NULL) { return VFS_STATUS_INVALID_ARG; }
  if (dir->type != VFS_NODE_DIR) { return VFS_STATUS_NOTDIR; }
  if (!vfs_validate_name(name, name_len)) { return VFS_STATUS_INVALID_ARG; }
  return VFS_STATUS_OK;
}

static char* join_symlink_path(const char* target, const char* remaining) {
  if (target == NULL) { return NULL; }
  if (remaining == NULL) { remaining = ""; }

  uint64_t target_len = strlen(target);
  uint64_t remaining_len = strlen(remaining);
  bool join = remaining_len > 0;
  uint64_t total_len = target_len + (join ? 1 + remaining_len : 0);

  char* joined = (char*)calloc(total_len + 1, sizeof(char));
  if (joined == NULL) { return NULL; }

  memcpy(joined, target, target_len);
  if (join) {
    joined[target_len] = '/';
    memcpy(joined + target_len + 1, remaining, remaining_len);
  }
  joined[total_len] = '\0';
  return joined;
}

static vfs_status_t lookup_child_no_follow(const char* path, vfs_node_t** out) {
  if (out == NULL) { return VFS_STATUS_INVALID_ARG; }
  *out = NULL;

  vfs_node_t* parent = NULL;
  const char* name = NULL;
  uint32_t name_len = 0;
  vfs_status_t status = lookup_parent_entry(path, &parent, &name, &name_len);
  if (status != VFS_STATUS_OK) { return status; }
  if (HVFS_VOP_MISSING(parent, lookup)) {
    vfs_vnode_release(parent);
    return VFS_STATUS_NOTDIR;
  }

  status = parent->ops->lookup(parent, name, name_len, out);
  vfs_vnode_release(parent);
  return status;
}

static vfs_status_t lookup_parent_entry(const char* path,
                                        vfs_node_t** parent_out,
                                        const char** name_out,
                                        uint32_t* name_len_out) {
  if (parent_out == NULL || name_out == NULL || name_len_out == NULL) {
    return VFS_STATUS_INVALID_ARG;
  }

  vfs_status_t status =
      vfs_lookup_parent(path, parent_out, name_out, name_len_out);
  if (status != VFS_STATUS_OK) { return status; }
  if (*name_out == NULL || *name_len_out == 0) {
    vfs_vnode_release(*parent_out);
    *parent_out = NULL;
    *name_out = NULL;
    *name_len_out = 0;
    return VFS_STATUS_INVALID_ARG;
  }

  return VFS_STATUS_OK;
}

static bool open_create_path_invalid(const char* path, uint32_t flags) {
  if (!(flags & VFS_OPEN_CREATE)) { return false; }
  if (flags & VFS_OPEN_DIRECTORY) { return true; }

  const char* path_end = path;
  while (*path_end != '\0') { path_end++; }
  return path_end > path + 1 && path_end[-1] == '/';
}

static bool path_is_root(const char* path) {
  if (path == NULL) { return false; }
  while (*path == '/') { path++; }
  return *path == '\0';
}

static vfs_status_t vnode_owning_mount(vfs_node_t* vnode, vfs_mount_t** out) {
  if (vnode == NULL || out == NULL) { return VFS_STATUS_INVALID_ARG; }
  if (root_mount == NULL || root_mount->root == NULL) {
    return VFS_STATUS_NOENT;
  }

  vfs_node_t* current = vnode;
  vfs_vnode_borrow(current);
  vfs_status_t status = VFS_STATUS_LOOP;

  for (uint32_t depth = 0; depth < VFS_SYMLINK_TARGET_MAX; ++depth) {
    if (current == root_mount->root) {
      *out = root_mount;
      status = VFS_STATUS_OK;
      goto cleanup;
    }
    if (current->mount != NULL && current == current->mount->root) {
      *out = current->mount;
      status = VFS_STATUS_OK;
      goto cleanup;
    }
    if (HVFS_VOP_MISSING(current, parent)) {
      status = VFS_STATUS_NOT_IMPLEMENTED;
      goto cleanup;
    }

    vfs_node_t* parent = NULL;
    status = current->ops->parent(current, &parent);
    if (status != VFS_STATUS_OK) { goto cleanup; }
    if (parent == NULL || parent == current) {
      vfs_vnode_release(parent);
      status = VFS_STATUS_NOENT;
      goto cleanup;
    }

    vfs_vnode_release(current);
    current = parent;
  }

cleanup:
  vfs_vnode_release(current);
  return status;
}

static vfs_status_t resolve_symlink_component(vfs_node_t* base,
                                              vfs_node_t* link,
                                              const char* remaining,
                                              uint32_t symlink_depth,
                                              vfs_node_t** out) {
  if (base == NULL || link == NULL || out == NULL) {
    return VFS_STATUS_INVALID_ARG;
  }
  *out = NULL;
  if (symlink_depth >= VFS_SYMLINK_MAX_DEPTH) { return VFS_STATUS_LOOP; }

  char* target = NULL;
  char* linked_path = NULL;
  vfs_status_t status = clone_symlink_target(link, &target);
  if (status != VFS_STATUS_OK) { goto cleanup; }

  linked_path = join_symlink_path(target, remaining);
  if (linked_path == NULL) {
    status = VFS_STATUS_NOMEM;
    goto cleanup;
  }

  vfs_lookup_request_t request = {.base = linked_path[0] == '/' ? NULL : base,
                                  .path = linked_path,
                                  .stop_at_parent = false,
                                  .symlink_depth = symlink_depth + 1};
  vfs_lookup_result_t result = {0};
  status = walk_path(&request, &result);
  if (status == VFS_STATUS_OK) { *out = result.vnode; }

cleanup:
  free(linked_path);
  free(target);
  return status;
}

static vfs_status_t stat_vnode(vfs_node_t* vnode, vfs_stat_t** out) {
  if (HVFS_VOP_MISSING(vnode, stat)) {
    SET_OUT(out, NULL);
    return VFS_STATUS_NOT_IMPLEMENTED;
  }

  vfs_status_t status = vnode->ops->stat(vnode, out);
  if (status == VFS_STATUS_OK) { cache_vnode_stat_timestamps(vnode, out); }
  return status;
}

static vfs_status_t walk_path(const vfs_lookup_request_t* request,
                              vfs_lookup_result_t* result) {
  if (request == NULL || result == NULL || request->path == NULL ||
      root_mount == NULL || root_mount->root == NULL) {
    return VFS_STATUS_NOENT;
  }

  result->vnode = NULL;
  result->final_name = NULL;
  result->final_name_len = 0;

  const char* path = request->path;
  if (path[0] == '\0') { return VFS_STATUS_NOENT; }

  vfs_node_t* base = request->base;
  if (path[0] != '/') {
    vfs_node_t* cwd = sched_pb_get_cwd(g_kernel.current_process);
    if (base == NULL) { base = cwd == NULL ? root_mount->root : cwd; }
  }

  vfs_node_t* current = path[0] == '/' ? root_mount->root : base;
  if (current == NULL) { return VFS_STATUS_NOENT; }
  vfs_vnode_borrow(current);
  cross_mount(&current);

  const char* cursor = path;

  while (*cursor == '/') { cursor++; }
  if (*cursor == '\0') {
    result->vnode = current;
    return VFS_STATUS_OK;
  }

  while (*cursor != '\0') {
    if (current->type != VFS_NODE_DIR || HVFS_VOP_MISSING(current, lookup)) {
      vfs_vnode_release(current);
      return VFS_STATUS_NOTDIR;
    }

    const char* component = cursor;
    uint32_t component_len = 0;
    while (*cursor != '\0' && *cursor != '/') {
      cursor++;
      component_len++;
    }

    while (*cursor == '/') { cursor++; }
    if (request->stop_at_parent && *cursor == '\0') {
      result->vnode = current;
      result->final_name = component;
      result->final_name_len = component_len;
      return VFS_STATUS_OK;
    }

    if (component_is_dot(component, component_len)) { continue; }
    if (component_is_dotdot(component, component_len)) {
      vfs_status_t status = component_parent(&current);
      if (status != VFS_STATUS_OK) {
        vfs_vnode_release(current);
        return status;
      }
      continue;
    }

    vfs_node_t* next = NULL;
    vfs_status_t status =
        current->ops->lookup(current, component, component_len, &next);
    if (status != VFS_STATUS_OK) {
      vfs_vnode_release(current);
      return status;
    }
    cross_mount(&next);

    if (next->type == VFS_NODE_SYMLINK) {
      vfs_node_t* resolved = NULL;
      status = resolve_symlink_component(
          current, next, cursor, request->symlink_depth, &resolved);
      vfs_vnode_release(next);
      vfs_vnode_release(current);
      if (status != VFS_STATUS_OK) { return status; }

      result->vnode = resolved;
      return VFS_STATUS_OK;
    }

    vfs_vnode_release(current);
    current = next;
  }

  result->vnode = current;
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
    status = VFS_STATUS_NOTDIR;
    goto cleanup;
  }
  if (!expect_dir && target->type == VFS_NODE_DIR) {
    status = VFS_STATUS_ISDIR;
    goto cleanup;
  }

  if (expect_dir) {
    status = dir->ops->rmdir(dir, name, name_len, flags);
  } else {
    status = dir->ops->unlink(dir, name, name_len, flags);
  }
  if (status != VFS_STATUS_OK) { goto cleanup; }

  if (target->link_count > 0) { target->link_count--; }
  mark_dir_changed(dir);

cleanup:
  vfs_vnode_release(target);
  return status;
}

static void mark_dir_changed(vfs_node_t* dir) {
  int64_t now = unix_time();
  dir->modified_timestamp = now;
  dir->changed_mdt_timestamp = now;
}

static void return_created_ref(vfs_node_t* created, vfs_node_t** out) {
  if (out == NULL) {
    vfs_vnode_release(created);
  } else {
    *out = created;
  }
}

static vfs_status_t validate_root_mount(vfs_mount_t* mount) {
  if (mount == NULL || mount->root == NULL) { return VFS_STATUS_NOMEM; }
  if (mount->root->type != VFS_NODE_DIR) { return VFS_STATUS_NOTDIR; }
  if (HVFS_VOP_MISSING(mount->root, lookup)) { return VFS_STATUS_NOTDIR; }

  return VFS_STATUS_OK;
}
