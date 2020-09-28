#ifndef __FILESYSTEM_SYSCALLS_H__
#define __FILESYSTEM_SYSCALLS_H__

#include <stdint.h>

struct mount_info_struct {
    dev_t       source;
    const char *target;
    const char *fstype;
    uint flags;
    void *opts;
};

typedef  struct mount_info_struct  mount_info_t;

int sys_mount(mount_info_t *);
int sys_umount(const char *target);


struct cosec_dirent {
    index_t d_ino;
    off_t   d_off;
    size_t  d_reclen;
    char    d_name[];
};

int sys_mkdir(const char *pathname, mode_t mode);
int sys_lsdir(const char *pathname, struct cosec_dirent *dirs, count_t count);
int sys_rmdir(const char *pathname);

int sys_chdir(const char *path);
char *sys_pwd(char *buf);

int sys_rename(const char *oldpath, const char *newpath);
int sys_link(const char *oldpath, const char *newpath);
int sys_symlink(const char *oldpath, const char *newpath);
int sys_unlink(const char *pathname);

int sys_open(const char *pathname, int flags);
int sys_read(int fd, void *buf, size_t count);
int sys_write(int fd, const void *buf, size_t count);
off_t lseek(int fd, off_t offset, int whence);
int sys_close(int fd);

off_t sys_lseek(int fd, off_t offset, int whence);
int sys_ftruncate(int fd, off_t length);

#endif // __FILESYSTEM_SYSCALLS_H__
