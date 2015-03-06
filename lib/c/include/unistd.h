#ifndef __CSCLIBC_UNISTD_H__
#define __CSCLIBC_UNISTD_H__

int symlink(const char *path1, const char *path2);

int link(const char *path1, const char *path2);

int unlink(const char *path);

#endif //__CSCLIBC_UNISTD_H__
