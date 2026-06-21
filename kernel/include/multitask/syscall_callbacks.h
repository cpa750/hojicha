#ifndef SYSCALL_CALLBACKS_H
#define SYSCALL_CALLBACKS_H

#include <cpu/isr.h>
#include <dirent.h>
#include <sys/stat.h>

long syscall_exit(int code);
unsigned long syscall_nanosleep(unsigned long ns);

long syscall_close(long fd);
long syscall_dup2(long oldfd, long newfd);
long syscall_execve(const char* pathname,
                    char* const argv[],
                    char* const envp[]);
long syscall_fork(interrupt_frame_t* frame);
long syscall_getcwd(char* buf, unsigned long size);
long syscall_getdents(unsigned long fd,
                      linux_dirent_t* dirent_buf,
                      unsigned int count);
long syscall_ioctl(long fd, unsigned long request, void* arg);
long syscall_link(const char* old_path, const char* new_path);
long syscall_lstat(const char* path, stat_t* stat_buf);
long syscall_lseek(long fd, long offset, int whence);
long syscall_mkdir(const char* path);
long syscall_open(const char* absolute_path, unsigned int flags);
long syscall_read(long fd, void* buf, long count);
long syscall_readlink(const char* path, char* buf, long bufsiz);
long syscall_rmdir(const char* path);
long syscall_symlink(const char* target, const char* link_path);
long syscall_unlink(const char* path);
long syscall_waitpid(long pid, int* wstatus, int options);
long syscall_write(long fd, void* buf, long count);
long syscall_stat(const char* path, stat_t* stat_buf);
long syscall_fstat(long fd, stat_t* stat_buf);
long syscall_chdir(const char* target);
long syscall_fchdir(long target_fd);

unsigned long syscall_brk(unsigned long brk);

#endif  // SYSCALL_CALLBACKS_H
