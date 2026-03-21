#include <fs/vfs.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

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

vfs_status_t vfs_lookup(const char* absolute_path, vnode_t** out) {
  if (absolute_path == NULL || out == NULL || root_mount == NULL ||
      root_mount->root == NULL || absolute_path[0] != '/') {
    return VFS_STATUS_NOENT;
  }

  vnode_t* current = root_mount->root;
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

    vnode_t* next = NULL;
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
                      vfile_t** out) {
  vnode_t* target = NULL;
  vfs_status_t lookup_res = vfs_lookup(absolute_path, &target);
  if (lookup_res != VFS_STATUS_OK) {
    *out = NULL;
    return lookup_res;
  }

  return target->ops->open(target, flags, out);
}
