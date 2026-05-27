#ifndef HOJICHA_DEVFS_H
#define HOJICHA_DEVFS_H

#include <fs/vfs.h>
#include <stdbool.h>

#define HOJICHA_MAJOR_MAX 256
#define HOJICHA_MINOR_MAX 256

typedef enum {
  DEVFS_CHARDEV = 1,
  DEVFS_BLKDEV = 2,
} devfs_major_t;

typedef struct devfs_device devfs_device_t;

bool devfs_initialize(void);

devfs_device_t* devfs_device_new(vfs_file_ops_t* file_ops,
                                 vfs_node_ops_t* node_ops);
vfs_status_t devfs_register(devfs_major_t major,
                            uint64_t minor,
                            devfs_device_t* dev,
                            const char* name,
                            uint64_t name_len);
vfs_status_t devfs_unregister(devfs_major_t major, uint64_t minor);

vfs_status_t devfs_lookup(vfs_node_t* dir,
                          const char* name,
                          uint32_t name_len,
                          vfs_node_t** out);

vfs_status_t devfs_open(vfs_node_t* vnode, uint32_t flags, vfs_file_t** out);
vfs_status_t devfs_close(vfs_file_t* vfile);

vfs_status_t devfs_read(vfs_file_t* vfile,
                        void* buffer,
                        uint64_t len,
                        uint64_t* bytes_read_out);
vfs_status_t devfs_write(vfs_file_t* file,
                         void* buffer,
                         uint64_t len,
                         uint64_t* bytes_written_out);
vfs_status_t devfs_readdir(vfs_file_t* vdir, vfs_dirent_t** out);
vfs_status_t devfs_seek(vfs_file_t* vfile,
                        int64_t offset,
                        vfs_seek_whence_t whence,
                        uint64_t* new_pos);

vfs_status_t devfs_create_file(vfs_node_t* dir,
                               const char* name,
                               uint32_t name_len,
                               vfs_node_t** out);
vfs_status_t devfs_create_dir(vfs_node_t* dir,
                              const char* name,
                              uint32_t name_len,
                              vfs_node_t** out);

vfs_status_t devfs_delete_file(vfs_node_t* dir,
                               const char* name,
                               uint32_t name_len,
                               uint32_t flags);
vfs_status_t devfs_delete_dir(vfs_node_t* dir,
                              const char* name,
                              uint32_t name_len,
                              uint32_t flags);

vfs_status_t devfs_stat(vfs_node_t* vnode, vfs_stat_t** out);

void devfs_free(vfs_node_t* vnode);

#endif  // HOJICHA_DEVFS_H

