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

static inline uint64_t ustar_oct2dec(char* str, int size) {
  uint64_t n = 0;

  while (size > 0 && str[size - 1] == '\0') { size--; }

  for (int i = 0; i < size; ++i) {
    n *= 8;
    n += str[i] - '0';
  }

  return n;
}

#endif  // HOJICHA_USTAR_H
