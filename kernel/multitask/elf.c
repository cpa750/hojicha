#include <haddr.h>
#include <hlog.h>
#include <kernel/g_kernel.h>
#include <memory/pmm.h>
#include <memory/vmm.h>
#include <multitask/elf.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

#define USER_SPACE_MIN_VADDR 0x0000000000100000ULL
#define USER_SPACE_MAX_VADDR 0x00007FFFFFFFF000ULL

static char* read_error_msg_prefix = "Could not read ELF file:";
static char* load_error_msg_prefix = "Could not load ELF file:";
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
  uint64_t align;
} __attribute__((packed));
typedef struct elf_program_header elf_program_header_t;

struct elf {
  elf_header_t* header;
  elf_program_header_t* program_headers;
  void* buffer;
  haddr_t load_addr;
};
typedef struct elf elf_t;

extern void enter_ring3(haddr_t rsp, haddr_t rip);

bool is_valid_header(elf_header_t* header);

elf_t* elf_read(void* buffer, uint64_t size) {
  if (size < sizeof(elf_header_t)) {
    hlog_write(HLOG_ERROR,
               "%s provided buffer size is not large "
               "enough to contain ELF header.",
               read_error_msg_prefix);
  }
  elf_header_t* elf_header = (elf_header_t*)malloc(sizeof(elf_header_t));
  elf_header = memcpy(elf_header, buffer, sizeof(elf_header_t));

  if (!is_valid_header(elf_header)) { return NULL; }

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
  ret->buffer = buffer;
  return ret;
}

bool elf_map(elf_t* elf, vmm_t* vmm) {
  if (vmm == NULL) {
    hlog_write(
        HLOG_ERROR, "%s process does not have a VMM.", load_error_msg_prefix);
    return false;
  }

  if (vmm == g_kernel.vmm) {
    hlog_write(HLOG_WARN,
               "Loading ELF using kernel VMM. Are you sure you're using "
               "the right process handle?");
  }

  elf_program_header_t prog_header;
  haddr_t lowest_load = INT64_MAX;
  for (int i = 0; i < elf->header->count_prog_header_table_entry; ++i) {
    prog_header = elf->program_headers[i];
    if (prog_header.type == ELF_PROG_HEADER_TYPE_LOAD) {
      if (prog_header.load_vaddr < lowest_load) {
        lowest_load = prog_header.load_vaddr;
      }
      size_t page_count = (prog_header.mapped_size + PAGE_SIZE - 1) / PAGE_SIZE;
      vmm_map(vmm,
              prog_header.load_vaddr,
              page_count,
              PAGE_USER_ACCESIBLE | PAGE_PRESENT | PAGE_WRITABLE);
    } else if (prog_header.type == ELF_PROG_HEADER_TYPE_DYNAMIC) {
      hlog_write(HLOG_ERROR,
                 "%s encountered dynamic program header. Currently only static "
                 "binaries are supported.");
      return false;
    }
  }
  return true;
}

void elf_launch(elf_t* elf, vmm_t* vmm) {
  elf_map(elf, vmm);
  haddr_t highest_loaded_addr = USER_SPACE_MIN_VADDR;
  for (uint16_t i = 0; i < elf->header->count_prog_header_table_entry; ++i) {
    elf_program_header_t prog_header = elf->program_headers[i];
    if (prog_header.type == ELF_PROG_HEADER_TYPE_LOAD) {
      haddr_t segment_end = prog_header.load_vaddr + prog_header.mapped_size;
      if (segment_end > highest_loaded_addr) {
        highest_loaded_addr = segment_end;
      }
      memcpy((void*)prog_header.load_vaddr,
             elf->buffer + prog_header.buffer_offset,
             prog_header.buffer_size);
      if (prog_header.mapped_size > prog_header.buffer_size) {
        memset((void*)(prog_header.load_vaddr + prog_header.buffer_size),
               0,
               prog_header.mapped_size - prog_header.buffer_size);
      }
    }
  }

  uint64_t stack_size = (STACK_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
  haddr_t user_stack_top = USER_SPACE_MAX_VADDR;
  haddr_t user_stack_location =
      user_stack_top - pmm_page_to_addr_base(stack_size);

  if (user_stack_location <= highest_loaded_addr + PAGE_SIZE) {
    hlog_write(HLOG_ERROR,
               "%s no room for user stack in lower-half userspace range.",
               load_error_msg_prefix);
    abort();
  }

  vmm_map(vmm,
          user_stack_location,
          stack_size,
          PAGE_USER_ACCESIBLE | PAGE_PRESENT | PAGE_WRITABLE);

  user_stack_top = user_stack_location + pmm_page_to_addr_base(stack_size);
  enter_ring3(user_stack_top, elf->header->offset_entry);
  return;
}

void elf_free(elf_t* elf) {
  // TODO: what else is needed here?
  free(elf->program_headers);
  free(elf->header);
  free(elf);
  return;
}

bool is_valid_header(elf_header_t* header) {
  if (header->magic != ELF_MAGIC) {
    hlog_write(HLOG_ERROR, "%s no magic.", read_error_msg_prefix);
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
