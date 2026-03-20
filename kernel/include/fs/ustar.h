#ifndef HOJICHA_USTAR_H
#define HOJICHA_USTAR_H

#include <fs/initrd.h>
#include <stdint.h>

#define HOJICHA_USTAR_HEADER_LEN_BYTES 512

struct ustar_header {
  char filename[100];
  char mode[8];
  char owner_user_id[8];
  char group_user_id[8];
  char filesize_bytes_oct[12];
  char lmut_oct[12];
  char checksum[8];
  char type;
  char linked_filename[100];
  char ustar_magic[6];
  char ustar_version[2];
  char owner_username[32];
  char owner_groupname[32];
  char device_major[8];
  char device_minor[8];
  char filename_prefix[155];
  char padding[12];
} __attribute__((packed));
typedef struct ustar_header ustar_header_t;

struct ustar_file {
  void* buf;
  uint64_t size;
  uint64_t pos;
};
typedef struct ustar_file ustar_file_t;

/*
 * Parses a raw ustar blob from `buf` into a newly-allocated ramfs.
 */
uint64_t ustar_read_next_to_initrd_inode(ustar_file_t* u, ird_inode_t* target);

static inline uint64_t ustar_oct2dec(char* str, int size) {
  // Thank you OSDev Wiki for the octal to decimal helper function
  int n = 0;
  char* orig_c = str;
  char c = *orig_c;
  while (size-- > 0) {
    n *= 8;
    if (*orig_c == '\000') {
      c = '0';
    } else {
      c = *orig_c;
    }
    n += c - '0';
    orig_c++;
  }
  return n;
}

#endif  // HOJICHA_USTAR_H

