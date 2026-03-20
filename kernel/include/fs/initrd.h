#ifndef HOJICHA_INITRD_H
#define HOJICHA_INITRD_H

#include <fs/vfs.h>

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
  // a vector would be better here.
  ird_inode_t* next_sibling;
};

/*
 * Create an initrd VFS mount from a USTAR image.
 */
vfs_status_t ird_from_ustar(void* buffer, uint64_t size);

#endif  // HOJICHA_INITRD_H

