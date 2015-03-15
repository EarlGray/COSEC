#include <fs/fs_sys.h>
#include <fs/vfs.h>
#include <log.h>

#warning "TODO: remove #pragma GCC diagnostic ignored -Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-parameter"

int sys_mount(mount_info_t *mnt) {
    return ETODO; //vfs_mount(mnt->source, mnt->target, mnt->fstype);
}

int sys_mkdir(const char *pathname, mode_t mode) {
    return ETODO;
}

int sys_lsdir(const char *pathname, struct cosec_dirent *dirs, size_t count) {
    return ETODO;
}

int sys_open(const char *pathname, int flags) {
    return ETODO;
}

int sys_read(int fd, void *buf, size_t count) {
    return ETODO;
}

int sys_write(int fd, void *buf, size_t count) {
    return ETODO;
}

int sys_close(int fd) {
    return ETODO;
}

off_t sys_lseek(int fd, off_t offset, int whence) {
    return ETODO;
}

int sys_ftruncate(int fd, off_t length) {
    return ETODO;
}
