#include <fs/initrd.h>
#include <fs/ustar.h>
#include <fs/vfs.h>
#include <hlog.h>
#include <multitask/bootmodule.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SET_OUT(out, val)                                                      \
  if (out != NULL) { *out = val; }
#define SET_OUT_NULL(out) SET_OUT(out, NULL)

typedef struct initrd_inode initrd_inode_t;
struct initrd_inode {
  uint64_t number;
  char* name;
  uint64_t name_size;
  void* buf;
  uint64_t size;
  vfs_node_type_t type;
  initrd_inode_t* parent;
  initrd_inode_t* first_child;
  // TODO: currently we have to walk a linked list to get to target,
  // a hashmap would be better here.
  initrd_inode_t* next_sibling;
};

typedef struct initrd_file initrd_file_t;
struct initrd_file {
  union {
    initrd_inode_t* d_current;
    char* fbuf;
  } data;
};

void add_child(initrd_inode_t* parent, initrd_inode_t* new_child);
initrd_inode_t* create_dir(char* name,
                           uint64_t name_len,
                           initrd_inode_t* parent);
initrd_inode_t* create_root();
vfs_node_t* create_vnode(initrd_inode_t* inode);
initrd_inode_t* dir_lookup(char* path, uint64_t path_len, initrd_inode_t* root);
initrd_inode_t* find_child(initrd_inode_t* first_child,
                           char* name,
                           uint64_t name_len);
uint64_t get_entry_len(char* filename);
uint64_t get_header_size(ustar_header_t* header);
uint64_t get_name_start_idx(char* filename, uint64_t len);
initrd_inode_t* get_or_create_prefix(char* path,
                                     uint64_t path_len,
                                     initrd_inode_t* root);
bool is_zero_block(void* block);

static const vnode_ops_t initrd_vnode_ops = {
    .lookup = initrd_lookup,
    .open = initrd_open,
    .release = initrd_release,
    .stat = initrd_stat,
};

static const vfs_file_ops_t initrd_vfile_ops = {
    .read = initrd_read,
    .write = initrd_write,
    .readdir = initrd_readdir,
    .seek = initrd_seek,
    .close = initrd_close,
};

uint8_t initrd_initalize() {
  bootmodule_t* initrd_module = bootmodule_get("initrd.tar");
  if (initrd_module == NULL) {
    hlog_write(HLOG_ERROR, "Unable to find cached module initrd.tar");
    return VFS_STATUS_NOENT;
  } else {
    hlog_write(HLOG_INFO,
               "Loaded cached module %s at %x (%d bytes)",
               initrd_module->name,
               initrd_module->address,
               initrd_module->size);
  }
  vfs_mount_t* initrd = NULL;
  vfs_status_t ret =
      initrd_from_ustar(initrd_module->address, initrd_module->size, &initrd);
  if (ret != VFS_STATUS_OK) { return ret; }
  return vfs_mount_root(initrd);
}

vfs_status_t initrd_from_ustar(void* buffer,
                               uint64_t size,
                               vfs_mount_t** mount_out) {
  initrd_inode_t* root = create_root();

  uint64_t buf_pos = 0;
  while (buf_pos + HOJICHA_USTAR_HEADER_LEN_BYTES <= size) {
    ustar_header_t* h = (ustar_header_t*)(buffer + buf_pos);
    if (is_zero_block(h)) { break; }

    uint64_t entry_len = get_entry_len(h->filename);
    uint64_t header_size = get_header_size(h);

    if (entry_len == 2 && memcmp(h->filename, "./", 2) == 0) {
      buf_pos += header_size;
      continue;
    }

    uint64_t inode_count = 0;
    if (h->type == '0' || h->type == '\0') {
      uint64_t name_offset = get_name_start_idx(h->filename, entry_len);

      initrd_inode_t* new = (initrd_inode_t*)malloc(sizeof(initrd_inode_t));
      new->size = ustar_oct2dec(h->filesize_bytes_oct, 12);
      new->name = h->filename + name_offset;  // Skip leading './'
      new->name_size = entry_len - name_offset;
      new->buf = buffer + buf_pos + HOJICHA_USTAR_HEADER_LEN_BYTES;
      new->type = VFS_NODE_FILE;
      new->first_child = NULL;
      new->next_sibling = NULL;
      new->number = inode_count++;

      initrd_inode_t* parent;
      if (name_offset == 2) {  // Skip the leading `./`
        parent = root;
      } else {
        parent = get_or_create_prefix(h->filename, name_offset, root);
      }
      if (parent == NULL) { return VFS_STATUS_NOTDIR; }
      new->parent = parent;
      add_child(parent, new);
      buf_pos += header_size;
    } else if (h->type == '5') {
      if (entry_len > 0 && h->filename[entry_len - 1] == '/') { entry_len--; }

      if (entry_len > 2) {
        uint64_t name_offset = get_name_start_idx(h->filename, entry_len);
        uint64_t name_len = entry_len - name_offset;

        initrd_inode_t* parent;
        if (name_offset == 2) {
          parent = root;
        } else {
          parent = get_or_create_prefix(h->filename, name_offset, root);
        }
        if (parent == NULL) { return VFS_STATUS_NOTDIR; }

        if (find_child(parent->first_child,
                       h->filename + name_offset,
                       name_len) == NULL) {
          initrd_inode_t* dir =
              create_dir(h->filename + name_offset, name_len, parent);
          add_child(parent, dir);
        }
      }

      buf_pos += header_size;
    } else {
      buf_pos += header_size;
    }
  }

  vfs_node_t* root_vnode = create_vnode(root);
  if (root_vnode == NULL) { return VFS_STATUS_NOMEM; }

  vfs_mount_t* m = (vfs_mount_t*)malloc(sizeof(vfs_mount_t));
  m->root = root_vnode;
  m->fs_data = NULL;
  if (mount_out != NULL) { *mount_out = m; }
  return VFS_STATUS_OK;
}

vfs_status_t initrd_lookup(vfs_node_t* dir,
                           const char* name,
                           uint32_t name_len,
                           vfs_node_t** out) {
  if (dir == NULL || dir->type != VFS_NODE_DIR) { return VFS_STATUS_NOTDIR; }

  initrd_inode_t* dir_inode = (initrd_inode_t*)dir->fs_data;
  initrd_inode_t* child =
      find_child(dir_inode->first_child, (char*)name, name_len);
  if (child == NULL) { return VFS_STATUS_NOENT; }

  vfs_node_t* child_vnode = create_vnode(child);
  if (child_vnode == NULL) { return VFS_STATUS_NOMEM; }

  if (out != NULL) { *out = child_vnode; }
  return VFS_STATUS_OK;
}

vfs_status_t initrd_open(vfs_node_t* vnode, uint32_t flags, vfs_file_t** out) {
  if ((flags & VFS_OPEN_DIRECTORY) && vnode->type != VFS_NODE_DIR) {
    return VFS_STATUS_NOTDIR;
  }

  if ((flags & VFS_OPEN_READ) && vnode->type != VFS_NODE_FILE) {
    return VFS_STATUS_ISDIR;  // TODO: this should really be decided based
                              // on actual filetype
  }

  initrd_file_t* file = (initrd_file_t*)malloc(sizeof(initrd_file_t));
  vfs_file_t* vfile = (vfs_file_t*)malloc(sizeof(vfs_file_t));
  if (file == NULL || vfile == NULL) { return VFS_STATUS_NOMEM; }

  if (vnode->type == VFS_NODE_FILE) {
    file->data.fbuf = ((initrd_inode_t*)(vnode->fs_data))->buf;
  } else if (vnode->type == VFS_NODE_DIR) {
    file->data.d_current = ((initrd_inode_t*)(vnode->fs_data))->first_child;
  }

  vfile->flags = flags;
  vfile->fs_data = NULL;
  vfile->offset = 0;
  vfile->vnode = vnode;
  vfile->fs_data = (void*)file;
  vfile->ops = &initrd_vfile_ops;

  if (out != NULL) { *out = vfile; }
  return VFS_STATUS_OK;
}

vfs_status_t initrd_close(vfs_file_t* vfile) {
  free(vfile);
  return VFS_STATUS_OK;
}

vfs_status_t initrd_read(vfs_file_t* vfile,
                         void* buffer,
                         uint64_t len,
                         uint64_t* bytes_read_out) {
  if (vfile == NULL || buffer == NULL) { return VFS_STATUS_EOF; }

  initrd_inode_t* node = (initrd_inode_t*)((vfs_node_t*)vfile->vnode)->fs_data;
  if (vfile->offset >= node->size - 1) { return VFS_STATUS_NOENT; }

  uint64_t size_to_copy = node->size - 1 < len + vfile->offset
                              ? node->size - vfile->offset
                              : len - 1;
  void* src_buffer = ((initrd_file_t*)vfile->fs_data)->data.fbuf;
  memcpy(buffer, src_buffer + vfile->offset, size_to_copy);
  ((char*)buffer)[size_to_copy] = '\0';

  vfile->offset += size_to_copy;
  if (bytes_read_out != NULL) { *bytes_read_out = size_to_copy; }
  return VFS_STATUS_OK;
}

vfs_status_t initrd_write(vfs_file_t* file,
                          void* buffer,
                          uint64_t len,
                          uint64_t* bytes_written_out) {
  if (file == NULL || buffer == NULL) { return VFS_STATUS_INVALID_ARG; }

  uint64_t fsize = ((initrd_inode_t*)file->vnode->fs_data)->size;
  void* fbuf = ((initrd_file_t*)file->fs_data)->data.fbuf;
  if (len + file->offset < fsize - 1) {
    memcpy(fbuf + file->offset, buffer, len);
    file->offset += len;
    if (bytes_written_out != NULL) { *bytes_written_out = len; }
    return VFS_STATUS_OK;
  }

  uint64_t content_size = fsize + len;
  uint64_t new_size = content_size + (content_size >> 1);
  void* new_buf = malloc(sizeof(char) * new_size);
  if (new_buf == NULL) { return VFS_STATUS_NOMEM; }
  memcpy(new_buf, fbuf, fsize);
  memcpy(new_buf + fsize, buffer, len);
  ((initrd_file_t*)file->fs_data)->data.fbuf = new_buf;
  file->offset = fsize + len - 1;
  ((initrd_inode_t*)file->vnode->fs_data)->size = fsize + len;
  if (bytes_written_out != NULL) { *bytes_written_out = len; }
  return VFS_STATUS_NOMEM;
}

vfs_status_t initrd_readdir(vfs_file_t* vdir, vfs_dirent_t** out) {
  if (vdir == NULL) { return VFS_STATUS_NOENT; }
  if (vdir->vnode->type != VFS_NODE_DIR) { return VFS_STATUS_NOTDIR; }

  initrd_file_t* file = (initrd_file_t*)vdir->fs_data;

  if (file->data.d_current == NULL) {
    SET_OUT_NULL(out);
    return VFS_STATUS_EOF;
  }

  initrd_inode_t* current = file->data.d_current;
  file->data.d_current = file->data.d_current->next_sibling;
  vdir->offset++;

  // TODO don't abuse malloc once we have slab cache
  vfs_dirent_t* ret = (vfs_dirent_t*)malloc(sizeof(vfs_dirent_t));
  if (ret == NULL) { return VFS_STATUS_NOMEM; }

  ret->name = current->name;
  ret->inode_no = current->number;
  SET_OUT(out, ret);
  return VFS_STATUS_OK;
}

void initrd_release(vfs_node_t* vnode) { free(vnode); }

vfs_status_t initrd_seek(vfs_file_t* vfile,
                         int64_t offset,
                         vfs_seek_whence_t whence,
                         uint64_t* new_pos) {
  if (vfile == NULL) { return VFS_STATUS_INVALID_ARG; }

  uint64_t len = ((initrd_inode_t*)vfile->vnode->fs_data)->size;
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
      origin = (int64_t)len - 1;
      break;
  }

  if (offset + origin < 0) {
    pos = 0;
  } else if (offset + origin > (int64_t)len - 1) {
    pos = len - 1;
  } else {
    pos = offset + origin;
  }

  if (vfile->vnode->type == VFS_NODE_DIR) {
    initrd_file_t* file = (initrd_file_t*)vfile->fs_data;
    file->data.d_current =
        ((initrd_inode_t*)vfile->vnode->fs_data)->first_child;
    int i = *new_pos;
    while (i-- > 0 && file->data.d_current != NULL) {
      file->data.d_current = file->data.d_current->next_sibling;
    }
  }

  vfile->offset = pos;
  if (new_pos != NULL) { *new_pos = pos; }
  return VFS_STATUS_OK;
}

vfs_status_t initrd_stat(vfs_node_t* vnode, vfs_stat_t** out) {
  vfs_stat_t* ret = (vfs_stat_t*)malloc(sizeof(vfs_stat_t));
  if (ret == NULL) { return VFS_STATUS_NOMEM; }

  ret->size = ((initrd_inode_t*)vnode->fs_data)->size;
  ret->type = ((initrd_inode_t*)vnode->fs_data)->type;
  SET_OUT(out, ret);
  return VFS_STATUS_OK;
}

void add_child(initrd_inode_t* parent, initrd_inode_t* new_child) {
  new_child->next_sibling = NULL;

  if (parent->first_child == NULL) {
    parent->first_child = new_child;
    parent->size++;
    return;
  }

  initrd_inode_t* target = parent->first_child;
  while (target->next_sibling != NULL) { target = target->next_sibling; }
  target->next_sibling = new_child;
  parent->size++;
}

initrd_inode_t* create_dir(char* name,
                           uint64_t name_len,
                           initrd_inode_t* parent) {
  initrd_inode_t* dir = (initrd_inode_t*)malloc(sizeof(initrd_inode_t));
  char* dir_name = (char*)malloc(name_len + 1);
  if (dir == NULL || dir_name == NULL) { return NULL; }

  memcpy(dir_name, name, name_len);
  dir_name[name_len] = '\0';

  dir->name = dir_name;
  dir->name_size = name_len;
  dir->buf = NULL;
  dir->size = 0;
  dir->type = VFS_NODE_DIR;
  dir->parent = parent;
  dir->first_child = NULL;
  dir->next_sibling = NULL;
  return dir;
}

initrd_inode_t* create_root() {
  initrd_inode_t* root = (initrd_inode_t*)malloc(sizeof(initrd_inode_t));
  if (root == NULL) { return NULL; }
  root->name = "";
  root->name_size = 0;
  root->buf = NULL;
  root->size = 0;
  root->type = VFS_NODE_DIR;
  root->parent = NULL;
  root->first_child = NULL;
  root->next_sibling = NULL;
  return root;
}

vfs_node_t* create_vnode(initrd_inode_t* inode) {
  // TODO: stop malloc abuse once we have slab cache
  vfs_node_t* vnode = (vfs_node_t*)malloc(sizeof(vfs_node_t));
  if (vnode == NULL) { return NULL; }

  vnode->fs_data = inode;
  vnode->ops = &initrd_vnode_ops;
  vnode->refcount = 1;
  vnode->type = inode->type;
  return vnode;
}

initrd_inode_t* dir_lookup(char* path,
                           uint64_t path_len,
                           initrd_inode_t* root) {
  if (root == NULL) { return NULL; }

  uint64_t path_start = 0;
  initrd_inode_t* ret = root;

  if (path_len >= 2 && path[0] == '.' && path[1] == '/') { path_start = 2; }

  while (path_start < path_len) {
    while (path_start < path_len && path[path_start] == '/') { path_start++; }
    if (path_start >= path_len) { return ret; }

    uint64_t path_end = path_start;
    while (path_end < path_len && path[path_end] != '/') { path_end++; }

    uint64_t name_len = path_end - path_start;
    initrd_inode_t* next =
        find_child(ret->first_child, path + path_start, name_len);
    if (next == NULL || next->type != VFS_NODE_DIR) { return NULL; }

    ret = next;
    path_start = path_end + 1;
  }

  return ret;
}

initrd_inode_t* find_child(initrd_inode_t* first_child,
                           char* name,
                           uint64_t name_len) {
  for (initrd_inode_t* n = first_child; n != NULL; n = n->next_sibling) {
    if (n->name_size == name_len && memcmp(name, n->name, name_len) == 0) {
      return n;
    }
  }
  return NULL;
}

uint64_t get_entry_len(char* filename) {
  uint64_t len = 0;
  while (filename[len] != '\0') { len++; }
  return len;
}

uint64_t get_header_size(ustar_header_t* header) {
  uint64_t file_size = ustar_oct2dec(header->filesize_bytes_oct, 12);
  return (((file_size + 511) / 512) + 1) * 512;
}

uint64_t get_name_start_idx(char* filename, uint64_t len) {
  while (len > 0 && filename[len - 1] == '/') { len--; }
  --len;
  while (filename[len] != '/') { --len; }
  return len + 1;
}

/*
 * Ensures a directory path (prefix) exists from the given `root`.
 * Returns the inode to the last directory entry in the path.
 */
initrd_inode_t* get_or_create_prefix(char* path,
                                     uint64_t path_len,
                                     initrd_inode_t* root) {
  uint64_t path_start = 0;

  initrd_inode_t* ret = dir_lookup(path, path_len, root);
  if (ret != NULL) { return ret; }

  ret = root;
  if (path_len >= 2 && path[0] == '.' && path[1] == '/') { path_start = 2; }

  while (path_start < path_len) {
    while (path_start < path_len && path[path_start] == '/') { path_start++; }
    if (path_start >= path_len) { break; }

    uint64_t path_end = path_start;
    while (path_end < path_len && path[path_end] != '/') { path_end++; }

    uint64_t name_len = path_end - path_start;
    initrd_inode_t* next =
        find_child(ret->first_child, path + path_start, name_len);

    if (next == NULL) {
      next = create_dir(path + path_start, name_len, ret);
      add_child(ret, next);
    } else if (next->type != VFS_NODE_DIR) {
      return NULL;
    }

    ret = next;
    path_start = path_end + 1;
  }
  return ret;
}

bool is_zero_block(void* block) {
  unsigned char* bytes = (unsigned char*)block;

  for (uint64_t i = 0; i < HOJICHA_USTAR_HEADER_LEN_BYTES; ++i) {
    if (bytes[i] != 0) { return false; }
  }

  return true;
}

