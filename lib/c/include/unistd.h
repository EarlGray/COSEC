#ifndef __CSCLIBC_UNISTD_H__
#define __CSCLIBC_UNISTD_H__

#define STDIN_FILENO    0
#define STDOUT_FILENO   1
#define STDERR_FILENO   2

int symlink(const char *path1, const char *path2);

int link(const char *path1, const char *path2);

int unlink(const char *path);

off_t lseek(int fd, off_t offset, int whence);

#endif //__CSCLIBC_UNISTD_H__
