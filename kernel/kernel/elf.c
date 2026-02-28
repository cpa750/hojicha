#include <kernel/elf.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "haddr.h"
#include "hlog.h"

#define ELF_MAGIC 0x464C457F

#define ELF_HEADER_BITS_32         1
#define ELF_HEADER_BITS_64         2
#define ELF_HEADER_ENDIAN_LITTLE   1
#define ELF_HEADER_ENDIAN_BIG      2
#define ELF_HEADER_ABI_SYSV        0
#define ELF_HEADER_ISA_X86         0x03
#define ELF_HEADER_ISA_X86_64      0x3E
#define ELF_HEADER_TYPE_EXECUTABLE 2

#define ELF_PROG_HEADER_TYPE_NULL        0
#define ELF_PROG_HEADER_TYPE_LOAD        1
#define ELF_PROG_HEADER_TYPE_DYNAMIC     2
#define ELF_PROG_HEADER_TYPE_INTERPRETER 3
#define ELF_PROG_HEADER_TYPE_NOTE        4

#define ELF_PROG_HEADER_FLAG_EXECUTABLE 1
#define ELF_PROG_HEADER_FLAG_WRITABLE   2
#define ELF_PROG_HEADER_FLAG_READABLE   4

static char* error_msg_prefix = "Could not read ELF file:";

struct elf_header {
  uint32_t magic;
  uint8_t bits;
  uint8_t endianness;
  uint8_t version_header;
  uint8_t abi;
  uint64_t reserved;
  uint16_t type;
  uint16_t isa;
  uint32_t version_elf;
  uint64_t offset_entry;
  uint64_t offset_prog_header_table;
  uint64_t offset_sect_header_table;
  uint32_t flags;
  uint16_t size_header;
  uint16_t size_prog_header_table_entry;
  uint16_t count_prog_header_table_entry;
  uint16_t size_sect_header_table_entry;
  uint16_t count_sect_header_table_entry;
  uint16_t idx_sect_header_str_table;
} __attribute__((packed));
typedef struct elf_header elf_header_t;

struct elf_program_header {
  uint32_t type;
  uint32_t flags;
  uint64_t buffer_offset;
  uint64_t load_vaddr;
  uint64_t reserved_paddr;
  uint64_t buffer_size;
  uint64_t mapped_size;
} __attribute__((packed));
typedef struct elf_program_header elf_program_header_t;

bool is_valid_exec(elf_header_t* header);

elf_t* elf_read(void* buffer, uint64_t size) {
  if (size < sizeof(elf_header_t)) {
    hlog_write(HLOG_ERROR,
               "%s provided buffer size is not large "
               "enough to contain ELF header.",
               error_msg_prefix);
  }
  elf_header_t* elf_header = (elf_header_t*)malloc(sizeof(elf_header_t));
  elf_header = memcpy(elf_header, buffer, sizeof(elf_header_t));

  if (!is_valid_exec(elf_header)) { return NULL; }

  elf_program_header_t* program_headers = (elf_program_header_t*)malloc(
      sizeof(elf_program_header_t) * elf_header->count_prog_header_table_entry);
  void* prog_header_offset_buf_start =
      (void*)((haddr_t)buffer + elf_header->offset_prog_header_table);
  memcpy(
      program_headers,
      prog_header_offset_buf_start,
      sizeof(elf_program_header_t) * elf_header->count_prog_header_table_entry);

  elf_t* ret = (elf_t*)malloc(sizeof(elf_t));
  ret->header = elf_header;
  ret->program_headers = program_headers;
  return ret;
}

bool elf_load(elf_t* elf) {
  elf;
  return false;
}

void elf_free(elf_t* elf) {
  elf;
  return;
}

bool is_valid_exec(elf_header_t* header) {
  if (header->magic != ELF_MAGIC) {
    hlog_write(HLOG_ERROR, "%s no magic.", error_msg_prefix);
    return false;
  }

  if (header->type != ELF_HEADER_TYPE_EXECUTABLE) {
    hlog_write(HLOG_ERROR, "%s file is not executable.");
    return false;
  }

  if (header->abi != ELF_HEADER_ABI_SYSV) {
    hlog_write(HLOG_ERROR, "%s executable does not use the System V ABI.");
    return false;
  }

  if (header->bits != ELF_HEADER_BITS_64) {
    hlog_write(HLOG_ERROR, "%s executable is not 64 bits.");
    return false;
  }

  if (header->isa != ELF_HEADER_ISA_X86_64) {
    hlog_write(HLOG_ERROR, "%s executable is not built for x86-64.");
    return false;
  }

  return true;
}

