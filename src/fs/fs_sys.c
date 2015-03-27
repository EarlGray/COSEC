#include <fs/fs_sys.h>
#include <fs/vfs.h>
#include <process.h>

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
    const char *funcname = __FUNCTION__;
    int ret;
    size_t nread = 0;

    process *p = current_proc();
    return_err_if(!p, -EKERN, "%s: no current pid", funcname);

    return_dbg_if(!((0 <= fd) && (fd < N_PROCESS_FDS)), -EINVAL,
            "%s(fd=%d): EINVAL\n", fd);

    filedescr *filedes = p->ps_fds + fd;
    return_dbg_if(!filedes, -EBADF,
            "%s(fd=%d): EBADF\n", funcname, fd);

    /* TODO: check if fd is writable */
    ret = vfs_inode_read(filedes->fd_sb, filedes->fd_ino, filedes->fd_pos,
                buf, count, &nread);
    return_dbg_if(ret, -ret, "%s: inode_read failed(%d)\n", ret);

    if (filedes->fd_pos >= 0) {
        filedes->fd_pos += nread;
    }
    return nread;
}

int sys_write(int fd, void *buf, size_t count) {
    const char *funcname = __FUNCTION__;
    int ret;
    size_t nwritten = 0;

    process *p = current_proc();
    return_err_if(!p, -EKERN, "%s: no current pid", funcname);

    return_dbg_if(!((0 <= fd) && (fd < N_PROCESS_FDS)), -EINVAL,
            "%s(fd=%d): EINVAL\n", fd);

    filedescr *filedes = p->ps_fds + fd;
    return_dbg_if(!filedes, -EBADF,
            "%s(fd=%d): EBADF\n", funcname, fd);

    ret = vfs_inode_write(filedes->fd_sb, filedes->fd_ino, filedes->fd_pos,
                buf, count, &nwritten);
    return_dbg_if(ret, -ret, "%s: inode_read failed(%d)\n", ret);

    if (filedes->fd_pos >= 0) {
        filedes->fd_pos += nwritten;
    }
    return nwritten;
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
