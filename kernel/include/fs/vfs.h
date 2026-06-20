#ifndef HOJICHA_VFS_H
#define HOJICHA_VFS_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#define MAX_FDS 256

typedef enum {
  VFS_STATUS_OK = 0,
  VFS_STATUS_NOENT,
  VFS_STATUS_NOTDIR,
  VFS_STATUS_ISDIR,
  VFS_STATUS_NOMEM,
  VFS_STATUS_INVALID_ARG,
  VFS_STATUS_TOO_MANY_OPEN,
  VFS_STATUS_BAD_FD,
  VFS_STATUS_NOT_IMPLEMENTED,
  VFS_STATUS_NOTEMPTY,
  VFS_STATUS_EXISTS,
  VFS_STATUS_XDEV,
  VFS_STATUS_LOOP,
} vfs_status_t;

typedef enum {
  VFS_NODE_FILE = 1,
  VFS_NODE_DIR,
  VFS_NODE_SYMLINK,
  VFS_NODE_DEVICE,
} vfs_node_type_t;

typedef enum {
  VFS_OPEN_READ = 1,
  VFS_OPEN_WRITE = 2,
  VFS_OPEN_DIRECTORY = 4,
  VFS_OPEN_CREATE = 8,
  VFS_OPEN_CLOEXEC = 16,
} vfs_open_flags_t;

typedef enum {
  VFS_SEEK_SET = SEEK_SET,
  VFS_SEEK_CUR = SEEK_CUR,
  VFS_SEEK_END = SEEK_END,
} vfs_seek_whence_t;

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
typedef struct vfs_node_ops vfs_node_ops_t;

typedef struct vfs_stat vfs_stat_t;

struct vfs_mount {
  vfs_node_t* point;
  vfs_node_t* root;
  vfs_mount_t* parent;
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
  uint32_t refcount;
  uint64_t offset;
  void* fs_data;
};

struct vfs_node {
  const vfs_node_ops_t* ops;
  vfs_node_type_t type;
  uint32_t refcount;
  uint32_t link_count;
  int64_t accessed_timestamp;
  int64_t modified_timestamp;
  int64_t changed_mdt_timestamp;
  /*
   * Mount boundary metadata. On a covered mountpoint, this points to the child
   * mount mounted over it. On a mount root, this points to the mount that owns
   * the root. Other vnodes leave it NULL.
   */
  vfs_mount_t* mount;
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
  vfs_status_t (*ioctl)(vfs_file_t* file, uint64_t number, void* args);
};

struct vfs_node_ops {
  /*
   * Vnode-returning ops return borrowed references on success. The caller owns
   * that reference and must release it with `vfs_vnode_release()`.
   */
  vfs_status_t (*lookup)(vfs_node_t* dir,
                         const char* name,
                         uint32_t name_len,
                         vfs_node_t** out);
  vfs_status_t (*parent)(vfs_node_t* dir, vfs_node_t** out);
  vfs_status_t (*open)(vfs_node_t* vnode, uint32_t flags, vfs_file_t** out);
  vfs_status_t (*create_file)(vfs_node_t* dir,
                              const char* name,
                              uint32_t name_len,
                              vfs_node_t** out);
  vfs_status_t (*create_dir)(vfs_node_t* dir,
                             const char* name,
                             uint32_t name_len,
                             vfs_node_t** out);
  vfs_status_t (*unlink)(vfs_node_t* dir,
                         const char* name,
                         uint32_t name_len,
                         uint32_t flags);
  vfs_status_t (*rmdir)(vfs_node_t* dir,
                        const char* name,
                        uint32_t name_len,
                        uint32_t flags);
  vfs_status_t (*link)(vfs_node_t* dir,
                       const char* name,
                       uint32_t name_len,
                       vfs_node_t* target);
  vfs_status_t (*symlink)(vfs_node_t* dir,
                          const char* name,
                          uint32_t name_len,
                          const char* target,
                          uint32_t target_len,
                          vfs_node_t** out);
  vfs_status_t (*readlink)(vfs_node_t* vnode,
                           char* buffer,
                           uint64_t len,
                           uint64_t* bytes_read_out);
  void (*free)(vfs_node_t* vnode);
  vfs_status_t (*stat)(vfs_node_t* vnode, vfs_stat_t** out);
};

struct vfs_stat {
  vfs_node_type_t type;
  uint64_t size;
  uint32_t link_count;
  int64_t accessed_timestamp;
  int64_t modified_timestamp;
  int64_t changed_mdt_timestamp;
};

/*
 * Mounts the root filesystem at `/`.
 */
vfs_status_t vfs_mount_root(vfs_mount_t* mount);

/*
 * Mounts a filesystem on an existing directory vnode.
 */
vfs_status_t vfs_mount(vfs_node_t* mountpoint,
                       vfs_mount_t* mount,
                       vfs_mount_t* parent);

/*
 * Unmounts a previously mounted filesystem. Root unmount is not supported.
 */
vfs_status_t vfs_unmount(vfs_mount_t* mount);

/*
 * Resolves a path to a vnode. Absolute paths start at root; relative paths
 * start at the current process cwd, or root if there is no cwd.
 */
vfs_status_t vfs_lookup(const char* path, vfs_node_t** out);

/*
 * Resolves `path` from `base` when relative, or from the current process cwd
 * if `base` is NULL. Absolute paths always start at root.
 */
vfs_status_t vfs_lookup_at(vfs_node_t* base,
                           const char* path,
                           vfs_node_t** out);

/*
 * Resolves the parent directory of a path and returns the final path component
 * as a view into `path`.
 */
vfs_status_t vfs_lookup_parent(const char* path,
                               vfs_node_t** parent_out,
                               const char** name_out,
                               uint32_t* name_len_out);

/*
 * Resolves the parent directory of `path` from `base` when relative, or from
 * the current process cwd if `base` is NULL. Absolute paths always start at
 * root. The final component is returned as a view into `path`.
 */
vfs_status_t vfs_lookup_parent_at(vfs_node_t* base,
                                  const char* path,
                                  vfs_node_t** parent_out,
                                  const char** name_out,
                                  uint32_t* name_len_out);

/*
 * Opens a regular file or directory at `path`. `out_fd` is optional and
 * receives the process fd assigned to the opened handle on success.
 */
vfs_status_t vfs_open(const char* path,
                      uint32_t flags,
                      vfs_file_t** out,
                      uint64_t* out_fd);

/*
 * Opens a regular file or directory at `path` and returns a file handle.
 */
vfs_status_t vfs_get_file_handle(const char* path,
                                 uint32_t flags,
                                 vfs_file_t** out);

/*
 * Creates a file in a given `dir`.
 */
vfs_status_t vfs_create(vfs_node_t* dir,
                        const char* name,
                        uint32_t name_len,
                        vfs_node_t** out);

/*
 * Creates a subdirectory in the given `dir`.
 */
vfs_status_t vfs_mkdir(vfs_node_t* dir,
                       const char* name,
                       uint32_t name_len,
                       vfs_node_t** out);

/*
 * Unlinks a file given by `name` in `dir`.
 */
vfs_status_t vfs_unlink(vfs_node_t* dir,
                        const char* name,
                        uint32_t name_len,
                        uint32_t flags);

/*
 * Removes a directory given by `name` from the parent `dir`.
 */
vfs_status_t vfs_rmdir(vfs_node_t* dir,
                       const char* name,
                       uint32_t name_len,
                       uint32_t flags);

/*
 * Creates a hard link at `new_path` to an existing non-directory file at
 * `old_path`. Hard links cannot cross mounted filesystems.
 */
vfs_status_t vfs_link(const char* old_path, const char* new_path);

/*
 * Creates a symbolic link at `link_path` whose stored target is `target`.
 */
vfs_status_t vfs_symlink(const char* target, const char* link_path);

/*
 * Reads the target stored in a symbolic link. The target is copied into
 * `buffer` without adding a terminating NUL byte.
 */
vfs_status_t vfs_readlink(const char* path,
                          char* buffer,
                          uint64_t len,
                          uint64_t* bytes_read_out);

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
                      int64_t offset,
                      vfs_seek_whence_t whence,
                      uint64_t* new_pos);

/*
 * Returns metadata for a file without opening it.
 */
vfs_status_t vfs_stat(const char* path, vfs_stat_t** out);

/*
 * Returns metadata for an already-open file.
 */
vfs_status_t vfs_fstat(vfs_file_t* file, vfs_stat_t** out);

/*
 * Performs a device-specific control operation on an open file handle.
 */
vfs_status_t vfs_ioctl(vfs_file_t* file, uint64_t number, void* args);

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

void vfs_file_borrow(vfs_file_t* file);
void vfs_file_release(vfs_file_t* file);
void vfs_vnode_borrow(vfs_node_t* vnode);
void vfs_vnode_release(vfs_node_t* vnode);
bool vfs_validate_name(const char* name, uint64_t name_len);
char* vfs_clone_name(const char* name, uint64_t name_len, bool trailing_slash);
int vfs_status_to_errno(vfs_status_t status);

#endif  // HOJICHA_VFS_H
