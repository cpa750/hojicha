#ifndef HOJICHA_MP_EXECVE_H
#define HOJICHA_MP_EXECVE_H

#include <mp/proc.h>
#include <stdint.h>

long execve_proc(proc_t* process,
                 elf_t* elf,
                 char* name,
                 uint64_t name_len,
                 uint64_t argc,
                 char** argv,
                 char** envp);

#endif  // HOJICHA_MP_EXECVE_H
