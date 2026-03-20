#ifndef HOJICHA_VFS_H
#define HOJICHA_VFS_H

#include <stdint.h>

typedef enum {
  VFS_STATUS_OK = 0,
  VFS_STATUS_NOTDIR,
  VFS_STATUS_ISDIR,
  VFS_STATUS_NOMEM,
  VFS_STATUS_EOF,
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

/*
 * One mounted filesystem instance. Owns the root vnode for that mount and any
 * filesystem-state.
 */
typedef struct vfs_mount vfs_mount_t;

/*
 * Generic filesystem object, may represent either a
 * file or directory. Is *not* a handle to an open file.
 */
typedef struct vnode vnode_t;

/*
 * One directory entry returned by `vfs_readdir()`.
 */
typedef struct vfs_dirent vfs_dirent_t;

/*
 * Generic open object handle.
 */
typedef struct vfile vfile_t;

typedef struct vfile_ops vfile_ops_t;
typedef struct vnode_ops vnode_ops_t;

struct vfs_mount {
  vnode_t* root;
  void* fs_data;
};

struct vnode {
  vfs_mount_t* mount;
  const vnode_ops_t* ops;
  vfs_node_type_t type;
  uint32_t refcount;
  void* fs_data;
};

struct vfs_dirent {
  const char* name;
  uint32_t name_len;
  vfs_node_type_t type;
};

struct vfile {
  vnode_t* vnode;
  const vfile_ops_t* ops;
  uint32_t flags;
  uint64_t offset;
  void* fs_data;
};

struct vfile_ops {
  vfs_status_t (*read)(vfile_t* file,
                       void* buffer,
                       uint64_t len,
                       uint64_t* out_read);
  vfs_status_t (*readdir)(vfile_t* dir, vfs_dirent_t* out);
  void (*close)(vfile_t* file);
};

struct vnode_ops {
  vfs_status_t (*lookup)(vnode_t* dir,
                         const char* name,
                         uint32_t name_len,
                         vnode_t** out);
  vfs_status_t (*open)(vnode_t* vnode, uint32_t flags, vfile_t** out);
  void (*release)(vnode_t* vnode);
};

/*
 * Mounts the root filesystem at `/`.
 */
vfs_status_t vfs_mount_root(vfs_mount_t* mount);

/*
 * Resolves an absolute path to a vnode.
 */
vfs_status_t vfs_lookup(const char* absolute_path, vnode_t** out);

/*
 * Opens a regular file or directory at `absolute_path`. `flags` are currently
 * ignored but included in the API with intent for further expansion.
 */
vfs_status_t vfs_open(const char* absolute_path, uint32_t flags, vfile_t** out);

/*
 * Convenience wrapper that opens a directory handle directly.
 */
vfs_status_t vfs_opendir(const char* absolute_path, vfile_t** out_dir);

/*
 * Reads up to `len` bytes from a file handle, advancing its current offset.
 */
vfs_status_t vfs_read(vfile_t* file,
                      void* buffer,
                      uint64_t len,
                      uint64_t* out_read);

/*
 * Returns the next directory entry from an open directory handle.
 */
vfs_status_t vfs_readdir(vfile_t* dir, vfs_dirent_t* out);

/*
 * Closes an open handle.
 */
vfs_status_t vfs_close(vfile_t* file);

#endif  // HOJICHA_VFS_H

