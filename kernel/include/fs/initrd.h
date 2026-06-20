#ifndef HOJICHA_INITRD_H
#define HOJICHA_INITRD_H

#include <fs/vfs.h>

uint8_t initrd_initalize();

vfs_status_t initrd_lookup(vfs_node_t* dir,
                           const char* name,
                           uint32_t name_len,
                           vfs_node_t** out);
vfs_status_t initrd_parent(vfs_node_t* dir, vfs_node_t** out);

vfs_status_t initrd_open(vfs_node_t* vnode, uint32_t flags, vfs_file_t** out);
vfs_status_t initrd_close(vfs_file_t* vfile);

vfs_status_t initrd_read(vfs_file_t* vfile,
                         void* buffer,
                         uint64_t len,
                         uint64_t* bytes_read_out);
vfs_status_t initrd_write(vfs_file_t* file,
                          void* buffer,
                          uint64_t len,
                          uint64_t* bytes_written_out);
vfs_status_t initrd_readdir(vfs_file_t* vdir, vfs_dirent_t** out);
vfs_status_t initrd_seek(vfs_file_t* vfile,
                         int64_t offset,
                         vfs_seek_whence_t whence,
                         uint64_t* new_pos);

vfs_status_t initrd_create_file(vfs_node_t* dir,
                                const char* name,
                                uint32_t name_len,
                                vfs_node_t** out);
vfs_status_t initrd_create_dir(vfs_node_t* dir,
                               const char* name,
                               uint32_t name_len,
                               vfs_node_t** out);

vfs_status_t initrd_delete_file(vfs_node_t* dir,
                                const char* name,
                                uint32_t name_len,
                                uint32_t flags);
vfs_status_t initrd_delete_dir(vfs_node_t* dir,
                               const char* name,
                               uint32_t name_len,
                               uint32_t flags);
vfs_status_t initrd_link(vfs_node_t* dir,
                         const char* name,
                         uint32_t name_len,
                         vfs_node_t* target);
vfs_status_t initrd_symlink(vfs_node_t* dir,
                            const char* name,
                            uint32_t name_len,
                            const char* target,
                            uint32_t target_len,
                            vfs_node_t** out);
vfs_status_t initrd_readlink(vfs_node_t* vnode,
                             char* buffer,
                             uint64_t len,
                             uint64_t* bytes_read_out);

vfs_status_t initrd_stat(vfs_node_t* vnode, vfs_stat_t** out);

void initrd_free(vfs_node_t* vnode);

#endif  // HOJICHA_INITRD_H
