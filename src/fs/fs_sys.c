#include <fs/fs_sys.h>
#include <fs/vfs.h>
#include <log.h>

int sys_mount(mount_info_t *mnt) {
    return vfs_mount(mnt->source, mnt->target, mnt->fstype);
}

int sys_mkdir(const char *pathname, mode_t mode) {
    return -1;
}

int sys_lsdir(const char *pathname, struct cosec_dirent *dirs, size_t count) {
    /** TODO **/
    return -1;
}

int sys_open(const char *pathname, int flags) {
    /** TODO **/
    return -1;
}

int sys_read(int fd, void *buf, size_t count) {
    /** TODO **/
    return -1;
}

int sys_write(int fd, void *buf, size_t count) {
    /** TODO **/
    return -1;
}

int sys_close(int fd) {
    /** TODO **/
    return -1;
}

off_t sys_lseek(int fd, off_t offset, int whence) {
    /** TODO **/
    return -1;
}

int sys_ftruncate(int fd, off_t length) {
    /** TODO **/
    return -1;
}
