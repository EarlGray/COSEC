#ifndef __CSCLIBC_UNISTD_H__
#define __CSCLIBC_UNISTD_H__

#include <sys/types.h>
#include <sys/stat.h>


/* sysconf */
enum sysconf_name {
    _SC_NULL,
    _SC_PAGESIZE,
};

#define _SC_PAGESIZE    _SC_PAGESIZE
#define PAGESIZE        _SC_PAGESIZE
#define PAGE_SIZE       _SC_PAGESIZE

long sysconf(int name);

/* file system */

#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

int stat(const char *pathname, struct stat *statbuf);
int lstat(const char *pathname, struct stat *statbuf);
int fstat(int fd, struct stat *statbuf);


int symlink(const char *path1, const char *path2);

int link(const char *path1, const char *path2);

int unlink(const char *path);

off_t lseek(int fd, off_t offset, int whence);

ssize_t write(int fd, const void *buf, size_t count);

/* processes */
pid_t fork(void);

int execve(const char *pathname, char *const argv[], char *const envp[]);

int execl(const char *pathname, const char *arg, ... /* (char *) NULL */);
int execlp(const char *file, const char *arg, ... /* (char *)NULL */);
int execle(const char *pathname, const char *arg, ... /* (char *)NULL, char *const envp[] */);

int execv(const char *pathname, char *const argv[]);
int execvp(const char *file, char *const argv[]);
int execvpe(const char *file, char *const argv[], char *const envp[]);

void _exit(int status);

void *sbrk(intptr_t increment);

/* process credentials */
pid_t getpid(void);

pid_t getpgid(pid_t pid);
int setpgid(pid_t pid, pid_t pgid);

pid_t setsid(void);

// POSIX version, not BSD:
pid_t getpgrp(void);
int setpgrp(void);

pid_t getsid(void);
pid_t setsid(void);

/* terminal foreground group */
pid_t tcgetpgrp(int fd);
int tcsetpgrp(int fd, pid_t pgrp);

int isatty(int fd);

#endif //__CSCLIBC_UNISTD_H__
