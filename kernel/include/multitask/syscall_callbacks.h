#ifndef SYSCALL_CALLBACKS_H
#define SYSCALL_CALLBACKS_H

long syscall_exit(int code);
unsigned long syscall_nanosleep(unsigned long ns);

#endif  // SYSCALL_CALLBACKS_H

