#include <fs/devfs.h>
#include <fs/vfs.h>
#include <kernel/ktime.h>
#include <utils/set_out.h>
#include <hlog.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct devfs_node devfs_node_t;
struct devfs_node {
  vfs_node_t* vnode;
  devfs_node_t* parent;
  devfs_node_t* first_child;
  devfs_node_t* next;
  devfs_major_t major;
  uint64_t minor;
  char* name;
  uint64_t name_len;
  char* symlink_target;
  uint64_t symlink_target_len;
  uint64_t len;
};

typedef struct devfs_device devfs_device_t;
struct devfs_device {
  vfs_file_ops_t* file_ops;
  vfs_node_ops_t* node_ops;
  devfs_node_t* node;
};

typedef struct devfs_open_dir devfs_open_dir_t;
struct devfs_open_dir {
  devfs_node_t* current;
};

static vfs_node_t* devfs_root;
static devfs_node_t* devfs_root_node;
static devfs_device_t* (*dev_table)[HOJICHA_MINOR_MAX];

static const vfs_node_ops_t devfs_node_ops = {.lookup = devfs_lookup,
                                              .parent = devfs_parent,
                                              .name = devfs_name,
                                              .open = devfs_open,
                                              .free = devfs_free,
                                              .create_file = devfs_create_file,
                                              .create_dir = devfs_create_dir,
                                              .stat = devfs_stat,
                                              .unlink = devfs_delete_file,
                                              .rmdir = devfs_delete_dir,
                                              .symlink = devfs_symlink,
                                              .readlink = devfs_readlink};

static const vfs_file_ops_t devfs_file_ops = {
    .read = devfs_read,
    .write = devfs_write,
    .readdir = devfs_readdir,
    .seek = devfs_seek,
    .ioctl = devfs_ioctl,
    .close = devfs_close,
};

static vfs_status_t dir_seek(vfs_file_t* vfile,
                             int64_t offset,
                             vfs_seek_whence_t whence,
                             uint64_t* new_pos);
static void init_vnode(vfs_node_t* vnode,
                       vfs_node_type_t type,
                       devfs_node_t* node);
static devfs_node_t* create_child(devfs_node_t* parent,
                                  vfs_node_type_t type,
                                  devfs_major_t major,
                                  uint64_t minor,
                                  const char* name,
                                  uint64_t name_len);
static devfs_node_t* detach_child(devfs_node_t* target);
static devfs_node_t* find_child(devfs_node_t* first_child,
                                const char* name,
                                uint64_t name_len);
static devfs_device_t* get_device(devfs_node_t* node);
static void init_vfile(vfs_file_t* vfile, vfs_node_t* vnode, uint32_t flags);

bool devfs_initialize() {
  vfs_node_t* root = NULL;
  vfs_status_t lookup_status = vfs_lookup("/", &root);
  if (lookup_status != VFS_STATUS_OK) {
    hlog_write(HLOG_ERROR,
               "Error initializing devfs, could not lookup root: %d",
               lookup_status);
    return false;
  }
  vfs_node_t* dev = NULL;
  vfs_status_t mkdir_status = vfs_mkdir(root, "dev", 3, &dev);
  if (mkdir_status != VFS_STATUS_OK) {
    hlog_write(HLOG_ERROR,
               "Error initializing devfs, could not create mount dir: %d",
               mkdir_status);
    vfs_vnode_release(root);
    return false;
  }

  vfs_mount_t* mount = (vfs_mount_t*)calloc(1, sizeof(vfs_mount_t));
  devfs_root = (vfs_node_t*)calloc(1, sizeof(vfs_node_t));
  devfs_root_node = (devfs_node_t*)calloc(1, sizeof(devfs_node_t));
  dev_table = calloc(HOJICHA_MAJOR_MAX, sizeof(*dev_table));
  if (mount == NULL || devfs_root == NULL || devfs_root_node == NULL ||
      dev_table == NULL) {
    free(mount);
    free(devfs_root);
    free(devfs_root_node);
    free(dev_table);
    hlog_write(HLOG_ERROR, "Error initializing devfs, OOM");
    vfs_vnode_release(root);
    vfs_vnode_release(dev);
    return false;
  }
  init_vnode(devfs_root, VFS_NODE_DIR, devfs_root_node);
  devfs_root_node->vnode = devfs_root;
  devfs_root_node->parent = NULL;

  mount->root = devfs_root;
  mount->fs_data = NULL;
  vfs_status_t mount_st = vfs_mount(dev, mount, root->mount);
  if (mount_st != VFS_STATUS_OK) {
    free(mount);
    free(devfs_root_node);
    free(devfs_root);
    free(dev_table);
    hlog_write(HLOG_ERROR,
               "Error initializing devfs, could not mount filesystem: %d",
               mkdir_status);
    vfs_vnode_release(root);
    vfs_vnode_release(dev);
    return false;
  }

  vfs_vnode_release(root);
  vfs_vnode_release(dev);
  return true;
}

devfs_device_t* devfs_device_new(vfs_file_ops_t* file_ops,
                                 vfs_node_ops_t* node_ops) {
  devfs_device_t* dev = (devfs_device_t*)calloc(1, sizeof(devfs_device_t));
  if (dev == NULL) { return NULL; }
  dev->file_ops = file_ops;
  dev->node_ops = node_ops;
  return dev;
}

vfs_status_t devfs_register(devfs_major_t major,
                            uint64_t minor,
                            devfs_device_t* dev,
                            const char* name,
                            uint64_t name_len) {
  if (dev_table[major][minor] != NULL) { return VFS_STATUS_EXISTS; }
  if (dev == NULL) { return VFS_STATUS_INVALID_ARG; }
  devfs_node_t* node = create_child(
      devfs_root_node, VFS_NODE_DEVICE, major, minor, name, name_len);
  if (node == NULL) { return VFS_STATUS_NOMEM; }

  dev->node = node;
  dev_table[major][minor] = dev;
  return VFS_STATUS_OK;
}

vfs_status_t devfs_unregister(devfs_major_t major, uint64_t minor) {
  if (dev_table[major][minor] == NULL) { return VFS_STATUS_NOENT; }
  detach_child(dev_table[major][minor]->node);
  dev_table[major][minor] = NULL;
  return VFS_STATUS_OK;
}

vfs_status_t devfs_lookup(vfs_node_t* dir,
                          const char* name,
                          uint32_t name_len,
                          vfs_node_t** out) {
  if (dir == NULL || dir->type != VFS_NODE_DIR) { return VFS_STATUS_NOTDIR; }

  devfs_node_t* dir_node = (devfs_node_t*)dir->fs_data;
  devfs_node_t* child = find_child(dir_node->first_child, name, name_len);
  if (child == NULL) { return VFS_STATUS_NOENT; }

  vfs_vnode_borrow(child->vnode);
  SET_OUT(out, child->vnode);
  return VFS_STATUS_OK;
}

vfs_status_t devfs_parent(vfs_node_t* dir, vfs_node_t** out) {
  if (dir == NULL || out == NULL) { return VFS_STATUS_INVALID_ARG; }

  devfs_node_t* node = (devfs_node_t*)dir->fs_data;
  if (node == NULL) { return VFS_STATUS_NOENT; }

  devfs_node_t* parent = node->parent == NULL ? node : node->parent;
  vfs_vnode_borrow(parent->vnode);
  *out = parent->vnode;
  return VFS_STATUS_OK;
}

vfs_status_t devfs_name(vfs_node_t* vnode,
                        const char** name_out,
                        uint32_t* name_len_out) {
  if (vnode == NULL || name_out == NULL || name_len_out == NULL) {
    return VFS_STATUS_INVALID_ARG;
  }

  devfs_node_t* node = (devfs_node_t*)vnode->fs_data;
  if (node == NULL || node->name == NULL || node->name_len > UINT32_MAX) {
    return VFS_STATUS_NOENT;
  }

  *name_out = node->name;
  *name_len_out = (uint32_t)node->name_len;
  return VFS_STATUS_OK;
}

vfs_status_t devfs_open(vfs_node_t* vnode, uint32_t flags, vfs_file_t** out) {
  if (vnode == NULL || out == NULL) { return VFS_STATUS_INVALID_ARG; }
  SET_OUT(out, NULL);
  if ((flags & VFS_OPEN_DIRECTORY) && vnode->type != VFS_NODE_DIR) {
    return VFS_STATUS_NOTDIR;
  }

  if ((flags & VFS_OPEN_READ) && vnode->type != VFS_NODE_DEVICE) {
    return VFS_STATUS_ISDIR;
  }

  if (vnode->type == VFS_NODE_DIR) {
    devfs_open_dir_t* dir =
        (devfs_open_dir_t*)calloc(1, sizeof(devfs_open_dir_t));
    vfs_file_t* vfile = (vfs_file_t*)calloc(1, sizeof(vfs_file_t));
    if (dir == NULL || vfile == NULL) {
      free(dir);
      free(vfile);
      return VFS_STATUS_NOMEM;
    }

    dir->current = ((devfs_node_t*)(vnode->fs_data))->first_child;
    vfile->flags = flags;
    vfile->refcount = 1;
    vfile->fs_data = (void*)dir;
    vfile->offset = 0;
    vfile->vnode = vnode;
    vfile->ops = &devfs_file_ops;
    *out = vfile;
    return VFS_STATUS_OK;
  }

  devfs_node_t* dev_node = (devfs_node_t*)vnode->fs_data;
  devfs_device_t* dev = get_device(dev_node);
  if (dev == NULL) { return VFS_STATUS_NOENT; }

  vfs_file_t* vfile = (vfs_file_t*)calloc(1, sizeof(vfs_file_t));
  if (vfile == NULL) { return VFS_STATUS_NOMEM; }

  init_vfile(vfile, vnode, flags);
  if (dev->node_ops != NULL && dev->node_ops->open != NULL) {
    // Device node open hooks initialize the preallocated file handle in-place.
    vfs_file_t* opened = vfile;
    vfs_status_t status = dev->node_ops->open(vnode, flags, &opened);
    if (status != VFS_STATUS_OK) {
      free(vfile);
      return status;
    }
    if (opened != vfile) {
      free(opened);
      free(vfile);
      return VFS_STATUS_INVALID_ARG;
    }
  }

  *out = vfile;
  return VFS_STATUS_OK;
}

vfs_status_t devfs_close(vfs_file_t* vfile) {
  if (vfile == NULL) { return VFS_STATUS_OK; }

  if (vfile->vnode->type == VFS_NODE_DIR) {
    free(vfile->fs_data);
    free(vfile);
    return VFS_STATUS_OK;
  }

  devfs_node_t* dev_node = (devfs_node_t*)vfile->vnode->fs_data;
  devfs_device_t* dev = get_device(dev_node);
  if (dev != NULL && dev->file_ops != NULL && dev->file_ops->close != NULL) {
    vfs_status_t status = dev->file_ops->close(vfile);
    if (status != VFS_STATUS_OK) { return status; }
  }

  free(vfile);
  return VFS_STATUS_OK;
}

vfs_status_t devfs_read(vfs_file_t* vfile,
                        void* buffer,
                        uint64_t len,
                        uint64_t* bytes_read_out) {
  devfs_node_t* dev_node = (devfs_node_t*)vfile->vnode->fs_data;
  devfs_device_t* dev = get_device(dev_node);
  if (dev == NULL || dev->file_ops == NULL || dev->file_ops->read == NULL) {
    return VFS_STATUS_NOT_IMPLEMENTED;
  }
  return dev->file_ops->read(vfile, buffer, len, bytes_read_out);
}

vfs_status_t devfs_write(vfs_file_t* file,
                         void* buffer,
                         uint64_t len,
                         uint64_t* bytes_written_out) {
  devfs_node_t* dev_node = (devfs_node_t*)file->vnode->fs_data;
  devfs_device_t* dev = get_device(dev_node);
  if (dev == NULL || dev->file_ops == NULL || dev->file_ops->write == NULL) {
    return VFS_STATUS_NOT_IMPLEMENTED;
  }
  return dev->file_ops->write(file, buffer, len, bytes_written_out);
}

vfs_status_t devfs_readdir(vfs_file_t* vdir, vfs_dirent_t** out) {
  if (vdir == NULL) { return VFS_STATUS_NOENT; }
  if (vdir->vnode->type != VFS_NODE_DIR) { return VFS_STATUS_NOTDIR; }

  devfs_open_dir_t* file = (devfs_open_dir_t*)vdir->fs_data;

  if (file->current == NULL) {
    SET_OUT(out, NULL);
    return VFS_STATUS_OK;
  }

  devfs_node_t* current = file->current;
  file->current = file->current->next;
  vdir->offset++;

  vfs_dirent_t* ret = (vfs_dirent_t*)calloc(1, sizeof(vfs_dirent_t));
  if (ret == NULL) { return VFS_STATUS_NOMEM; }

  ret->name = vfs_clone_name(
      current->name, current->name_len, current->vnode->type == VFS_NODE_DIR);
  if (ret->name == NULL) {
    free(ret);
    return VFS_STATUS_NOMEM;
  }
  ret->inode_no = 0;
  SET_OUT(out, ret);
  return VFS_STATUS_OK;
}

vfs_status_t devfs_seek(vfs_file_t* vfile,
                        int64_t offset,
                        vfs_seek_whence_t whence,
                        uint64_t* new_pos) {
  if (vfile->vnode->type == VFS_NODE_DIR) {
    return dir_seek(vfile, offset, whence, new_pos);
  }

  devfs_node_t* dev_node = (devfs_node_t*)vfile->vnode->fs_data;
  devfs_device_t* dev = get_device(dev_node);
  if (dev == NULL || dev->file_ops == NULL || dev->file_ops->seek == NULL) {
    return VFS_STATUS_NOT_IMPLEMENTED;
  }
  return dev->file_ops->seek(vfile, offset, whence, new_pos);
}

vfs_status_t devfs_ioctl(vfs_file_t* file, uint64_t number, void* args) {
  devfs_node_t* dev_node = (devfs_node_t*)file->vnode->fs_data;
  devfs_device_t* dev = get_device(dev_node);
  if (dev == NULL || dev->file_ops == NULL || dev->file_ops->ioctl == NULL) {
    return VFS_STATUS_NOT_IMPLEMENTED;
  }
  return dev->file_ops->ioctl(file, number, args);
}

vfs_status_t devfs_create_file(vfs_node_t* dir,
                               const char* name,
                               uint32_t name_len,
                               vfs_node_t** out) {
  return VFS_STATUS_NOT_IMPLEMENTED;
}

vfs_status_t devfs_create_dir(vfs_node_t* dir,
                              const char* name,
                              uint32_t name_len,
                              vfs_node_t** out) {
  if (dir == NULL || dir->type != VFS_NODE_DIR || name == NULL) {
    SET_OUT(out, NULL);
    return VFS_STATUS_INVALID_ARG;
  }

  while (name_len > 0 && name[name_len - 1] == '/') { --name_len; }
  if (!vfs_validate_name(name, name_len)) {
    SET_OUT(out, NULL);
    return VFS_STATUS_INVALID_ARG;
  }

  devfs_node_t* dir_node = (devfs_node_t*)dir->fs_data;
  devfs_node_t* child =
      create_child(dir_node, VFS_NODE_DIR, 0, 0, name, name_len);
  if (child == NULL) {
    SET_OUT(out, NULL);
    return VFS_STATUS_NOMEM;
  }

  vfs_vnode_borrow(child->vnode);
  SET_OUT(out, child->vnode);
  return VFS_STATUS_OK;
}

vfs_status_t devfs_delete_file(vfs_node_t* dir,
                               const char* name,
                               uint32_t name_len,
                               uint32_t flags) {
  if (dir == NULL || dir->type != VFS_NODE_DIR || name == NULL ||
      name_len == 0) {
    return VFS_STATUS_INVALID_ARG;
  }

  devfs_node_t* dir_node = (devfs_node_t*)dir->fs_data;
  devfs_node_t* child = find_child(dir_node->first_child, name, name_len);
  if (child == NULL) { return VFS_STATUS_NOENT; }
  if (child->vnode->type == VFS_NODE_DIR) { return VFS_STATUS_ISDIR; }
  if (child->vnode->type != VFS_NODE_SYMLINK) {
    return VFS_STATUS_NOT_IMPLEMENTED;
  }

  return detach_child(child) == NULL ? VFS_STATUS_NOENT : VFS_STATUS_OK;
}
vfs_status_t devfs_delete_dir(vfs_node_t* dir,
                              const char* name,
                              uint32_t name_len,
                              uint32_t flags) {
  if (dir == NULL || dir->type != VFS_NODE_DIR || name == NULL ||
      name_len == 0) {
    return VFS_STATUS_INVALID_ARG;
  }

  devfs_node_t* dir_node = (devfs_node_t*)dir->fs_data;
  devfs_node_t* child = find_child(dir_node->first_child, name, name_len);
  if (child == NULL) { return VFS_STATUS_NOENT; }
  if (child->vnode->type != VFS_NODE_DIR) { return VFS_STATUS_NOTDIR; }
  if (child->first_child != NULL) { return VFS_STATUS_NOTEMPTY; }

  return detach_child(child) == NULL ? VFS_STATUS_NOENT : VFS_STATUS_OK;
}

vfs_status_t devfs_symlink(vfs_node_t* dir,
                           const char* name,
                           uint32_t name_len,
                           const char* target,
                           uint32_t target_len,
                           vfs_node_t** out) {
  if (dir == NULL || dir->type != VFS_NODE_DIR ||
      !vfs_validate_name(name, name_len) || target == NULL ||
      target_len == 0) {
    SET_OUT(out, NULL);
    return VFS_STATUS_INVALID_ARG;
  }

  devfs_node_t* dir_node = (devfs_node_t*)dir->fs_data;
  if (find_child(dir_node->first_child, name, name_len) != NULL) {
    SET_OUT(out, NULL);
    return VFS_STATUS_EXISTS;
  }

  devfs_node_t* child =
      create_child(dir_node, VFS_NODE_SYMLINK, 0, 0, name, name_len);
  if (child == NULL) {
    SET_OUT(out, NULL);
    return VFS_STATUS_NOMEM;
  }

  child->symlink_target = vfs_clone_name(target, target_len, false);
  if (child->symlink_target == NULL) {
    detach_child(child);
    devfs_free(child->vnode);
    SET_OUT(out, NULL);
    return VFS_STATUS_NOMEM;
  }
  child->symlink_target_len = target_len;
  child->len = target_len;

  vfs_vnode_borrow(child->vnode);
  SET_OUT(out, child->vnode);
  return VFS_STATUS_OK;
}

vfs_status_t devfs_readlink(vfs_node_t* vnode,
                            char* buffer,
                            uint64_t len,
                            uint64_t* bytes_read_out) {
  if (vnode == NULL || vnode->type != VFS_NODE_SYMLINK || buffer == NULL) {
    SET_OUT(bytes_read_out, 0);
    return VFS_STATUS_INVALID_ARG;
  }

  devfs_node_t* node = (devfs_node_t*)vnode->fs_data;
  uint64_t copy_len = node->symlink_target_len;
  if (copy_len > len) { copy_len = len; }
  memcpy(buffer, node->symlink_target, copy_len);
  SET_OUT(bytes_read_out, copy_len);
  return VFS_STATUS_OK;
}

vfs_status_t devfs_stat(vfs_node_t* vnode, vfs_stat_t** out) {
  if (vnode->type == VFS_NODE_DIR || vnode->type == VFS_NODE_SYMLINK) {
    vfs_stat_t* ret = (vfs_stat_t*)calloc(1, sizeof(vfs_stat_t));
    if (ret == NULL) { return VFS_STATUS_NOMEM; }

    ret->size = ((devfs_node_t*)vnode->fs_data)->len;
    ret->type = vnode->type;
    ret->link_count = vnode->link_count;
    SET_OUT(out, ret);
    return VFS_STATUS_OK;
  }

  devfs_node_t* dev_node = (devfs_node_t*)vnode->fs_data;
  devfs_device_t* dev = get_device(dev_node);
  if (dev == NULL || dev->node_ops == NULL || dev->node_ops->stat == NULL) {
    return VFS_STATUS_NOT_IMPLEMENTED;
  }
  return dev->node_ops->stat(vnode, out);
}

void devfs_free(vfs_node_t* vnode) {
  if (vnode == NULL) { return; }

  if (vnode->type == VFS_NODE_DEVICE) {
    devfs_node_t* dev_node = (devfs_node_t*)vnode->fs_data;
    devfs_device_t* dev = get_device(dev_node);
    if (dev != NULL && dev->node_ops != NULL && dev->node_ops->free != NULL) {
      dev->node_ops->free(vnode);
    }
  }

  devfs_node_t* node = (devfs_node_t*)vnode->fs_data;
  if (node != NULL) {
    free(node->name);
    free(node->symlink_target);
    free(node);
  }
  free(vnode);
}

static devfs_node_t* create_child(devfs_node_t* parent,
                                  vfs_node_type_t type,
                                  devfs_major_t major,
                                  uint64_t minor,
                                  const char* name,
                                  uint64_t name_len) {
  if (parent == NULL || !vfs_validate_name(name, name_len)) { return NULL; }

  devfs_node_t* child = (devfs_node_t*)calloc(1, sizeof(devfs_node_t));
  vfs_node_t* vnode = (vfs_node_t*)calloc(1, sizeof(vfs_node_t));
  char* owned_name = vfs_clone_name(name, name_len, false);
  if (child == NULL || vnode == NULL || owned_name == NULL) {
    free(child);
    free(vnode);
    free(owned_name);
    return NULL;
  }

  child->vnode = vnode;
  child->parent = parent;
  child->first_child = NULL;
  child->next = parent->first_child;
  child->major = major;
  child->minor = minor;
  child->name = owned_name;
  child->name_len = name_len;
  child->symlink_target = NULL;
  child->symlink_target_len = 0;
  child->len = 0;
  init_vnode(vnode, type, child);

  parent->first_child = child;
  parent->len++;
  return child;
}

static devfs_node_t* detach_child(devfs_node_t* target) {
  if (target == NULL || target->parent == NULL) { return NULL; }

  devfs_node_t* parent = target->parent;
  devfs_node_t* prev = NULL;
  for (devfs_node_t* cur = parent->first_child; cur != NULL; cur = cur->next) {
    if (cur == target) {
      if (prev == NULL) {
        parent->first_child = cur->next;
      } else {
        prev->next = cur->next;
      }
      cur->next = NULL;
      cur->parent = NULL;
      if (parent->len > 0) { parent->len--; }
      return cur;
    }

    prev = cur;
  }

  return NULL;
}

static vfs_status_t dir_seek(vfs_file_t* vfile,
                             int64_t offset,
                             vfs_seek_whence_t whence,
                             uint64_t* new_pos) {
  uint64_t len = ((devfs_node_t*)vfile->vnode->fs_data)->len;
  uint64_t pos;
  int64_t origin;
  switch (whence) {
    case VFS_SEEK_SET:
      origin = 0;
      break;
    case VFS_SEEK_CUR:
      origin = (int64_t)vfile->offset;
      break;
    case VFS_SEEK_END:
      origin = (int64_t)len;
      break;
  }

  if (offset + origin < 0) {
    pos = 0;
  } else if (offset + origin > (int64_t)len) {
    pos = len;
  } else {
    pos = offset + origin;
  }

  devfs_open_dir_t* dir = (devfs_open_dir_t*)vfile->fs_data;
  dir->current = ((devfs_node_t*)vfile->vnode->fs_data)->first_child;
  uint64_t i = pos;
  while (i-- > 0 && dir->current != NULL) { dir->current = dir->current->next; }

  vfile->offset = pos;
  SET_OUT(new_pos, pos);
  return VFS_STATUS_OK;
}

static void init_vnode(vfs_node_t* vnode,
                       vfs_node_type_t type,
                       devfs_node_t* node) {
  vnode->fs_data = node;
  vnode->ops = &devfs_node_ops;
  vnode->refcount = 0;
  vnode->link_count = 1;
  vnode->mount = NULL;
  vnode->type = type;
  int64_t now = unix_time();
  vnode->accessed_timestamp = now;
  vnode->modified_timestamp = now;
  vnode->changed_mdt_timestamp = now;
}

static devfs_device_t* get_device(devfs_node_t* node) {
  if (node == NULL || node->major >= HOJICHA_MAJOR_MAX ||
      node->minor >= HOJICHA_MINOR_MAX) {
    return NULL;
  }

  return dev_table[node->major][node->minor];
}

static void init_vfile(vfs_file_t* vfile, vfs_node_t* vnode, uint32_t flags) {
  vfile->flags = flags;
  vfile->refcount = 1;
  vfile->fs_data = NULL;
  vfile->offset = 0;
  vfile->vnode = vnode;
  vfile->ops = &devfs_file_ops;
}

static devfs_node_t* find_child(devfs_node_t* first_child,
                                const char* name,
                                uint64_t name_len) {
  for (devfs_node_t* n = first_child; n != NULL; n = n->next) {
    if (n->name_len == name_len && memcmp(name, n->name, name_len) == 0) {
      return n;
    }
  }
  return NULL;
}
