#include <dirent.h>
#include <errno.h>
#include <fs/vfs.h>
#include <multitask/syscall_callbacks.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static unsigned short linux_dirent_record_len(uint64_t name_len) {
  uint64_t len = offsetof(linux_dirent_t, d_name) + name_len + 1;
  uint64_t align = sizeof(unsigned long);
  len = (len + align - 1) & ~(align - 1);
  if (len > UINT16_MAX) { return 0; }
  return (unsigned short)len;
}

static void free_vfs_dirent(vfs_dirent_t* dirent) {
  if (dirent == NULL) { return; }
  free((void*)dirent->name);
  free(dirent);
}

long syscall_getdents(unsigned long fd,
                      linux_dirent_t* dirent_buf,
                      unsigned int count) {
  if (dirent_buf == NULL) { return -EINVAL; }

  vfs_file_t* file = NULL;
  vfs_status_t status = vfs_resolve_fd(fd, &file);
  if (status != VFS_STATUS_OK) { return -vfs_status_to_errno(status); }

  uint64_t written = 0;
  while (written < count) {
    vfs_dirent_t* vfs_dirent = NULL;
    status = vfs_readdir(file, &vfs_dirent);
    if (status != VFS_STATUS_OK) {
      return written > 0 ? (long)written : -vfs_status_to_errno(status);
    }
    if (vfs_dirent == NULL) { break; }

    uint64_t name_len = strlen(vfs_dirent->name);
    if (name_len > 0 && vfs_dirent->name[name_len - 1] == '/') { name_len--; }

    unsigned short record_len = linux_dirent_record_len(name_len);
    if (record_len == 0 || written + record_len > count) {
      vfs_seek(file, -1, VFS_SEEK_CUR, NULL);
      free_vfs_dirent(vfs_dirent);
      return written > 0 ? (long)written : -EINVAL;
    }

    linux_dirent_t* out = (linux_dirent_t*)((char*)dirent_buf + written);
    memset(out, 0, record_len);
    out->d_ino = vfs_dirent->inode_no;
    out->d_off = file->offset;
    out->d_reclen = record_len;
    memcpy(out->d_name, vfs_dirent->name, name_len);
    out->d_name[name_len] = '\0';

    written += record_len;
    free_vfs_dirent(vfs_dirent);
  }

  return written;
}
