#ifndef HOJICHA_MP_FORK_H
#define HOJICHA_MP_FORK_H

#include <cpu/isr.h>
#include <mp/proc.h>

long fork_proc(proc_t* process, interrupt_frame_t* frame);

#endif  // HOJICHA_MP_FORK_H
