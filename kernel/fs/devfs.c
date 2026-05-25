#include <fs/devfs.h>
#include <fs/initrd.h>
#include <fs/vfs.h>
#include <hlog.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SET_OUT(out, val)                                                      \
  if (out != NULL) { *out = val; }

typedef struct devfs_device devfs_device_t;
struct devfs_device {
  vfs_file_ops_t* file_ops;
  vfs_node_ops_t* node_ops;
};

typedef struct devfs_node devfs_node_t;
struct devfs_node {
  vfs_node_t* vnode;
  devfs_node_t* first_child;
  devfs_node_t* next;
  devfs_major_t major;
  uint64_t minor;
  char* name;
  uint64_t name_len;
};

typedef struct devfs_open_dir devfs_open_dir_t;
struct devfs_open_dir {
  devfs_node_t* current;
};

static vfs_node_t* devfs_root;
static devfs_device_t** dev_table;

static const vfs_node_ops_t devfs_node_ops = {.lookup = devfs_lookup,
                                              .open = devfs_open,
                                              .free = devfs_free,
                                              .create_file = devfs_create_file,
                                              .create_dir = devfs_create_dir,
                                              .stat = devfs_stat,
                                              .unlink = devfs_delete_file,
                                              .rmdir = devfs_delete_dir};

static const vfs_file_ops_t devfs_file_ops = {
    .read = devfs_read,
    .write = devfs_write,
    .readdir = devfs_readdir,
    .seek = devfs_seek,
    .close = devfs_close,
};

static devfs_node_t* create_child(devfs_node_t* parent,
                                  vfs_node_t* vnode,
                                  devfs_major_t major,
                                  uint64_t minor,
                                  const char* name,
                                  uint64_t name_len);
static bool delete_child(devfs_node_t* parent, devfs_node_t* child);
static devfs_node_t* find_child(devfs_node_t* first_child,
                                const char* name,
                                uint64_t name_len);

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
  dev_table = (devfs_device_t**)calloc(
      HOJICHA_MINOR_MAX, sizeof(devfs_device_t*) * HOJICHA_MAJOR_MAX);
  if (mount == NULL || devfs_root == NULL || dev_table == NULL) {
    free(mount);
    free(devfs_root);
    free(dev_table);
    hlog_write(HLOG_ERROR, "Error initializing devfs, OOM");
    vfs_vnode_release(root);
    vfs_vnode_release(dev);
    return false;
  }
  devfs_root->type = VFS_NODE_DIR;

  mount->root = devfs_root;
  mount->fs_data = NULL;
  vfs_status_t mount_st = vfs_mount(dev, mount, root->mount);
  if (mount_st != VFS_STATUS_OK) {
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

vfs_status_t devfs_register(devfs_major_t major,
                            uint64_t minor,
                            vfs_node_ops_t* node_ops,
                            vfs_file_ops_t* file_ops) {
  return VFS_STATUS_OK;
}

vfs_status_t devfs_unregister(devfs_major_t major, uint64_t minor) {
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
  SET_OUT(out, child->vnode)
  return VFS_STATUS_OK;
}

vfs_status_t devfs_open(vfs_node_t* vnode, uint32_t flags, vfs_file_t** out) {
  if (vnode == NULL || out == NULL) { return VFS_STATUS_INVALID_ARG; }
  *out = NULL;
  if ((flags & VFS_OPEN_DIRECTORY) && vnode->type != VFS_NODE_DIR) {
    return VFS_STATUS_NOTDIR;
  }

  if ((flags & VFS_OPEN_READ) && vnode->type != VFS_NODE_FILE) {
    return VFS_STATUS_ISDIR;  // TODO: this should really be decided based
                              // on actual filetype
  }

  devfs_open_dir_t* dir = (devfs_open_dir_t*)malloc(sizeof(devfs_open_dir_t));
  vfs_file_t* vfile = (vfs_file_t*)malloc(sizeof(vfs_file_t));
  if (dir == NULL || vfile == NULL) {
    free(dir);
    free(vfile);
    return VFS_STATUS_NOMEM;
  }

  if (vnode->type == VFS_NODE_DIR) {
    dir->current = ((devfs_node_t*)(vnode->fs_data))->first_child;
  }

  vfile->flags = flags;
  vfile->fs_data = NULL;
  vfile->offset = 0;
  vfile->vnode = vnode;
  vfile->fs_data = (void*)dir;
  vfile->ops = &devfs_file_ops;

  if (out != NULL) { *out = vfile; }
  return VFS_STATUS_OK;
}

vfs_status_t devfs_close(vfs_file_t* vfile) {
  return VFS_STATUS_NOT_IMPLEMENTED;
}

vfs_status_t devfs_read(vfs_file_t* vfile,
                        void* buffer,
                        uint64_t len,
                        uint64_t* bytes_read_out) {
  return VFS_STATUS_NOT_IMPLEMENTED;
}
vfs_status_t devfs_write(vfs_file_t* file,
                         void* buffer,
                         uint64_t len,
                         uint64_t* bytes_written_out) {
  return VFS_STATUS_NOT_IMPLEMENTED;
}
vfs_status_t devfs_readdir(vfs_file_t* vdir, vfs_dirent_t** out) {
  return VFS_STATUS_NOT_IMPLEMENTED;
}
vfs_status_t devfs_seek(vfs_file_t* vfile,
                        int64_t offset,
                        vfs_seek_whence_t whence,
                        uint64_t* new_pos) {
  return VFS_STATUS_NOT_IMPLEMENTED;
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
  return VFS_STATUS_NOT_IMPLEMENTED;
}

vfs_status_t devfs_delete_file(vfs_node_t* dir,
                               const char* name,
                               uint32_t name_len,
                               uint32_t flags) {
  return VFS_STATUS_NOT_IMPLEMENTED;
}
vfs_status_t devfs_delete_dir(vfs_node_t* dir,
                              const char* name,
                              uint32_t name_len,
                              uint32_t flags) {
  return VFS_STATUS_NOT_IMPLEMENTED;
}

vfs_status_t devfs_stat(vfs_node_t* vnode, vfs_stat_t** out) {
  return VFS_STATUS_NOT_IMPLEMENTED;
}

void devfs_free(vfs_node_t* vnode) {}

static devfs_node_t* create_child(devfs_node_t* parent,
                                  vfs_node_t* vnode,
                                  devfs_major_t major,
                                  uint64_t minor,
                                  const char* name,
                                  uint64_t name_len) {
  if (parent == NULL || name == NULL || name_len == 0) { return NULL; }

  devfs_node_t* child = (devfs_node_t*)malloc(sizeof(devfs_node_t));
  if (child == NULL) { return NULL; }

  char* owned_name = (char*)malloc(name_len + 1);
  if (owned_name == NULL) {
    free(child);
    return NULL;
  }

  memcpy(owned_name, name, name_len);
  owned_name[name_len] = '\0';

  child->vnode = vnode;
  child->first_child = NULL;
  child->next = parent->first_child;
  child->major = major;
  child->minor = minor;
  child->name = owned_name;
  child->name_len = name_len;

  parent->first_child = child;
  return child;
}

static bool delete_child(devfs_node_t* parent, devfs_node_t* child) {
  if (parent == NULL || child == NULL || child->first_child != NULL) {
    return false;
  }

  devfs_node_t* prev = NULL;
  devfs_node_t* cur = parent->first_child;
  while (cur != NULL) {
    if (cur == child) {
      if (prev == NULL) {
        parent->first_child = cur->next;
      } else {
        prev->next = cur->next;
      }

      free(cur->name);
      free(cur);
      return true;
    }

    prev = cur;
    cur = cur->next;
  }

  return false;
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

