#ifndef __FILESYSTEM_SYSCALLS_H__
#define __FILESYSTEM_SYSCALLS_H__

#include <stdint.h>

#define N_SYSCALLS      0x100

#define SYS_EXIT        0x01
#define SYS_FORK        0x02
#define SYS_READ        0x03
#define SYS_WRITE       0x04
#define SYS_OPEN        0x05
#define SYS_CLOSE       0x06
#define SYS_WAITPID     0x07
#define SYS_LINK        0x09
#define SYS_UNLINK      0x0a
#define SYS_EXECVE      0x0b
#define SYS_CHDIR       0x0c
#define SYS_TIME        0x0d
#define SYS_MKNOD       0x0e
#define SYS_CHMOD       0x0f
#define SYS_STAT        0x12
#define SYS_LSEEK       0x13
#define SYS_GETPID      0x14

#define SYS_MOUNT       0x15
#define SYS_UMOUNT      0x16

#define SYS_KILL        0x25
#define SYS_RENAME      0x26
#define SYS_MKDIR       0x27
#define SYS_RMDIR       0x28

#define SYS_DUP         0x29
#define SYS_PIPE        0x2a

#define SYS_BRK         0x2d

#define SYS_PREAD       0x31
#define SYS_PWRITE      0x33
#define SYS_TRUNC       0x35

#define SYS_PRINT       0xff

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
