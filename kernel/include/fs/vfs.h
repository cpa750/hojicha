#ifndef HOJICHA_VFS_H
#define HOJICHA_VFS_H

#include <stdint.h>

#define MAX_FDS 256

typedef enum {
  VFS_STATUS_OK = 0,
  VFS_STATUS_NOENT,
  VFS_STATUS_NOTDIR,
  VFS_STATUS_ISDIR,
  VFS_STATUS_NOMEM,
  VFS_STATUS_EOF,
  VFS_STATUS_INVALID_ARG,
  VFS_STATUS_TOO_MANY_OPEN,
  VFS_STATUS_BAD_FD,
} vfs_status_t;

typedef enum {
  VFS_NODE_FILE = 1,
  VFS_NODE_DIR,
  VFS_NODE_SYMLINK,
  VFS_NODE_DEVICE,
} vfs_node_type_t;

typedef enum {
  VFS_OPEN_READ = 1,
  VFS_OPEN_DIRECTORY,
} vfs_open_flags_t;

typedef enum { VFS_SEEK_SET = 0, VFS_SEEK_CUR, VFS_SEEK_END } vfs_seek_whence_t;

/*
 * One mounted filesystem instance. Owns the root vnode for that mount and any
 * filesystem-state.
 */
typedef struct vfs_mount vfs_mount_t;

/*
 * Generic filesystem object, may represent either a
 * file or directory. Is *not* a handle to an open file.
 */
typedef struct vfs_node vfs_node_t;

/*
 * One directory entry returned by `vfs_readdir()`.
 */
typedef struct vfs_dirent vfs_dirent_t;

/*
 * Generic open object handle.
 */
typedef struct vfs_file vfs_file_t;

typedef struct vfs_file_ops vfs_file_ops_t;
typedef struct vfs_node_ops vnode_ops_t;

typedef struct vfs_stat vfs_stat_t;

struct vfs_mount {
  vfs_node_t* root;
  void* fs_data;
};

struct vfs_dirent {
  const char* name;
  uint64_t inode_no;
};

struct vfs_file {
  vfs_node_t* vnode;
  const vfs_file_ops_t* ops;
  uint32_t flags;
  uint64_t offset;
  void* fs_data;
};

struct vfs_node {
  const vnode_ops_t* ops;
  vfs_node_type_t type;
  uint32_t refcount;
  void* fs_data;
};

struct vfs_file_ops {
  vfs_status_t (*read)(vfs_file_t* file,
                       void* buffer,
                       uint64_t len,
                       uint64_t* bytes_read_out);
  vfs_status_t (*write)(vfs_file_t* file,
                        void* buffer,
                        uint64_t len,
                        uint64_t* bytes_written_out);
  vfs_status_t (*readdir)(vfs_file_t* dir, vfs_dirent_t** out);
  vfs_status_t (*seek)(vfs_file_t* file,
                       int64_t offset,
                       vfs_seek_whence_t whence,
                       uint64_t* new_pos);
  vfs_status_t (*close)(vfs_file_t* file);
};

struct vfs_node_ops {
  vfs_status_t (*lookup)(vfs_node_t* dir,
                         const char* name,
                         uint32_t name_len,
                         vfs_node_t** out);
  vfs_status_t (*open)(vfs_node_t* vnode, uint32_t flags, vfs_file_t** out);
  void (*release)(vfs_node_t* vnode);
  vfs_status_t (*stat)(vfs_node_t* vnode, vfs_stat_t** out);
};

struct vfs_stat {
  vfs_node_type_t type;
  uint64_t size;
};

/*
 * Mounts the root filesystem at `/`.
 */
vfs_status_t vfs_mount_root(vfs_mount_t* mount);

/*
 * Resolves an absolute path to a vnode.
 */
vfs_status_t vfs_lookup(const char* absolute_path, vfs_node_t** out);

/*
 * Opens a regular file or directory at `absolute_path`. `flags` are currently
 * ignored but included in the API with intent for further expansion.
 */
vfs_status_t vfs_open(const char* absolute_path,
                      uint32_t flags,
                      vfs_file_t** out);

/*
 * Reads up to `len` bytes from a file handle, advancing its current offset.
 */
vfs_status_t vfs_read(vfs_file_t* file,
                      void* buffer,
                      uint64_t len,
                      uint64_t* out_read);

/*
 * Writes up to `len` bytes from `buffer` into `file`.
 */
vfs_status_t vfs_write(vfs_file_t* file,
                       void* buffer,
                       uint64_t len,
                       uint64_t* bytes_written_out);

/*
 * Returns the next directory entry from an open directory handle.
 */
vfs_status_t vfs_readdir(vfs_file_t* dir, vfs_dirent_t** out);

/*
 * Seeks to at most the given `offset` from `whence` in the given `file`.
 */
vfs_status_t vfs_seek(vfs_file_t* file,
                      uint64_t offset,
                      vfs_seek_whence_t whence,
                      uint64_t* new_pos);

/*
 * Returns metadata for a file without opening it.
 */
vfs_status_t vfs_stat(const char* absolute_path, vfs_stat_t** out);

/*
 * Returns metadata for an already-open file.
 */
vfs_status_t vfs_fstat(vfs_file_t* file, vfs_stat_t** out);

/*
 * Closes an open handle.
 */
vfs_status_t vfs_close(vfs_file_t* file);

/*
 * Gets the file object associated with the given `fd` in the context of
 * the current running process.
 * Returns `VFS_STATUS_OK` and the file object in `out` on success,
 * on failure returns the relevant error code and does not modify `out`.
 */
vfs_status_t vfs_resolve_fd(uint64_t fd, vfs_file_t** out);

#endif  // HOJICHA_VFS_H
