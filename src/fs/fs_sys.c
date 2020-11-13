#include <stdlib.h>
#include <fcntl.h>
#include <sys/errno.h>

#include <fs/vfs.h>
#include <process.h>

#include <cosec/log.h>
#include <cosec/syscall.h>


int sys_mount(mount_info_t *mnt) {
    logmsgdf("%s(*%x)\n", __func__, mnt);
    return ETODO; //vfs_mount(mnt->source, mnt->target, mnt->fstype);
}

int sys_mkdir(const char *pathname, mode_t mode) {
    logmsgdf("%s('%s', 0x%x)\n", __func__, pathname, mode);
    /* TODO : check for absolute path, make abspath if needed */
    return vfs_mkdir(pathname, mode);
}

int sys_lsdir(const char *pathname, struct cosec_dirent *dirs, size_t count) {
    logmsgdf("%s('%s')\n", __func__, pathname);
    return ETODO;
}

int sys_open(const char *pathname, int flags) {
    logmsgdf("%s('%s', 0x%x)\n", __func__, pathname, flags);
    int ret;

    /* validate flags */
    int rw = flags & (O_RDWR | O_RDONLY | O_WRONLY);
    if (!rw) {
        logmsgdf("%s: no O_RDWR | O_RDONLY | O_WRONLY set\n", __func__);
        return -EINVAL;
    }
    if (  ((rw & O_RDWR)   && (rw & ~O_RDWR))
       || ((rw & O_RDONLY) && (rw & ~O_RDONLY))
       || ((rw & O_WRONLY) && (rw & ~O_WRONLY)))
    {
        logmsgdf("%s: only one of O_RDWR | O_RDONLY | O_WRONLY may be set\n", __func__);
        return -EINVAL;
    }

    /* get filesystem info */
    mountnode *sb = NULL;
    inode_t ino = 0;
    inode_t dirino = 0;
    ret = vfs_lookup(pathname, &sb, &ino);
    switch (ret) {
      case 0: break;
      case ENOENT:
        if (flags & O_CREAT) {
            ino = 0;
            break;
        }
        /* fallthrough */
      default:
        return -ret;
    }

    /* get process info */
    pid_t pid = current_pid();
    process *p = current_proc();
    return_err_if(!p, -EKERN, "%s: no current pid", __func__);

    /* TODO: protect with mutex till the end of function */
    int fd = alloc_fd_for_pid(pid); /* does not actually alloc, just gets a free fd */
    return_dbg_if(fd < 0, -EMFILE,
            "%s; pid=%d fds exhausted\n", __func__, pid);
    logmsgdf("%s: fd=%d\n", __func__, fd);

    filedescr *filedes = get_filedescr_for_pid(pid, fd);

    filedes->fd_flags = flags;
    filedes->fd_pos = 0;

    if ((ino == 0) && (flags & O_CREAT)) {
        /* create a regular file */
        ret = vfs_mknod(pathname, S_IFREG | p->ps_umask, 0);
        return_dbg_if(ret, -ret, "%s: vfs_mknod failed(%d)\n", __func__, ret);

        ret = vfs_lookup(pathname, &sb, &ino);
        return_err_if(ret, -EKERN,
                "%s: cannot find ino for created path='%s'\n", __func__, pathname);
    }

    /* update the inode */
    struct inode idata;
    ret = vfs_inode_get(sb, ino, &idata);
    if (ret) return -ret;

    ++ idata.i_nfds;
    vfs_inode_set(sb, ino, &idata);

    device *dev;
    switch (idata.i_mode & S_IFMT) {
      case S_IFCHR:
        dev = device_by_devno(DEV_CHR, inode_devno(&idata));
        if (dev && dev->dev_ops->dev_has_data)
            filedes->fd_pos = -1; /* this device is not seekable */
        break;
      case S_IFSOCK: case S_IFIFO:
        logmsgdf("TODO: opened a socket/pipe\n");
        break;
      default:
        if (rw & (O_RDWR | O_WRONLY)) {
            if (flags & O_TRUNC) {
                vfs_inode_trunc(sb, ino, 0);
            } else if (flags & O_APPEND) {
                filedes->fd_pos = idata.i_size;
            }
        }
    }

    filedes->fd_sb = sb;
    filedes->fd_ino = ino; /* now filedescr is used */
    return fd;
}

int sys_read(int fd, void *buf, size_t count) {
    logmsgdf("%s(%d, *%x, %d)\n", __func__, fd, buf, count);
    int ret;
    size_t nread = 0;

    process *p = current_proc();
    return_err_if(!p, -EKERN, "%s: no current pid", __func__);

    return_dbg_if(!((0 <= fd) && (fd < N_PROCESS_FDS)), -EINVAL,
            "%s(fd=%d): EINVAL\n", fd);

    filedescr *filedes = p->ps_fds + fd;
    return_dbg_if(!filedes, -EBADF,
            "%s(fd=%d): EBADF\n", __func__, fd);
    return_dbg_if(filedes->fd_flags & O_WRONLY, -EBADF,
            "%s(fd=%d): write-only, EBADF\n", __func__, fd);

    /* TODO: check if fd is writable */
    ret = vfs_inode_read(filedes->fd_sb, filedes->fd_ino, filedes->fd_pos,
                buf, count, &nread);
    return_dbg_if(ret, -ret, "%s: inode_read failed(%d)\n", ret);

    if (filedes->fd_pos >= 0) {
        filedes->fd_pos += nread;
    }
    return nread;
}

int sys_write(int fd, const void *buf, size_t count) {
    logmsgdf("%s(%d, *%x, %d)\n", __func__, fd, buf, count);
    int ret;
    size_t nwritten = 0;

    process *p = current_proc();
    return_err_if(!p, -EKERN, "%s: no current pid", __func__);

    return_dbg_if(!((0 <= fd) && (fd < N_PROCESS_FDS)), -EINVAL,
            "%s(fd=%d): EINVAL\n", fd);

    filedescr *filedes = p->ps_fds + fd;
    return_dbg_if(!filedes, -EBADF,
            "%s(fd=%d): EBADF\n", __func__, fd);
    return_dbg_if(filedes->fd_flags & O_RDONLY, -EBADF,
            "%s(fd=%d): O_RDONLY, EBADF\n", __func__, fd);

    ret = vfs_inode_write(filedes->fd_sb, filedes->fd_ino, filedes->fd_pos,
                buf, count, &nwritten);
    return_dbg_if(ret, -ret, "%s: inode_write failed(%d)\n", ret);

    if (filedes->fd_pos >= 0) {
        filedes->fd_pos += nwritten;
    }
    return nwritten;
}

int sys_close(int fd) {
    logmsgdf("%s(%d)\n", __func__, fd);
    int ret;

    pid_t pid = current_pid();

    filedescr *filedes = get_filedescr_for_pid(pid, fd);
    return_dbg_if(!filedes, EBADF, "%s: EBADF fd=%d\n", __func__, fd);

    struct inode idata;
    ret = vfs_inode_get(filedes->fd_sb, filedes->fd_ino, &idata);
    return_dbg_if(ret, ret, "%s: inode_get failed(%d)\n", __func__, ret);

    --idata.i_nfds;
    /* inode may be deleted if i_nfds == 0 and i_nlinks == 0 */
    ret = vfs_inode_set(filedes->fd_sb, filedes->fd_ino, &idata);

    filedes->fd_ino = 0; /* now it's free */
    return 0;
}

/* @returns negative error if error or the new offset */
off_t sys_lseek(int fd, off_t offset, int whence) {
    logmsgdf("%s(%d, %d, %d)\n", __func__, fd, offset, whence);
    int ret;

    filedescr *fildes = get_filedescr_for_pid(current_pid(), fd);
    return_dbg_if(!fildes, -EBADF,
            "%s(fd=%d): EBADF\n", __func__, fd);
    return_dbg_if(0 == fildes->fd_ino, -EBADF,
            "%s(fd=%d): fd_ino=0, EBADF\n", __func__, fd, fildes->fd_ino);
    return_dbg_if(fildes->fd_pos < 0, -ESPIPE,
            "%s(fd=%d): fd_pos < 0, ESPIPE\n", __func__, fd);

    struct inode idata;
    ret = vfs_inode_get(fildes->fd_sb, fildes->fd_ino, &idata);
    return_dbg_if(ret, -ret, "%s: inode_get failed(%d)\n", __func__, ret);

    device *dev;
    switch (idata.i_mode & S_IFMT) {
      case S_IFIFO: case S_IFSOCK:
        return -ESPIPE;
      case S_IFDIR:
        return -EISDIR;
      case S_IFCHR:
        dev = device_by_devno(DEV_CHR, inode_devno(&idata));
        return_dbg_if(!dev, -ENXIO, "%s: ENODEV\n", __func__);
        return_dbg_if(dev->dev_ops->dev_has_data, -ESPIPE,
                    "%s(fd=%d): device is not seekable\n", __func__, fd);
        break;
    }

    switch (whence) {
      case SEEK_CUR:
        fildes->fd_pos += offset;
        ret = fildes->fd_pos;
        break;
      case SEEK_SET:
        fildes->fd_pos = offset;
        ret = fildes->fd_pos;
        break;
      case SEEK_END:
        fildes->fd_pos = idata.i_size - offset;
        break;
      default:
        return -EINVAL;
    }
    ret = fildes->fd_pos;
    if (fildes->fd_pos > idata.i_size)
        fildes->fd_pos = idata.i_size;
    if (fildes->fd_pos < 0)
        fildes->fd_pos = 0;
    return ret;
}

int sys_ftruncate(int fd, off_t length) {
    logmsgdf("%s(%d, %d)\n", __func__, fd, length);
    /* TODO */
    return ETODO;
}

int sys_unlink(const char *path) {
    logmsgdf("%s('%s')\n", __func__, path);
    return vfs_unlink(path);
}

int sys_rename(const char *oldpath, const char *newpath) {
    logmsgdf("%s('%s', '%s')\n", __func__, oldpath, newpath);
    return vfs_rename(oldpath, newpath);
}
