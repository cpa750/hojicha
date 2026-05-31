#ifndef SYSCALL_CALLBACKS_H
#define SYSCALL_CALLBACKS_H

long syscall_exit(int code);
unsigned long syscall_nanosleep(unsigned long ns);

long syscall_open(const char* absolute_path, unsigned int flags);
long syscall_read(long fd, void* buf, long count);
long syscall_write(long fd, void* buf, long count);

#endif  // SYSCALL_CALLBACKS_H

