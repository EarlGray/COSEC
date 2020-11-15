#ifndef __CSCLIBC_UNISTD_H__
#define __CSCLIBC_UNISTD_H__

#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

#include <sys/types.h>

int symlink(const char *path1, const char *path2);

int link(const char *path1, const char *path2);

int unlink(const char *path);

off_t lseek(int fd, off_t offset, int whence);

ssize_t write(int fd, const void *buf, size_t count);

pid_t getpid(void);

void *sbrk(intptr_t increment);


enum sysconf_name {
    _SC_NULL,
    _SC_PAGESIZE,
};

#define _SC_PAGESIZE    _SC_PAGESIZE
#define PAGESIZE        _SC_PAGESIZE
#define PAGE_SIZE       _SC_PAGESIZE

long sysconf(int name);

#endif //__CSCLIBC_UNISTD_H__
