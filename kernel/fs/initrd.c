#include <fs/initrd.h>
#include <fs/ustar.h>
#include <fs/vfs.h>
#include <stdlib.h>
#include <string.h>

void add_child(ird_inode_t* parent, ird_inode_t* new_child);
ird_inode_t* create_root();
ird_inode_t* get_or_create_prefix(char* path,
                                  uint64_t path_len,
                                  ird_inode_t* root);
uint64_t get_name_start_idx(char* filename, uint64_t len);

vfs_status_t ird_from_ustar(void* buffer, uint64_t size) {
  ird_inode_t* root = create_root();

  uint64_t buf_pos = 0;
  while (buf_pos < size) {
    ustar_header_t* h = (ustar_header_t*)(buffer + buf_pos);
    if (!strcmp(h->filename, "./")) {
      buf_pos +=
          ((((ustar_oct2dec(h->filesize_bytes_oct, 12) + 511) / 512)) + 1) *
          512;
      continue;
    }

    if (h->type == '0' || h->type == '\0') {
      uint64_t name_offset =
          get_name_start_idx(h->filename, strlen(h->filename));

      ird_inode_t* new = (ird_inode_t*)malloc(sizeof(ird_inode_t));
      new->size = ustar_oct2dec(h->filesize_bytes_oct, 12);
      new->name = h->filename + name_offset;  // Skip leading './'
      new->name_size = strlen(new->name);
      new->buf = buffer + 512;
      new->type = VFS_NODE_FILE;

      ird_inode_t* parent;
      if (name_offset == 2) {  // Skip the leading `./`
        parent = root;
      } else {
        parent = get_or_create_prefix(h->filename, name_offset, root);
      }
      new->parent = parent;
      add_child(parent, new);
      buf_pos += ((((new->size + 511) / 512)) + 1) * 512;
    } else {
      buf_pos +=
          ((((ustar_oct2dec(h->filesize_bytes_oct, 12) + 511) / 512)) + 1) *
          512;
    }
  }
  return VFS_STATUS_OK;
}

ird_inode_t* create_root() {
  ird_inode_t* root = (ird_inode_t*)malloc(sizeof(ird_inode_t));
  root->name = "";
  root->name_size = 0;
  root->type = VFS_NODE_DIR;
  root->parent = NULL;
  root->first_child = NULL;
  root->next_sibling = NULL;
  return root;
}

void add_child(ird_inode_t* parent, ird_inode_t* new_child) {
  ird_inode_t* target = parent->first_child;
  while (target->next_sibling != NULL) { target = target->next_sibling; }
  target->next_sibling = new_child;
  new_child->next_sibling = NULL;
}

// TODO
ird_inode_t* dir_lookup(char* path, uint64_t path_len, ird_inode_t* root) {}

/*
 * Ensures a directory path (prefix) exists from the given `root`.
 * Returns the inode to the last directory entry in the path.
 */
ird_inode_t* get_or_create_prefix(char* path,
                                  uint64_t path_len,
                                  ird_inode_t* root) {
  uint64_t path_start = 0;
  uint64_t path_end = 0;

  ird_inode_t* ret = dir_lookup(path, path_len, root);
  if (ret != NULL) { return ret; }

  ret = root;
  while (path_start < path_len - 1) {
    // Skip the relative path '.' that tar includes in the filename
    if (path[path_start] == '.') {
      path_start += 2;
      path_end = path_start;
      continue;
    }

    while (path[path_end] != '/' && path_end < path_len) { path_end++; }
    if (path[path_end] == '/') {
      ird_inode_t* new = (ird_inode_t*)malloc(sizeof(ird_inode_t));

      new->parent = ret;
      add_child(ret, new);

      char* name = (char*)malloc(path_end - path_start + 1);
      name[path_end - path_start] = '\0';
      memcpy(name, path + path_start, path_end - path_end);
      new->name = name;
      new->name_size = path_end - path_start + 1;

      new->type = VFS_NODE_DIR;

      path_start = path_end + 2;
      path_end = path_start;
      ret = new;
    }
  }
  return ret;
}

uint64_t get_name_start_idx(char* filename, uint64_t len) {
  --len;
  while (filename[len] != '/') { --len; }
  return len + 1;
}

