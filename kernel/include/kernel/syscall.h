#ifndef SYSCALL_H
#define SYSCALL_H

#include <cpu/isr.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SYSCALL_INT_NO 0x80

typedef void (*syscall_callback_t)(interrupt_frame_t* frame);

/*
 * Syscall handler. Uses the following ABI:
 * RAX: Syscall number, return value
 * RDI: Argument 1
 * RSI: Argument 2
 * RDX: Argument 3
 * R10: Argument 4
 * R8:  Argument 5
 * R9:  Argument 6
 *
 * Performs argument validation and dispatches to the correct syscall. Will
 * fail if the syscall number or the arguments are not valid, or if there is
 * not a valid registered handler.
 */
void syscall_handle(interrupt_frame_t* frame);

#endif  // SYSCALL_H

