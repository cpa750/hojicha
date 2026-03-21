#include <fs/initrd.h>
#include <fs/ustar.h>
#include <fs/vfs.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct ird_inode ird_inode_t;
struct ird_inode {
  char* name;
  uint64_t name_size;
  void* buf;
  uint64_t size;
  vfs_node_type_t type;
  ird_inode_t* parent;
  ird_inode_t* first_child;
  // TODO: currently we have to walk a linked list to get to target,
  // a hashmap would be better here.
  ird_inode_t* next_sibling;
};

typedef struct ird_file ird_file_t;
struct ird_file {
  char* buf;
};

vfs_status_t initrd_lookup(vnode_t* dir,
                           const char* name,
                           uint32_t name_len,
                           vnode_t** out);
void initrd_release(vnode_t* vnode);
vnode_t* create_vnode(ird_inode_t* inode);
void add_child(ird_inode_t* parent, ird_inode_t* new_child);
ird_inode_t* create_root();
ird_inode_t* create_dir(char* name, uint64_t name_len, ird_inode_t* parent);
ird_inode_t* dir_lookup(char* path, uint64_t path_len, ird_inode_t* root);
ird_inode_t* find_child(ird_inode_t* first_child,
                        char* name,
                        uint64_t name_len);
ird_inode_t* get_or_create_prefix(char* path,
                                  uint64_t path_len,
                                  ird_inode_t* root);
uint64_t get_entry_len(char* filename);
uint64_t get_header_size(ustar_header_t* header);
uint64_t get_name_start_idx(char* filename, uint64_t len);
bool is_zero_block(void* block);

static const vnode_ops_t initrd_vnode_ops = {
    .lookup = initrd_lookup,
    .open = NULL,
    .release = initrd_release,
};

vfs_status_t initrd_from_ustar(void* buffer,
                               uint64_t size,
                               vfs_mount_t** mount_out) {
  ird_inode_t* root = create_root();

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

      ird_inode_t* new = (ird_inode_t*)malloc(sizeof(ird_inode_t));
      new->size = ustar_oct2dec(h->filesize_bytes_oct, 12);
      new->name = h->filename + name_offset;  // Skip leading './'
      new->name_size = entry_len - name_offset;
      new->buf = buffer + buf_pos + HOJICHA_USTAR_HEADER_LEN_BYTES;
      new->type = VFS_NODE_FILE;
      new->first_child = NULL;
      new->next_sibling = NULL;

      ird_inode_t* parent;
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

        ird_inode_t* parent;
        if (name_offset == 2) {
          parent = root;
        } else {
          parent = get_or_create_prefix(h->filename, name_offset, root);
        }
        if (parent == NULL) { return VFS_STATUS_NOTDIR; }

        if (find_child(parent->first_child,
                       h->filename + name_offset,
                       name_len) == NULL) {
          ird_inode_t* dir =
              create_dir(h->filename + name_offset, name_len, parent);
          add_child(parent, dir);
        }
      }

      buf_pos += header_size;
    } else {
      buf_pos += header_size;
    }
  }

  vnode_t* root_vnode = create_vnode(root);
  if (root_vnode == NULL) { return VFS_STATUS_NOMEM; }

  vfs_mount_t* m = (vfs_mount_t*)malloc(sizeof(vfs_mount_t));
  m->root = root_vnode;
  m->fs_data = NULL;
  *mount_out = m;
  return VFS_STATUS_OK;
}

vfs_status_t initrd_lookup(vnode_t* dir,
                           const char* name,
                           uint32_t name_len,
                           vnode_t** out) {
  if (dir == NULL || dir->type != VFS_NODE_DIR) { return VFS_STATUS_NOTDIR; }

  ird_inode_t* dir_inode = (ird_inode_t*)dir->fs_data;
  ird_inode_t* child =
      find_child(dir_inode->first_child, (char*)name, name_len);
  if (child == NULL) { return VFS_STATUS_NOENT; }

  vnode_t* child_vnode = create_vnode(child);
  if (child_vnode == NULL) { return VFS_STATUS_NOMEM; }

  *out = child_vnode;
  return VFS_STATUS_OK;
}

void initrd_release(vnode_t* vnode) { free(vnode); }

vfs_status_t initrd_open(vnode_t* vnode, uint32_t flags, vfile_t** out) {
  if ((flags & VFS_OPEN_DIRECTORY) && vnode->type != VFS_NODE_DIR) {
    *out = NULL;
    return VFS_STATUS_NOTDIR;
  }

  if ((flags & VFS_OPEN_READ) && vnode->type != VFS_NODE_FILE) {
    *out = NULL;
    return VFS_STATUS_ISDIR;  // TODO: this should really be decided based
                              // on actual filetype
  }

  ird_file_t* file = (ird_file_t*)malloc(sizeof(ird_file_t));
  file->buf = ((ird_inode_t*)(vnode->fs_data))->buf;

  vfile_t* vfile = (vfile_t*)malloc(sizeof(vfile_t));
  vfile->flags = flags;
  vfile->fs_data = NULL;
  vfile->offset = 0;
  vfile->fs_data = (void*)file;

  *out = vfile;
  return VFS_STATUS_OK;
}

vnode_t* create_vnode(ird_inode_t* inode) {
  vnode_t* vnode = (vnode_t*)malloc(sizeof(vnode_t));
  if (vnode == NULL) { return NULL; }

  vnode->fs_data = inode;
  vnode->ops = &initrd_vnode_ops;
  vnode->refcount = 1;
  vnode->type = inode->type;
  return vnode;
}

ird_inode_t* create_root() {
  ird_inode_t* root = (ird_inode_t*)malloc(sizeof(ird_inode_t));
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

void add_child(ird_inode_t* parent, ird_inode_t* new_child) {
  new_child->next_sibling = NULL;

  if (parent->first_child == NULL) {
    parent->first_child = new_child;
    return;
  }

  ird_inode_t* target = parent->first_child;
  while (target->next_sibling != NULL) { target = target->next_sibling; }
  target->next_sibling = new_child;
}

ird_inode_t* create_dir(char* name, uint64_t name_len, ird_inode_t* parent) {
  ird_inode_t* dir = (ird_inode_t*)malloc(sizeof(ird_inode_t));
  char* dir_name = (char*)malloc(name_len + 1);

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

ird_inode_t* dir_lookup(char* path, uint64_t path_len, ird_inode_t* root) {
  if (root == NULL) { return NULL; }

  uint64_t path_start = 0;
  ird_inode_t* ret = root;

  if (path_len >= 2 && path[0] == '.' && path[1] == '/') { path_start = 2; }

  while (path_start < path_len) {
    while (path_start < path_len && path[path_start] == '/') { path_start++; }
    if (path_start >= path_len) { return ret; }

    uint64_t path_end = path_start;
    while (path_end < path_len && path[path_end] != '/') { path_end++; }

    uint64_t name_len = path_end - path_start;
    ird_inode_t* next =
        find_child(ret->first_child, path + path_start, name_len);
    if (next == NULL || next->type != VFS_NODE_DIR) { return NULL; }

    ret = next;
    path_start = path_end + 1;
  }

  return ret;
}

ird_inode_t* find_child(ird_inode_t* first_child,
                        char* name,
                        uint64_t name_len) {
  for (ird_inode_t* n = first_child; n != NULL; n = n->next_sibling) {
    if (n->name_size == name_len && memcmp(name, n->name, name_len) == 0) {
      return n;
    }
  }
  return NULL;
}

/*
 * Ensures a directory path (prefix) exists from the given `root`.
 * Returns the inode to the last directory entry in the path.
 */
ird_inode_t* get_or_create_prefix(char* path,
                                  uint64_t path_len,
                                  ird_inode_t* root) {
  uint64_t path_start = 0;

  ird_inode_t* ret = dir_lookup(path, path_len, root);
  if (ret != NULL) { return ret; }

  ret = root;
  if (path_len >= 2 && path[0] == '.' && path[1] == '/') { path_start = 2; }

  while (path_start < path_len) {
    while (path_start < path_len && path[path_start] == '/') { path_start++; }
    if (path_start >= path_len) { break; }

    uint64_t path_end = path_start;
    while (path_end < path_len && path[path_end] != '/') { path_end++; }

    uint64_t name_len = path_end - path_start;
    ird_inode_t* next =
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

bool is_zero_block(void* block) {
  unsigned char* bytes = (unsigned char*)block;

  for (uint64_t i = 0; i < HOJICHA_USTAR_HEADER_LEN_BYTES; ++i) {
    if (bytes[i] != 0) { return false; }
  }

  return true;
}
