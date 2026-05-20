#include <fs/initrd.h>
#include <fs/ustar.h>
#include <fs/vfs.h>
#include <hlog.h>
#include <multitask/bootmodule.h>
#include <multitask/scheduler.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SET_OUT(out, val)                                                      \
  if (out != NULL) { *out = val; }
#define SET_OUT_NULL(out) SET_OUT(out, NULL)

typedef struct initrd_inode initrd_inode_t;
struct initrd_inode {
  vfs_node_t vnode;
  uint64_t number;
  const char* name;
  uint64_t name_size;
  bool name_owned;
  void* buf;
  uint64_t bufsize;
  bool buf_owned;
  uint64_t len;
  initrd_inode_t* parent;
  initrd_inode_t* first_child;
  // TODO: currently we have to walk a linked list to get to target,
  // a hashmap would be better here.
  initrd_inode_t* next_sibling;
};

typedef struct initrd_file initrd_file_t;
struct initrd_file {
  initrd_inode_t* d_current;
};

void add_child(initrd_inode_t* parent, initrd_inode_t* new_child);
initrd_inode_t* create_dir(const char* name,
                           uint64_t name_len,
                           initrd_inode_t* parent);
initrd_inode_t* create_root();
void init_vnode(initrd_inode_t* inode, vfs_node_type_t type);
vfs_node_t* borrow_vnode(initrd_inode_t* inode);
initrd_inode_t* dir_lookup(const char* path,
                           uint64_t path_len,
                           initrd_inode_t* root);
initrd_inode_t* find_child(initrd_inode_t* first_child,
                           const char* name,
                           uint64_t name_len);
uint64_t get_entry_len(char* filename);
uint64_t get_header_size(ustar_header_t* header);
uint64_t get_name_start_idx(char* filename, uint64_t len);
initrd_inode_t* get_or_create_prefix(const char* path,
                                     uint64_t path_len,
                                     initrd_inode_t* root);
initrd_inode_t* detach_child(initrd_inode_t* parent,
                             const char* name,
                             uint64_t name_len);
bool is_zero_block(void* block);
vfs_status_t load_ustar(void* buffer, uint64_t size, vfs_mount_t** mount_out);
bool validate_name(const char* name, uint64_t name_len);
char* clone_name(const char* name, uint64_t name_len, bool trailing_slash);

static const vnode_ops_t initrd_vnode_ops = {.lookup = initrd_lookup,
                                             .open = initrd_open,
                                             .free = initrd_free,
                                             .create_file = initrd_create_file,
                                             .create_dir = initrd_create_dir,
                                             .stat = initrd_stat,
                                             .unlink = initrd_delete_file,
                                             .rmdir = initrd_delete_dir};

static const vfs_file_ops_t initrd_vfile_ops = {
    .read = initrd_read,
    .write = initrd_write,
    .readdir = initrd_readdir,
    .seek = initrd_seek,
    .close = initrd_close,
};

static uint64_t inode_count = 0;

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
      load_ustar(initrd_module->address, initrd_module->size, &initrd);
  if (ret != VFS_STATUS_OK) { return ret; }
  return vfs_mount_root(initrd);
}

vfs_status_t initrd_lookup(vfs_node_t* dir,
                           const char* name,
                           uint32_t name_len,
                           vfs_node_t** out) {
  if (dir == NULL || dir->type != VFS_NODE_DIR) { return VFS_STATUS_NOTDIR; }

  initrd_inode_t* dir_inode = (initrd_inode_t*)dir->fs_data;
  initrd_inode_t* child = find_child(dir_inode->first_child, name, name_len);
  if (child == NULL) { return VFS_STATUS_NOENT; }

  if (out != NULL) { *out = borrow_vnode(child); }
  return VFS_STATUS_OK;
}

vfs_status_t initrd_open(vfs_node_t* vnode, uint32_t flags, vfs_file_t** out) {
  if (vnode == NULL || out == NULL) { return VFS_STATUS_INVALID_ARG; }
  *out = NULL;
  if ((flags & VFS_OPEN_DIRECTORY) && vnode->type != VFS_NODE_DIR) {
    return VFS_STATUS_NOTDIR;
  }

  if ((flags & VFS_OPEN_READ) && vnode->type != VFS_NODE_FILE) {
    return VFS_STATUS_ISDIR;  // TODO: this should really be decided based
                              // on actual filetype
  }

  initrd_file_t* file = (initrd_file_t*)malloc(sizeof(initrd_file_t));
  vfs_file_t* vfile = (vfs_file_t*)malloc(sizeof(vfs_file_t));
  if (file == NULL || vfile == NULL) {
    free(file);
    free(vfile);
    return VFS_STATUS_NOMEM;
  }

  if (vnode->type == VFS_NODE_DIR) {
    file->d_current = ((initrd_inode_t*)(vnode->fs_data))->first_child;
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
  free(vfile->fs_data);
  free(vfile);
  return VFS_STATUS_OK;
}

vfs_status_t initrd_read(vfs_file_t* vfile,
                         void* buffer,
                         uint64_t len,
                         uint64_t* bytes_read_out) {
  if (vfile == NULL || buffer == NULL) { return VFS_STATUS_INVALID_ARG; }

  initrd_inode_t* node = (initrd_inode_t*)((vfs_node_t*)vfile->vnode)->fs_data;
  if (vfile->offset >= node->len) {
    SET_OUT(bytes_read_out, 0);
    return VFS_STATUS_EOF;
  }

  uint64_t remaining = node->len - vfile->offset;
  uint64_t size_to_copy = remaining < len ? remaining : len;

  sched_postpone();
  memcpy(buffer, node->buf + vfile->offset, size_to_copy);
  vfile->offset += size_to_copy;
  sched_resume();

  if (bytes_read_out != NULL) { *bytes_read_out = size_to_copy; }
  return VFS_STATUS_OK;
}

vfs_status_t initrd_write(vfs_file_t* file,
                          void* buffer,
                          uint64_t len,
                          uint64_t* bytes_written_out) {
  if (file == NULL || buffer == NULL) { return VFS_STATUS_INVALID_ARG; }
  initrd_inode_t* inode = (initrd_inode_t*)file->vnode->fs_data;
  uint64_t fsize = inode->len;
  uint64_t bufsize = inode->bufsize;

  uint64_t content_size = fsize + len;
  uint64_t new_size = content_size + (content_size >> 1);
  if (inode->buf == NULL || bufsize == 0) {
    sched_postpone();
    void* new_buf = malloc(new_size);
    sched_resume();

    if (new_buf == NULL) { return VFS_STATUS_NOMEM; }
    inode->buf = new_buf;
    inode->bufsize = new_size;
    inode->buf_owned = true;
    bufsize = new_size;
  }

  if (len + file->offset < bufsize - 1) {
    sched_postpone();
    memcpy(inode->buf + file->offset, buffer, len);
    file->offset += len;
    inode->len += len;
    sched_resume();

    if (bytes_written_out != NULL) { *bytes_written_out = len; }
    return VFS_STATUS_OK;
  }

  void* new_buf = malloc(sizeof(char) * new_size);
  if (new_buf == NULL) { return VFS_STATUS_NOMEM; }

  void* old;
  sched_postpone();
  memcpy(new_buf, inode->buf, fsize);
  memcpy(new_buf + fsize, buffer, len);
  old = inode->buf;
  bool old_buf_owned = inode->buf_owned;
  inode->buf = new_buf;
  inode->bufsize = new_size;
  inode->buf_owned = true;
  file->offset = fsize + len - 1;
  inode->len = fsize + len;
  if (old_buf_owned) { free(old); }
  sched_resume();

  if (bytes_written_out != NULL) { *bytes_written_out = len; }
  return VFS_STATUS_OK;
}

vfs_status_t initrd_readdir(vfs_file_t* vdir, vfs_dirent_t** out) {
  if (vdir == NULL) { return VFS_STATUS_NOENT; }
  if (vdir->vnode->type != VFS_NODE_DIR) { return VFS_STATUS_NOTDIR; }

  initrd_file_t* file = (initrd_file_t*)vdir->fs_data;

  if (file->d_current == NULL) {
    SET_OUT_NULL(out);
    return VFS_STATUS_EOF;
  }

  initrd_inode_t* current = file->d_current;
  file->d_current = file->d_current->next_sibling;
  vdir->offset++;

  // TODO don't abuse malloc once we have slab cache
  vfs_dirent_t* ret = (vfs_dirent_t*)malloc(sizeof(vfs_dirent_t));
  if (ret == NULL) { return VFS_STATUS_NOMEM; }

  ret->name = clone_name(
      current->name, current->name_size, current->vnode.type == VFS_NODE_DIR);
  if (ret->name == NULL) {
    free(ret);
    return VFS_STATUS_NOMEM;
  }
  ret->inode_no = current->number;
  SET_OUT(out, ret);
  return VFS_STATUS_OK;
}

void initrd_free(vfs_node_t* vnode) {
  if (vnode == NULL) { return; }

  initrd_inode_t* inode = (initrd_inode_t*)vnode->fs_data;
  if (inode != NULL) {
    if (inode->name_owned) { free((void*)inode->name); }
    if (inode->buf_owned) { free(inode->buf); }
    free(inode);
  }
}

vfs_status_t initrd_seek(vfs_file_t* vfile,
                         int64_t offset,
                         vfs_seek_whence_t whence,
                         uint64_t* new_pos) {
  if (vfile == NULL) { return VFS_STATUS_INVALID_ARG; }

  uint64_t len = ((initrd_inode_t*)vfile->vnode->fs_data)->len;
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
    file->d_current = ((initrd_inode_t*)vfile->vnode->fs_data)->first_child;
    int i = *new_pos;
    while (i-- > 0 && file->d_current != NULL) {
      file->d_current = file->d_current->next_sibling;
    }
  }

  vfile->offset = pos;
  if (new_pos != NULL) { *new_pos = pos; }
  return VFS_STATUS_OK;
}

vfs_status_t initrd_create_file(vfs_node_t* dir,
                                const char* name,
                                uint32_t name_len,
                                vfs_node_t** out) {
  if (dir == NULL || dir->type != VFS_NODE_DIR ||
      !validate_name(name, name_len)) {
    return VFS_STATUS_INVALID_ARG;
  }

  initrd_inode_t* inode = malloc(sizeof(initrd_inode_t));
  if (inode == NULL) {
    SET_OUT_NULL(out);
    return VFS_STATUS_NOMEM;
  }

  char* file_name = clone_name(name, name_len, false);
  if (file_name == NULL) {
    free(inode);
    SET_OUT_NULL(out);
    return VFS_STATUS_NOMEM;
  }

  init_vnode(inode, VFS_NODE_FILE);
  inode->name = file_name;
  inode->name_size = name_len;
  inode->name_owned = true;
  inode->number = inode_count++;
  inode->buf = NULL;
  inode->bufsize = 0;
  inode->buf_owned = false;
  inode->len = 0;
  inode->parent = (initrd_inode_t*)dir->fs_data;

  add_child((initrd_inode_t*)dir->fs_data, inode);

  SET_OUT(out, borrow_vnode(inode));
  return VFS_STATUS_OK;
}

vfs_status_t initrd_create_dir(vfs_node_t* dir,
                               const char* name,
                               uint32_t name_len,
                               vfs_node_t** out) {
  if (dir == NULL || dir->type != VFS_NODE_DIR || name == NULL) {
    SET_OUT_NULL(out);
    return VFS_STATUS_INVALID_ARG;
  }

  while (name_len > 0 && name[name_len - 1] == '/') { --name_len; }
  if (!validate_name(name, name_len)) {
    SET_OUT_NULL(out);
    return VFS_STATUS_INVALID_ARG;
  }

  initrd_inode_t* inode =
      create_dir(name, name_len, (initrd_inode_t*)dir->fs_data);
  if (inode == NULL) {
    SET_OUT_NULL(out);
    return VFS_STATUS_NOMEM;
  }

  add_child((initrd_inode_t*)dir->fs_data, inode);

  SET_OUT(out, borrow_vnode(inode));
  return VFS_STATUS_OK;
}

vfs_status_t initrd_delete_file(vfs_node_t* dir,
                                const char* name,
                                uint32_t name_len,
                                uint32_t flags) {
  if (dir == NULL || dir->type != VFS_NODE_DIR || name == NULL ||
      name_len == 0) {
    return VFS_STATUS_INVALID_ARG;
  }

  initrd_inode_t* idir = (initrd_inode_t*)dir->fs_data;
  initrd_inode_t* child = find_child(idir->first_child, name, name_len);
  if (child == NULL) { return VFS_STATUS_NOENT; }
  if (child->vnode.type == VFS_NODE_DIR) { return VFS_STATUS_ISDIR; }

  return detach_child(idir, name, name_len) == NULL ? VFS_STATUS_NOENT
                                                    : VFS_STATUS_OK;
}

vfs_status_t initrd_delete_dir(vfs_node_t* dir,
                               const char* name,
                               uint32_t name_len,
                               uint32_t flags) {
  if (dir == NULL || dir->type != VFS_NODE_DIR || name == NULL ||
      name_len == 0) {
    return VFS_STATUS_INVALID_ARG;
  }

  initrd_inode_t* idir = (initrd_inode_t*)dir->fs_data;
  initrd_inode_t* child = find_child(idir->first_child, name, name_len);
  if (child == NULL) { return VFS_STATUS_NOENT; }
  if (child->vnode.type != VFS_NODE_DIR) { return VFS_STATUS_NOTDIR; }
  if (child->first_child != NULL) { return VFS_STATUS_NOTEMPTY; }

  return detach_child(idir, name, name_len) == NULL ? VFS_STATUS_NOENT
                                                    : VFS_STATUS_OK;
}

vfs_status_t initrd_stat(vfs_node_t* vnode, vfs_stat_t** out) {
  vfs_stat_t* ret = (vfs_stat_t*)malloc(sizeof(vfs_stat_t));
  if (ret == NULL) { return VFS_STATUS_NOMEM; }

  ret->size = ((initrd_inode_t*)vnode->fs_data)->len;
  ret->type = vnode->type;
  SET_OUT(out, ret);
  return VFS_STATUS_OK;
}

void add_child(initrd_inode_t* parent, initrd_inode_t* new_child) {
  new_child->next_sibling = NULL;

  if (parent->first_child == NULL) {
    parent->first_child = new_child;
    parent->len++;
    return;
  }

  initrd_inode_t* target = parent->first_child;
  while (target->next_sibling != NULL) { target = target->next_sibling; }
  target->next_sibling = new_child;
  parent->len++;
}

initrd_inode_t* create_dir(const char* name,
                           uint64_t name_len,
                           initrd_inode_t* parent) {
  initrd_inode_t* dir = (initrd_inode_t*)malloc(sizeof(initrd_inode_t));
  char* dir_name = clone_name(name, name_len, false);
  if (dir == NULL || dir_name == NULL) {
    free(dir);
    free(dir_name);
    return NULL;
  }

  dir->name = dir_name;
  dir->name_size = name_len;
  dir->name_owned = true;
  dir->buf = NULL;
  dir->bufsize = 0;
  dir->buf_owned = false;
  dir->len = 0;
  dir->number = inode_count++;
  dir->parent = parent;
  dir->first_child = NULL;
  dir->next_sibling = NULL;
  init_vnode(dir, VFS_NODE_DIR);
  return dir;
}

initrd_inode_t* create_root() {
  initrd_inode_t* root = (initrd_inode_t*)malloc(sizeof(initrd_inode_t));
  if (root == NULL) { return NULL; }
  root->name = "";
  root->name_size = 0;
  root->name_owned = false;
  root->buf = NULL;
  root->bufsize = 0;
  root->buf_owned = false;
  root->len = 0;
  root->parent = NULL;
  root->first_child = NULL;
  root->next_sibling = NULL;
  init_vnode(root, VFS_NODE_DIR);
  return root;
}

void init_vnode(initrd_inode_t* inode, vfs_node_type_t type) {
  inode->vnode.fs_data = inode;
  inode->vnode.ops = &initrd_vnode_ops;
  inode->vnode.refcount = 0;
  inode->vnode.link_count = 1;
  inode->vnode.type = type;
}

vfs_node_t* borrow_vnode(initrd_inode_t* inode) {
  inode->vnode.refcount++;
  return &inode->vnode;
}

initrd_inode_t* dir_lookup(const char* path,
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
    if (next == NULL || next->vnode.type != VFS_NODE_DIR) { return NULL; }

    ret = next;
    path_start = path_end + 1;
  }

  return ret;
}

initrd_inode_t* find_child(initrd_inode_t* first_child,
                           const char* name,
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
initrd_inode_t* get_or_create_prefix(const char* path,
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
    } else if (next->vnode.type != VFS_NODE_DIR) {
      return NULL;
    }

    ret = next;
    path_start = path_end + 1;
  }
  return ret;
}

initrd_inode_t* detach_child(initrd_inode_t* parent,
                             const char* name,
                             uint64_t name_len) {
  initrd_inode_t* prev = NULL;
  for (initrd_inode_t* n = parent->first_child; n != NULL;
       n = n->next_sibling) {
    if (n->name_size == name_len && memcmp(name, n->name, name_len) == 0) {
      if (prev == NULL) {
        parent->first_child = n->next_sibling;
      } else {
        prev->next_sibling = n->next_sibling;
      }
      n->next_sibling = NULL;
      if (parent->len > 0) { parent->len--; }
      return n;
    }
    prev = n;
  }
  return NULL;
}

bool is_zero_block(void* block) {
  unsigned char* bytes = (unsigned char*)block;

  for (uint64_t i = 0; i < HOJICHA_USTAR_HEADER_LEN_BYTES; ++i) {
    if (bytes[i] != 0) { return false; }
  }

  return true;
}

bool validate_name(const char* name, uint64_t name_len) {
  // TODO: More thorough validation
  if (name == NULL || name_len == 0) { return false; }

  for (uint64_t i = 0; i < name_len; ++i) {
    if (name[i] == '/' || (name[i] == '\0' && i < name_len - 1)) {
      return false;
    }
  }

  return true;
}

char* clone_name(const char* name, uint64_t name_len, bool trailing_slash) {
  uint64_t alloc_len = name_len + (trailing_slash ? 1 : 0);
  char* cloned = (char*)malloc(alloc_len + 1);
  if (cloned == NULL) { return NULL; }

  memcpy(cloned, name, name_len);
  if (trailing_slash) { cloned[name_len++] = '/'; }
  cloned[name_len] = '\0';
  return cloned;
}

vfs_status_t load_ustar(void* buffer, uint64_t size, vfs_mount_t** mount_out) {
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

    if (h->type == '0' || h->type == '\0') {
      uint64_t name_offset = get_name_start_idx(h->filename, entry_len);

      initrd_inode_t* new = (initrd_inode_t*)malloc(sizeof(initrd_inode_t));
      new->len = ustar_oct2dec(h->filesize_bytes_oct, 12);
      new->bufsize = new->len;
      new->buf_owned = false;
      new->name = h->filename + name_offset;  // Skip leading './'
      new->name_size = entry_len - name_offset;
      new->name_owned = false;
      new->buf = buffer + buf_pos + HOJICHA_USTAR_HEADER_LEN_BYTES;
      new->first_child = NULL;
      new->next_sibling = NULL;
      new->number = inode_count++;
      init_vnode(new, VFS_NODE_FILE);

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

  vfs_mount_t* m = (vfs_mount_t*)malloc(sizeof(vfs_mount_t));
  if (m == NULL) { return VFS_STATUS_NOMEM; }
  m->root = &root->vnode;
  m->fs_data = NULL;
  if (mount_out != NULL) { *mount_out = m; }
  return VFS_STATUS_OK;
}
