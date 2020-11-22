#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <sys/errno.h>
#include <sys/dirent.h>
#include <sys/stat.h>

#define __DEBUG
#include <cosec/log.h>

#include "conf.h"
#include "attrs.h"

#include "arch/mboot.h"
#include "arch/multiboot.h"
#include "mem/kheap.h"
#include "dev/screen.h"
#include "fs/vfs.h"
#include "fs/ramfs.h"
#include "fs/devices.h"

static const char *
vfs_match_mountpath(mountnode *parent_mnt, mountnode **match_mnt, const char *path);


/*
 *  Global state
 */

fsdriver * theFileSystems = NULL;

struct superblock *theRootMnt = NULL;

/* should be inode #0 */
struct inode theInvalidInode;


/*
 *  Sysfs
 */
#if 0
#define SYSFS_ID  0x00535953

static int sysfs_read_superblock(struct superblock *sb);
static int sysfs_make_directory(struct superblock *sb, const char *path, mode_t mode);
static int sysfs_get_direntry(struct superblock *sb, void **iter, struct dirent *dir);
static int sysfs_make_inode(struct superblock *sb, mode_t mode, void *info);

fs_ops sysfs_fsops = {
    .read_superblock = sysfs_read_superblock,
    .make_directory = sysfs_make_directory,
    .get_direntry = sysfs_get_direntry,
    .make_inode = sysfs_make_inode,
    .lookup_inode = sysfs_lookup_inode,
    .link_inode = sysfs_link_inode,
    .unlink_inode = sysfs_unlink_inode,
};

struct sysfs_data {
    struct dirent *rootdir,
};

fsdriver_t sysfs_driver = {
    .name = "sysfs",
    .fs_id = SYSFS_ID, /* "SYS" */
    .ops = &sysfs_fsops,
};

struct dirent sysfs_inode_table[] = {
};

static int sysfs_read_superblock(mountnode *sb) {
    sb->dev = gnu_dev_makedev(CHR_VIRT, CHR0_SYSFS);
    sb->data = { .rootdir =
    return 0;
}

static int sysfs_make_directory(mountnode *sb, inode_t *ino, const char *path, mode_t mode) {
    return -ETODO;
}

static int sysfs_get_direntry(mountnode *sb, inode_t ino, void **iter, struct dirent *dir) {
}
#endif




/*
 *  VFS operations
 */

void vfs_register_filesystem(fsdriver *fs) {
    if (!theFileSystems) {
        theFileSystems = fs;
        fs->lst.next = fs->lst.prev = fs;
        return;
    }

    /* insert just before theFileSystems in circular list */
    fsdriver *lastfs = theFileSystems->lst.prev;
    fs->lst.next = theFileSystems;
    fs->lst.prev = lastfs;
    lastfs->lst.next = fs;
    theFileSystems->lst.prev = fs;
};

fsdriver *vfs_filesystem_by_id(uint fs_id) {
    fsdriver *fs = theFileSystems;
    if (!fs) return NULL;

    do {
        if (fs->fs_id == fs_id)
            return fs;
    } while ((fs = fs->lst.next) != theFileSystems);
    return NULL;
}



static const char * vfs_match_mountpath(mountnode *parent_mnt, mountnode **match_mnt, const char *path) {
    mountnode *child_mnt = parent_mnt->sb_children;
    while (child_mnt) {
        const char *mountpath = child_mnt->sb_mntpath;
        size_t mountpath_len = strlen(mountpath);
        if (!strncmp(path, mountpath, mountpath_len)) {
            const char *nextpath = path + mountpath_len;
            if (nextpath[0] == '\0')
                break;

            while (nextpath[0] == FS_SEP)
                ++nextpath;

            if (match_mnt) *match_mnt = child_mnt;
            return nextpath;
        }
        child_mnt = child_mnt->sb_brother;
    }

    if (match_mnt) *match_mnt = parent_mnt;
    return NULL;
}

/*
 *   Determines the mountnode `path` resides on.
 *   Returns:
 *      its pointer through `*mntnode` (if not NULL);
 *      filesystem-specific part of `path` through `*relpath` (if not NULL);
 *
 */
int vfs_mountnode_by_path(const char *path, mountnode **mntnode, const char **relpath) {
    return_log_if(!theRootMnt, EBADF, "vfs_mountnode_by_path(): theRootMnt absent\n");

    return_log_if(!(path && *path), EINVAL, "vfs_mountnode_by_path(NULL or '')\n");
    return_log_if(path[0] != FS_SEP, EINVAL, "vfs_mountnode_by_path('%s'): requires the absolute path\n", path);
    ++path;

    mountnode *mnt = theRootMnt;
    mountnode *chld = NULL;
    while (true) {
        const char * nextpath = vfs_match_mountpath(mnt, &chld, path);
        if (!nextpath)
            break;

        mnt = chld;
        path = nextpath;
    }

    if (mntnode) *mntnode = mnt;
    if (relpath) *relpath = path;
    return 0;
}

int vfs_path_dirname_len(const char *path, size_t pathlen) {
    if (!path) return -1;

    char *last_sep = strnrchr(path, pathlen, FS_SEP);
    if (!last_sep) return 0;

    while (last_sep[-1] == FS_SEP)
        --last_sep;
    return (int)(last_sep - path);
}

int vfs_lookup(const char *path, mountnode **mntnode, inode_t *ino) {
    const char *funcname = __FUNCTION__;
    int ret = 0;
    mountnode *sb = NULL;
    const char *fspath = NULL;

    ret = vfs_mountnode_by_path(path, &sb, &fspath);
    return_dbg_if(ret, ret, "%s: no mountnode for path '%s' (%d)\n", funcname, path, ret);

    return_dbg_if(!sb->sb_fs->ops->lookup_inode, ENOSYS,
            "%s; no %s.lookup_inode\n", funcname, sb->sb_fs->name);

    ret = sb->sb_fs->ops->lookup_inode(sb, ino, fspath, SIZE_MAX);

    if (mntnode) *mntnode = sb;
    return ret;
}


int vfs_mount(dev_t source, const char *target, const mount_opts_t *opts) {
    if (theRootMnt == NULL) {
        if ((target[0] != '/') || (target[1] != '\0')) {
             logmsgef("vfs_mount: mount('%s') with no root", target);
             return -1;
        }

        fsdriver *fs = vfs_filesystem_by_id(opts->fs_id);
        if (!fs) return -2;

        struct superblock *sb = kmalloc(sizeof(struct superblock));
        return_err_if(!sb, -3, "vfs_mount: kmalloc(superblock) failed");

        sb->sb_parent = NULL;
        sb->sb_brother = NULL;
        sb->sb_children = NULL;
        sb->sb_mntpath = "";
        sb->sb_mntpath_hash = 0;
        sb->sb_dev = source;
        sb->sb_fs = fs;

        int ret = fs->ops->read_superblock(sb);
        return_err_if(ret, -4, "vfs_mount: read_superblock failed (%d)", ret);

        theRootMnt = sb;
        return 0;
    }

    logmsge("vfs_mount: TODO: non-root");
    return ETODO;
}


int vfs_mkdir(const char *path, mode_t mode) {
    int ret;
    mountnode *sb;
    const char *localpath;

    mode &= ~S_IFMT;
    mode |= S_IFDIR;

    ret = vfs_mountnode_by_path(path, &sb, &localpath);
    return_err_if(ret, ENOENT, "mkdir: no filesystem for path '%s'\n", path);
    logmsgdf("vfs_mkdir: localpath = '%s'\n", localpath);

    return_log_if(!sb->sb_fs->ops->make_directory, EBADF,
            "mkdir: no %s.make_directory()\n", sb->sb_fs->name);

    ret = sb->sb_fs->ops->make_directory(sb, NULL, localpath, mode);
    return_err_if(ret, ret, "mkdir: failed (%d)\n", ret);

    return 0;
}


int vfs_inode_get(mountnode *sb, inode_t ino, struct inode *idata) {
    const char *funcname = __FUNCTION__;
    int ret;
    return_log_if(!idata, EINVAL, "%s(NULL", funcname);
    return_dbg_if(!sb->sb_fs->ops->inode_get, ENOSYS,
                  "%s: no %s.inode_get()\n", funcname, sb->sb_fs->name);

    ret = sb->sb_fs->ops->inode_get(sb, ino, idata);
    return_dbg_if(ret, ret, "%s: %s.inode_get failed\n", funcname, sb->sb_fs->name);

    return 0;
}

int vfs_inode_set(mountnode *sb, inode_t ino, struct inode *inobuf) {
    const char *funcname = __FUNCTION__;
    int ret;

    return_dbg_if(!sb->sb_fs->ops->inode_get, ENOSYS,
            "%s: no %s.inode_get()\n", funcname, sb->sb_fs->name);
    return_dbg_if(!sb->sb_fs->ops->inode_set, ENOSYS,
            "%s: no %s.inode_set()\n", funcname, sb->sb_fs->name);

    struct inode idata;
    ret = sb->sb_fs->ops->inode_get(sb, ino, &idata);
    return_dbg_if(ret, ret,
            "%s: %s.inode_get() failed(%d)\n", funcname, sb->sb_fs->name, ret);

    /* inode_set can't change type of file */
    if ((idata.i_mode & S_IFMT) != (inobuf->i_mode & S_IFMT)) return EINVAL;
    /* inode index cannot be changed */
    if (idata.i_no != inobuf->i_no) return EINVAL;
    /* only fs code may change i_data */
    if (idata.i_data != inobuf->i_data) return EINVAL;

    return sb->sb_fs->ops->inode_set(sb, ino, &idata);
}


int vfs_mknod(const char *path, mode_t mode, dev_t dev) {
    const char *funcname = __FUNCTION__;
    int ret;
    return_log_if(S_ISDIR(mode), EINVAL, "Error: %s(IFDIR), use vfs_mkdir\n", funcname);
    return_log_if(S_ISLNK(mode), EINVAL, "Error: %s(IFLNK), use vfs_symlink\n", funcname);
    if ((S_IFMT & mode) == 0)
        mode |= S_IFREG;

    mountnode *sb = NULL;
    const char *fspath = NULL;
    inode_t ino = 0, dirino = 0;

    /* get filesystem and fspath */
    ret = vfs_mountnode_by_path(path, &sb, &fspath);
    return_dbg_if(ret, ret, "%s(%s) failed(%d)\n", funcname, path, ret);

    return_dbg_if(!sb->sb_fs->ops->make_inode, ENOSYS,
            "%s: %s does not support .make_inode()\n", funcname, sb->sb_fs->name);
    return_dbg_if(!sb->sb_fs->ops->link_inode, ENOSYS,
            "%s: no %s.link_inode()\n", funcname, sb->sb_fs->name);
    return_dbg_if(!sb->sb_fs->ops->lookup_inode, ENOSYS,
            "%s: no %s.lookup_inode()\n", funcname, sb->sb_fs->name);

    /* get dirino */
    int dirnamelen = vfs_path_dirname_len(fspath, SIZE_MAX);
    ret = sb->sb_fs->ops->lookup_inode(sb, &dirino, fspath, dirnamelen);
    return_dbg_if(ret, ret, "%s: no dirino for %s\n", funcname, fspath);

    struct inode idata;
    vfs_inode_get(sb, dirino, &idata);
    return_dbg_if(!S_ISDIR(idata.i_mode), ENOTDIR,
                 "%s: dirino=%d is not a directory\n", funcname, dirino);

    /* create the inode */
    void *mkinfo = NULL;
    if (S_ISCHR(mode) || S_ISBLK(mode))
        mkinfo = (void *)(size_t)dev;

    ret = sb->sb_fs->ops->make_inode(sb, &ino, mode, mkinfo);
    return_dbg_if(ret, ret,
            "%s(%s): %s.make_inode(mode=0x%x) failed\n", funcname, path, sb->sb_fs->name, mode);

    /* insert the inode */
    const char *de_name = fspath + dirnamelen;
    while (de_name[0] == FS_SEP) ++de_name;
    logmsgdf("%s: inserting ino=%d as '%s'\n", funcname, ino, de_name);

    ret = sb->sb_fs->ops->link_inode(sb, ino, dirino, de_name, SIZE_MAX);
    if (ret) {
        logmsgdf("%s: %s.link_inode() failed(%d)\n", funcname, sb->sb_fs->name, ret);
        if (sb->sb_fs->ops->free_inode)
            sb->sb_fs->ops->free_inode(sb, ino);
        return ret;
    }

    return 0;
}


int vfs_inode_read(
        mountnode *sb, inode_t ino, off_t pos,
        char *buf, size_t buflen, size_t *written)
{
    const char *funcname = "vfs_inode_read";
    int ret;
    return_err_if(!buf, EINVAL, "%s(NULL)", funcname);

    struct inode idata;

    return_dbg_if(!sb->sb_fs->ops->inode_get, ENOSYS,
            "%s: no %s.inode_get\n", funcname, sb->sb_fs->name);
    return_dbg_if(!sb->sb_fs->ops->read_inode, ENOSYS,
            "%s: no %s.read_inode\n", funcname, sb->sb_fs->name);

    ret = sb->sb_fs->ops->inode_get(sb, ino, &idata);
    return_dbg_if(ret, ret,
            "%s: %s.inode_get(%d) failed(%d)\n", funcname, sb->sb_fs->name, ino, ret);

    device *dev;
    switch (idata.i_mode & S_IFMT) {
        case S_IFCHR:
            dev = device_by_devno(DEV_CHR, inode_devno(&idata));
            if (!dev) return ENODEV;
            if (!dev->dev_ops->dev_read_buf) return ENOSYS;
            return dev->dev_ops->dev_read_buf(dev, buf, buflen, written, pos);
        case S_IFBLK:
            dev = device_by_devno(DEV_BLK, inode_devno(&idata));
            if (!dev) return ENODEV;
            return bdev_blocking_read(dev, pos, buf, buflen, written);
        case S_IFSOCK:
            logmsgef("%s(ino=%d -- SOCK): ETODO", funcname, ino);
            return ETODO;
        case S_IFIFO:
            logmsgef("%s(ino=%d -- FIFO): ETODO", funcname, ino);
            return ETODO;
        case S_IFLNK:
            /* TODO: read link path, read its inode */
            return ETODO;
        case S_IFDIR:
            logmsgdf("%s(ino=%d): EISDIR\n", funcname, ino);
            return EISDIR;
        case S_IFREG:
            if (pos >= idata.i_size) {
                if (written) *written = 0;
                return 0;
            }
            return sb->sb_fs->ops->read_inode(sb, ino, pos, buf, buflen, written);
        default:
            logmsgef("%s: unknown mode & S_IFMT = 0x%x", idata.i_mode & S_IFMT);
            return EKERN;
   }
}

int vfs_inode_write(
        mountnode *sb, inode_t ino, off_t pos,
        const char *buf, size_t buflen, size_t *written)
{
    const char *funcname = "vfs_inode_write";
    int ret;
    return_err_if(!buf, EINVAL, "%s(NULL)", funcname);

    struct inode idata;

    return_dbg_if(!sb->sb_fs->ops->inode_get, ENOSYS,
            "%s: no %s.inode_get\n", funcname, sb->sb_fs->name);
    return_dbg_if(!sb->sb_fs->ops->write_inode, ENOSYS,
            "%s: no %s.write_inode\n", funcname, sb->sb_fs->name);

    ret = sb->sb_fs->ops->inode_get(sb, ino, &idata);
    return_dbg_if(ret, ret,
            "%s; %s.inode_get(%d) failed(%d)\n", funcname, sb->sb_fs->name, ino, ret);

    device *dev;
    switch (idata.i_mode & S_IFMT) {
        case S_IFREG:
            return sb->sb_fs->ops->write_inode(sb, ino, pos, buf, buflen, written);
        case S_IFDIR:
            logmsgdf("%s(inode=%d): EISDIR\n", funcname, ino);
            return EISDIR;
        case S_IFBLK:
            return ETODO;
        case S_IFCHR:
            dev = device_by_devno(DEV_CHR, inode_devno(&idata));
            return_dbg_if(!dev, ENODEV, "%s: ENODEV\n");
            return_dbg_if(!dev->dev_ops->dev_write_buf, ENOSYS,
                    "%s: no device.dev_write_buf\n", funcname);
            return dev->dev_ops->dev_write_buf(dev, buf, buflen, written, pos);
        case S_IFLNK: case S_IFSOCK: case S_IFIFO:
            logmsgef("%s(inode.mode=LNK|FIFO|SOCK): ETODO", funcname);
            return ETODO;
        default:
            logmsgef("%s(mode=0x%x)", funcname, idata.i_mode & S_IFMT);
            return EKERN;
    }
}


int vfs_inode_trunc(mountnode *sb, inode_t ino, off_t length) {
    const char *funcname = __FUNCTION__;

    return_dbg_if(!sb->sb_fs->ops->trunc_inode, ENOSYS,
            "%s: no %s.trunc_inode\n", funcname, sb->sb_fs->name);
    return sb->sb_fs->ops->trunc_inode(sb, ino, length);
}

int vfs_inode_stat(mountnode *sb, inode_t ino, struct stat *stat) {
    const char *funcname = __FUNCTION__;
    int ret;
    return_log_if(!stat, EINVAL, "%s(NULL)\n", funcname);

    struct inode idata;

    return_dbg_if(!sb->sb_fs->ops->inode_get, ENOSYS,
             "%s: no %s.inode_get\n", funcname, sb->sb_fs->name);
    ret = sb->sb_fs->ops->inode_get(sb, ino, &idata);
    return_dbg_if(ret, ret,
            "%s: %s.inode_get() failed(%d)\n", funcname, sb->sb_fs->name, ret);

    memset(stat, 0, sizeof(struct stat));
    stat->st_dev = sb->sb_dev;
    stat->st_ino = ino;
    stat->st_mode = idata.i_mode;
    stat->st_nlink = idata.i_nlinks;
    stat->st_rdev = (S_ISCHR(idata.i_mode) || S_ISBLK(idata.i_mode) ?
                        inode_devno(&idata) : 0);
    stat->st_size = idata.i_size;
    return 0;
}

int vfs_stat(const char *path, struct stat *stat) {
    const char *funcname = "vfs_stat";
    int ret;

    mountnode *sb = NULL;
    const char *fspath;
    inode_t ino = 0;

    ret = vfs_mountnode_by_path(path, &sb, &fspath);
    return_dbg_if(ret, ret,
            "%s: no localpath and superblock for path '%s'\n", funcname, path);

    return_dbg_if(!sb->sb_fs->ops->lookup_inode, ENOSYS,
            "%s: no %s.lookup_inode\n", funcname, sb->sb_fs->name);
    ret = sb->sb_fs->ops->lookup_inode(sb, &ino, fspath, SIZE_MAX);
    return_dbg_if(ret, ret,
            "%s: %s.lookup_inode failed(%d)\n", funcname, sb->sb_fs->name, ret);

    return vfs_inode_stat(sb, ino, stat);
}

int vfs_hardlink(const char *path, const char *newpath) {
    const char *funcname = __FUNCTION__;
    int ret;

    mountnode *sb = NULL, *sbn = NULL;
    const char *fspath, *new_fspath;

    ret = vfs_mountnode_by_path(path, &sb, &fspath);
    return_dbg_if(ret, ret, "%s: no mountnode for '%s'\n", funcname, path);

    ret = vfs_mountnode_by_path(newpath, &sbn, &new_fspath);
    return_dbg_if(ret, ret, "%s; no mountnode for '%s'\n", funcname, newpath);

    return_dbg_if(sb != sbn, EXDEV,
            "%s: '%s' and '%s' on different devices\n", funcname, path, newpath);

    return_dbg_if(!sb->sb_fs->ops->lookup_inode, ENOSYS,
            "%s: no %s.lookup_inode", funcname, sb->sb_fs->name);
    return_dbg_if(!sb->sb_fs->ops->link_inode, ENOSYS,
            "%s: no %s.link_inode", funcname, sb->sb_fs->name);

    inode_t ino, dirino;
    ret = sb->sb_fs->ops->lookup_inode(sb, &ino, fspath, SIZE_MAX);
    return_dbg_if(ret, ret,
            "%s: lookup_inode(%s) failed(%d)\n", funcname, fspath, ret);

    int dlen = vfs_path_dirname_len(new_fspath, SIZE_MAX);
    return_dbg_if(dlen < 0, EINVAL, "%s: dlen=%d\n", funcname, dlen);
    size_t dirlen = (size_t)dlen;

    ret = sb->sb_fs->ops->lookup_inode(sb, &dirino, new_fspath, dirlen);
    return_dbg_if(ret, ret,
            "%s: lookup_inode(%s[:%d]) failed(%d)", funcname, new_fspath, dirino, ret);

    const char *basename = new_fspath + dirlen;
    while (basename[0] == FS_SEP) ++basename;

    return sb->sb_fs->ops->link_inode(sb, ino, dirino, basename, SIZE_MAX);
}

int vfs_unlink(const char *path) {
    const char *funcname = "vfs_unlink";
    int ret;

    mountnode *sb = NULL;
    const char *fspath;

    ret = vfs_mountnode_by_path(path, &sb, &fspath);
    return_dbg_if(ret, ret,
            "%s: no mountnode for path '%s'\n", funcname, path);

    return_dbg_if(!sb->sb_fs->ops->unlink_inode, ENOSYS,
            "%s: no %s.unlink_inode\n", funcname, sb->sb_fs->name);

    return sb->sb_fs->ops->unlink_inode(sb, fspath, SIZE_MAX);
}

int vfs_rename(const char *oldpath, const char *newpath) {
    const char *funcname = __FUNCTION__;
    int ret;

    ret = vfs_hardlink(oldpath, newpath);
    return_dbg_if(ret, ret, "%s: link() failed(%d)\n", funcname, ret);

    return vfs_unlink(oldpath);
}


void print_ls(const char *path) {
    int ret;
    mountnode *sb;
    const char *localpath;
    void *iter;
    struct dirent de;
    inode_t ino = 0;

    ret = vfs_mountnode_by_path(path, &sb, &localpath);
    returnv_err_if(ret, "ls: path '%s' not found\n", path, ret);
    logmsgdf("print_ls: localpath = '%s' \n", localpath);

    ret = sb->sb_fs->ops->lookup_inode(sb, &ino, localpath, SIZE_MAX);
    returnv_err_if(ret, "no inode at '%s' (%d)\n", localpath, ret);
    logmsgdf("print_ls: ino = %d\n", ino);

    iter = NULL; /* must be NULL at the start of enumeration */
    do {
        ret = sb->sb_fs->ops->get_direntry(sb, ino, &iter, &de);
        returnv_err_if(ret, "ls error: %s", strerror(ret));

        k_printf("%d\t%s\n", de.d_ino, de.d_name);
    } while (iter);

    /* the end of enumeration */
    k_printf("\n");
}

void print_mount(void) {
    struct superblock *sb = theRootMnt;
    if (!sb) return;

    k_printf("%s on /\n", sb->sb_fs->name);
    if (sb->sb_children) {
        logmsge("TODO: print child mounts");
    }
}

static void build_file_from_string(const char *path, const char *s, size_t size) {
    int ret;
    ret = vfs_mknod(path, 0644, 0);
    returnv_err_if(ret, "mknod %s: %s\n", path, strerror(ret));

    mountnode *sb = NULL;
    inode_t ino = 0;

    ret = vfs_lookup(path, &sb, &ino);
    returnv_err_if(ret, "lookup %s: %s", path, strerror(ret));

    off_t pos = 0;
    size_t nwrit = 0;
    vfs_inode_write(sb, ino, pos, s, size, &nwrit);
    pos += nwrit;
}

/*
 *  Tar files
 */
#define TAR_BLOCKSIZE   512

#define TAR_MAGIC       "ustar "
#define TAR_MAGIC_LEN   6
#define TAR_VERSION     0x2020
#define TAR_VERSION_LEN 2
#define TAR_NAME_LEN    100

/* typeflag */
#define TAR_TYPEFLAG_REG    '0'
#define TAR_TYPEFLAG_REG0   '\0'
#define TAR_TYPEFLAG_LINK   '1'
#define TAR_TYPEFLAG_SYMLNK '2'
#define TAR_TYPEFLAG_CHRDEV '3'
#define TAR_TYPEFLAG_BLKDEV '4'
#define TAR_TYPEFLAG_DIR    '5'
#define TAR_TYPEFLAG_FIFO   '6'

struct tar_header {
    char name[TAR_NAME_LEN];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[TAR_MAGIC_LEN];
    char version[TAR_VERSION_LEN];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
};

static void build_dir_from_tar(const char *dirname, const void *archive, size_t size) {
    char filepath[TAR_NAME_LEN+2];
    logmsgdf("%s('%s', *%x, %d)\n", __func__, dirname, archive, size);

    for (const void *block = archive; block < (archive + size); block += TAR_BLOCKSIZE) {
        const struct tar_header *info = block;
        if (strncmp(info->magic, TAR_MAGIC, TAR_MAGIC_LEN))
            continue;

        size_t filesize;
        sscan_uint(info->size, &filesize, 8);

        snprintf(filepath, sizeof(filepath), "%s%s", dirname, info->name);

        logmsgif("  mode=%s\tsize=%d\t%s", info->mode, filesize, filepath);

        switch (info->typeflag) {
        case TAR_TYPEFLAG_REG:
        case TAR_TYPEFLAG_REG0:
            build_file_from_string(filepath, block+TAR_BLOCKSIZE, filesize);
            break;
        default:
            logmsgf("%s: typeflag is not supported for file %s",
                    __func__, info->typeflag, info->name);
        }
    }
}

#if COSEC_SECD
# include <secd/repl.inc>
#endif

const char *build_date = "COSEC\n(c) Dmytro Sirenko\n"__DATE__", "__TIME__"\n";

static void build_boot_dir() {
    char name[128];
    strncpy(name, "/boot/", 128);

    vfs_mkdir("/boot", 0777);

    count_t n_mods = 0;
    module_t *mods;
    mboot_modules_info(&n_mods, &mods);

    const char *modname;
    size_t i;
    for (i = 0; i < n_mods; ++i) {
        if (mods[i].string) {
            modname = (const char *)mods[i].string;
            if (modname[0] == '/') ++modname;
            strncpy(name + 6, modname, 128);
        } else {
            strncpy(name + 6, "mod", 128);
            name[9] = '0' + (char)i;
            name[10] = '\0';
        }

        const char *modstart = (const char *)mods[i].mod_start;
        size_t size = mods[i].mod_end - mods[i].mod_start;
        k_printf(" module *%x (len=0x%x) :\t%s\n", mods[i].mod_start, size, name);

        size_t namelen = strlen(name);
        if (0 == strcmp(name + namelen - 4, ".tar")) {
            build_dir_from_tar("/", modstart, size);
        } else {
            build_file_from_string(name, modstart, size);
        }
    }
}

void vfs_setup(void) {
    int ret;

    /* register filesystems here */
    vfs_register_filesystem(ramfs_fs_driver());

    /* mount actual filesystems */
    dev_t fsdev = gnu_dev_makedev(CHR_MEMDEV, CHRMEM_MEM);
    mount_opts_t mntopts = { .fs_id = RAMFS_ID };
    ret = vfs_mount(fsdev, "/", &mntopts);
    returnv_err_if(ret, "root mount on sysfs failed (%d)", ret);

    k_printf("mounted %s on /\n", theRootMnt->sb_fs->name);

    /* misc */
    ret = vfs_mkdir("/tmp", 0777);
    returnv_err_if(ret, "mkdir /tmp: %s", strerror(ret));

    build_file_from_string("/BUILD",
        build_date, strlen(build_date));
#if COSEC_SECD
    build_file_from_string("/repl.secd",
        lib_secd_repl_secd, lib_secd_repl_secd_len);
#endif
    build_boot_dir();

    /* create devices */
    ret = vfs_mkdir("/dev", 0755);
    returnv_err_if(ret, "mkdir /dev: %s", strerror(ret));

    char ttyname[] = "/dev/tty0";
    int i;
    for (i = 0; i < N_VCSA_DEVICES; ++i) {
        ttyname[8] = (char)(i + '0');
        ret = vfs_mknod(ttyname, S_IFCHR | 0755, gnu_dev_makedev(CHR_TTY, i));
        if (ret) logmsgef("mkdev c 4:0 /dev/tty0: %s", strerror(ret));
    }
}
