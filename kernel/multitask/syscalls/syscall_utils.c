#include <errno.h>
#include <fs/vfs.h>
#include <multitask/syscall_utils.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

bool syscall_is_uaddr(const void* ptr, size_t len) {
  uintptr_t start = (uintptr_t)ptr;
  if (start >= SYSCALL_HIGHER_HALF_START) { return false; }
  if (len == 0) { return true; }

  uintptr_t end = start + len - 1;
  if (end < start) { return false; }
  return end < SYSCALL_HIGHER_HALF_START;
}

void* syscall_utok_memcpy(const void* src, size_t len) {
  if (src == NULL && len > 0) { return NULL; }
  if (!syscall_is_uaddr(src, len)) { return NULL; }

  size_t alloc_len = len == 0 ? 1 : len;
  void* dst = calloc(1, alloc_len);
  if (dst == NULL) { return NULL; }

  if (len > 0) { memcpy(dst, src, len); }
  return dst;
}

char* syscall_utok_strcpy(const char* src, size_t max_len) {
  if (src == NULL || max_len == 0) { return NULL; }
  if (!syscall_is_uaddr(src, max_len)) { return NULL; }

  size_t len = 0;
  while (len < max_len && src[len] != '\0') { len++; }

  size_t alloc_len = len < max_len ? len + 1 : max_len;
  size_t copy_len = len < max_len ? len : max_len - 1;
  char* dst = calloc(1, alloc_len);
  if (dst == NULL) { return NULL; }

  if (copy_len > 0) { memcpy(dst, src, copy_len); }
  return dst;
}

long syscall_lookup_parent_for_child(const char* path,
                                     vfs_node_t** parent_out,
                                     const char** name_out,
                                     uint32_t* name_len_out) {
  if (path == NULL || parent_out == NULL || name_out == NULL ||
      name_len_out == NULL) {
    return -EINVAL;
  }

  vfs_status_t status =
      vfs_lookup_parent(path, parent_out, name_out, name_len_out);
  if (status != VFS_STATUS_OK) { return -vfs_status_to_errno(status); }
  if (*name_out == NULL || *name_len_out == 0) {
    vfs_vnode_release(*parent_out);
    *parent_out = NULL;
    return -EINVAL;
  }

  return 0;
}
