#ifndef SYSCALL_CALLBACKS_H
#define SYSCALL_CALLBACKS_H

#include <cpu/isr.h>
#include <dirent.h>
#include <sys/stat.h>

long syscall_exit(int code);
unsigned long syscall_nanosleep(unsigned long ns);

long syscall_close(long fd);
long syscall_execve(const char* pathname,
                    char* const argv[],
                    char* const envp[]);
long syscall_fork(interrupt_frame_t* frame);
long syscall_getdents(unsigned long fd,
                      linux_dirent_t* dirent_buf,
                      unsigned int count);
long syscall_ioctl(long fd, unsigned long request, void* arg);
long syscall_lseek(long fd, long offset, int whence);
long syscall_mkdir(const char* path);
long syscall_open(const char* absolute_path, unsigned int flags);
long syscall_read(long fd, void* buf, long count);
long syscall_rmdir(const char* path);
long syscall_unlink(const char* path);
long syscall_waitpid(long pid, int* wstatus, int options);
long syscall_write(long fd, void* buf, long count);
long syscall_stat(const char* path, stat_t* stat_buf);
long syscall_fstat(long fd, stat_t* stat_buf);

unsigned long syscall_brk(unsigned long brk);

#endif  // SYSCALL_CALLBACKS_H
