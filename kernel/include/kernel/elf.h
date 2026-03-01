#ifndef ELF_H
#define ELF_H

#include <kernel/multitask.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct elf_header elf_header_t;
typedef struct elf_program_header elf_program_header_t;

struct elf {
  elf_header_t* header;
  elf_program_header_t* program_headers;
};
typedef struct elf elf_t;

/*
 * Reads an ELF executable from a given `buffer` and `size`. Can fail
 * for a variety of reasons, including size mismatch, binary format mismatch,
 * etc. Returns an `elf_t*`. The caller is responsible for calling `elf_free()`
 * on the handle when finished. This function does *not* load the ELF sections
 * into memory.
 */
elf_t* elf_read(void* buffer, uint64_t size);

/*
 * Maps the program sections of an ELF executable into memory for the given
 * `process`. Note that currently, only static loading is implemented.
 */
bool elf_load(process_block_t* proc, elf_t* elf);

/*
 * Free the handle to an ELF executable and all its owned resources. This will
 * internally free the header and program header structures, and unmap the
 * program sections in virtual memory. Do *not* call this function until
 * execution has completed.
 */
void elf_free(elf_t* elf);

#endif  // ELF_H

