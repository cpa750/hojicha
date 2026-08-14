#ifndef HOJICHA_MP_WAIT_H
#define HOJICHA_MP_WAIT_H

#include <mp/proc.h>

long waitpid_proc(proc_t* process, long pid, int* wstatus, int options);

#endif  // HOJICHA_MP_WAIT_H
